#define PROBE_TEST
#include "query_power.c"
#include <assert.h>
static unsigned seq[16],count;static int failInfo,failOp=-1,applied;
static int fake(int fd,unsigned long req,...){(void)fd;(void)req;va_list ap;va_start(ap,req);Control *q=va_arg(ap,Control*);va_end(ap);
 if(q->cmd==0x2080a618){seq[count++]=100;assert(q->size==20512);if(failInfo){q->status=0x1f;return 0;}unsigned char *b=(void*)(uintptr_t)q->params;unsigned mask=4,v=applied?225000:175000;memset(b,0,q->size);memcpy(b+4,&mask,4);memcpy(b+0x2e4,&v,4);return 0;}
 assert(q->cmd==0x2080ff72&&q->size==88);GSP_POWER_PROBE_PARAMS *p=(void*)(uintptr_t)q->params;seq[count++]=p->operation;
 assert(p->gpuOffset==0x196bb0&&p->pmgrOffset==0x380690&&p->boardOffset==0x3bc610);
 if((int)p->operation==failOp){p->result=0x1f;p->poisoned=1;return 0;}
 p->stage=100;p->armed=p->operation>0;p->active=p->operation==2||p->operation==4||p->operation==5;p->maxValue=p->feValue=p->upperValue=p->active?225000:175000;p->currentValue=p->operation==4?225000:145000;if(p->operation==2)applied=1;if(p->operation==3)applied=0;return 0;
}
static int run(int m){Control c={0};return run_power(fake,0,0,&c,0x196bb0,0x380690,0x3bc610,m);}
int main(void){
 assert(!run(0)&&count==1&&seq[0]==0);count=0;
 assert(!run(1)&&!applied);unsigned want[]={0,1,2,100,3,100};assert(count==6&&!memcmp(seq,want,sizeof(want)));
 count=0;failInfo=1;assert(run(1)!=0&&!applied&&count==6&&seq[4]==3);failInfo=0;
 for(int i=1;i<=3;i++){count=0;failOp=i;applied=0;assert(run(1)!=0);if(i<3)assert(count==(unsigned)i+1);else assert(count==5&&applied);}
 failOp=-1;count=0;assert(!run(2)&&!applied&&count==2&&seq[0]==3&&seq[1]==100);
 count=0;assert(!run(3)&&applied);unsigned activate[]={0,1,2,4,100};assert(count==5&&!memcmp(seq,activate,sizeof(activate)));
 count=0;assert(!run(4)&&!applied);unsigned release[]={5,3,100};assert(count==3&&!memcmp(seq,release,sizeof(release)));
 count=0;failOp=4;assert(run(3)&&count==4&&applied);failOp=-1;
 count=0;failOp=5;assert(run(4)&&count==1&&applied);failOp=-1;
 puts("PASS: activation stays active; release lowers current before ceilings; uncertain commands stop. ");
 puts("PASS: inspect has no writes; exercise applies/checks/restores; GET failure still restores; uncertain write stops; explicit restore");
}
