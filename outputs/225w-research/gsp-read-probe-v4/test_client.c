#define PROBE_TEST
#include "query_pages.c"
#include <assert.h>
static unsigned requests, fail_at;
static int fake_ioctl(int fd,unsigned long req,...) {
 (void)fd;(void)req;va_list ap;va_start(ap,req);Control *c=va_arg(ap,Control*);va_end(ap);
 GSP_READ_PROBE_PARAMS *p=(void*)(uintptr_t)c->params;
 assert(c->cmd==GSP_READ_PROBE_CMD&&p->version==4&&c->size==4200);
 p->wprStart=GSP_READ_PROBE_WPR_START;p->wprEnd=GSP_READ_PROBE_WPR_END;
 if(!p->operation){p->stage=1;return 0;}
 requests++;assert(p->pageOffset==(requests-1)*4096ULL);
 if(requests==fail_at){p->stage=2;p->rpcStatus=0x1f;p->retainedBuffer=1;return 0;}
 p->stage=3;p->bytesRead=4096;p->source=GSP_READ_PROBE_SOURCE+p->pageOffset;
 memset(p->data,(unsigned char)requests,4096);return 0;
}
int main(void){
 Control info={0};FILE *out=tmpfile();assert(out);
 assert(probe_pages(fake_ioctl,0,0,&info,0,0,NULL)==0&&requests==0);
 assert(probe_pages(fake_ioctl,0,0,&info,0,3,out)==0&&requests==3&&ftell(out)==12288);
 rewind(out);assert(fgetc(out)==1);fseek(out,4096,SEEK_SET);assert(fgetc(out)==2);fclose(out);
 requests=0;fail_at=2;out=tmpfile();assert(out);
 assert(probe_pages(fake_ioctl,0,0,&info,0,3,out)!=0&&requests==2&&ftell(out)==4096);fclose(out);
 requests=0;assert(probe_pages(fake_ioctl,0,0,&info,1,1,NULL)!=0&&requests==0);
 puts("PASS: client metadata/no DMA, page order, bounded output, stop on first RPC failure");
}
