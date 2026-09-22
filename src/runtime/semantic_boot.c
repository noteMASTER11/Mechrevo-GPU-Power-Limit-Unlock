#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include <unistd.h>
#include "nvtypes.h"
#include "deprecated/gsp_power_probe.h"

#define TOKEN "codex.semantic_tgp=250"
#define MARKER "semantic-tgp-v8r4-20260922"
#define TARGET_MW 250000U
#define GREEN "\033[1;32m"
#define RED "\033[1;31m"
#define RESET "\033[0m"

typedef struct nvmlDevice_st *nvmlDevice_t;
typedef int nvmlReturn_t;
typedef nvmlReturn_t (*nvml_init_fn)(void);
typedef nvmlReturn_t (*nvml_shutdown_fn)(void);
typedef nvmlReturn_t (*nvml_handle_fn)(unsigned int,nvmlDevice_t *);
typedef nvmlReturn_t (*nvml_limit_fn)(nvmlDevice_t,unsigned int *);
typedef nvmlReturn_t (*nvml_constraints_fn)(nvmlDevice_t,unsigned int *,unsigned int *);
typedef nvmlReturn_t (*nvml_name_fn)(nvmlDevice_t,char *,unsigned int);

extern int semantic_transaction_seen(void);
extern int semantic_transaction_result(void);
extern const GSP_POWER_PROBE_PARAMS *semantic_transaction_last(void);

static char recordPath[1024];

static void line(const char *kind,const char *message)
{
    if(kind)fprintf(stdout,"%s%s%s %s\n",!strcmp(kind,"[SUCCESS]")?GREEN:!strcmp(kind,"[FAILED]")?RED:"",kind,
                    !strcmp(kind,"[SUCCESS]")||!strcmp(kind,"[FAILED]")?RESET:"",message);
    else fprintf(stdout,"%s\n",message);
    fflush(stdout);
}

static int read_text(const char *path,char *out,size_t size)
{
    FILE *f=fopen(path,"r");size_t n;
    if(!f)return -1;
    n=fread(out,1,size-1,f);
    if(ferror(f)){fclose(f);return -1;}
    fclose(f);out[n]=0;
    while(n&&isspace((unsigned char)out[n-1]))out[--n]=0;
    return 0;
}

static int token_present(const char *text,const char *token)
{
    size_t n=strlen(token);const char *p=text;
    while(*p){while(isspace((unsigned char)*p))p++;if(!strncmp(p,token,n)&&(!p[n]||isspace((unsigned char)p[n])))return 1;
        while(*p&&!isspace((unsigned char)*p))p++;}
    return 0;
}

static int ac_online(void)
{
    DIR *d=opendir("/sys/class/power_supply");struct dirent *e;char path[512],type[64],online[16];int found=0;
    if(!d)return 0;
    while((e=readdir(d))){if(e->d_name[0]=='.')continue;
        snprintf(path,sizeof(path),"/sys/class/power_supply/%s/type",e->d_name);if(read_text(path,type,sizeof(type)))continue;
        if(strcmp(type,"Mains")&&strcmp(type,"USB")&&strcmp(type,"USB_PD"))continue;
        snprintf(path,sizeof(path),"/sys/class/power_supply/%s/online",e->d_name);
        if(!read_text(path,online,sizeof(online))&&!strcmp(online,"1")){found=1;break;}}
    closedir(d);return found;
}

static int process_running(const char *name)
{
    DIR *d=opendir("/proc");struct dirent *e;char path[512],comm[128];int found=0;
    if(!d)return 0;
    while((e=readdir(d))){if(!isdigit((unsigned char)e->d_name[0]))continue;
        snprintf(path,sizeof(path),"/proc/%s/comm",e->d_name);if(!read_text(path,comm,sizeof(comm))&&!strcmp(comm,name)){found=1;break;}}
    closedir(d);return found;
}

static int ucc_status(char *json,size_t size)
{
    sd_bus *bus=NULL;sd_bus_message *reply=NULL;sd_bus_error error=SD_BUS_ERROR_NULL;const char *value=NULL;int r;
    r=sd_bus_open_system(&bus);if(r<0)goto done;
    r=sd_bus_call_method(bus,"com.uniwill.uccd","/com/uniwill/uccd","com.uniwill.uccd",
                         "GetGPUPowerStatusJSON",&error,&reply,NULL);if(r<0)goto done;
    r=sd_bus_message_read(reply,"s",&value);if(r<0||!value){r=-1;goto done;}
    if(strlen(value)>=size){r=-1;goto done;}strcpy(json,value);
    r=strstr(json,"\"enabled\":true")&&strstr(json,"\"requestedW\":null")?0:-1;
done:
    sd_bus_error_free(&error);sd_bus_message_unref(reply);sd_bus_unref(bus);return r;
}

static int wait_for_marker(char *marker,size_t size)
{
    unsigned waited;
    for(waited=0;waited<120;waited++){
        if(!read_text("/sys/module/nvidia/parameters/GspReadProbeBuild",marker,size))
            return strcmp(marker,MARKER)?-1:0;
        sleep(1);
    }
    return -1;
}

static int wait_for_ucc(char *json,size_t size)
{
    unsigned waited;
    for(waited=0;waited<60;waited++){
        if(!ucc_status(json,size))return 0;
        sleep(1);
    }
    return -1;
}

static int make_record(const char *ucc)
{
    char boot[80],dir[768],tmp[4096];FILE *f;int n;
    if(read_text("/proc/sys/kernel/random/boot_id",boot,sizeof(boot)))return -1;
    if(mkdir("/var/lib/mechrevo-semantic-tgp",0700)&&errno!=EEXIST)return -1;
    snprintf(dir,sizeof(dir),"/var/lib/mechrevo-semantic-tgp/%s",boot);
    if(mkdir(dir,0700)&&errno!=EEXIST)return -1;
    n=snprintf(recordPath,sizeof(recordPath),"%s/activation.json",dir);if(n<0||(size_t)n>=sizeof(recordPath))return -1;
    f=fopen(recordPath,"wx");if(!f)return -1;
    n=snprintf(tmp,sizeof(tmp),"{\n  \"boot_id\": \"%s\",\n  \"target_mw\": %u,\n  \"status\": \"started\",\n  \"ucc\": %s,\n  \"actual_draw_verified\": false\n}\n",boot,TARGET_MW,ucc);
    if(n<0||(size_t)n>=sizeof(tmp)){fclose(f);return -1;}
    fputs(tmp,f);fclose(f);chmod(recordPath,0600);return 0;
}

static void finish_record(const char *status,const char *error,unsigned current,unsigned maximum)
{
    FILE *f;if(!recordPath[0])return;f=fopen(recordPath,"w");if(!f)return;
    fprintf(f,"{\n  \"target_mw\": %u,\n  \"status\": \"%s\",\n",TARGET_MW,status);
    if(error)fprintf(f,"  \"error\": \"%s\",\n",error);
    fprintf(f,"  \"current_limit_mw\": %u,\n  \"maximum_limit_mw\": %u,\n  \"actual_draw_verified\": false\n}\n",current,maximum);
    fclose(f);chmod(recordPath,0600);
}

static int run_nvml(unsigned *current,unsigned *maximum,char *name,size_t nameSize)
{
    void *lib=dlopen("libnvidia-ml.so.1",RTLD_NOW|RTLD_GLOBAL);nvmlDevice_t device=NULL;unsigned minimum=0,usage=0;
    nvml_init_fn init;nvml_shutdown_fn shutdown;nvml_handle_fn handle;nvml_limit_fn limit,power;
    nvml_constraints_fn constraints;nvml_name_fn getName;int r=-1;
    if(!lib)return -1;
#define SYM(dst,value) do{*(void **)(&(dst))=dlsym(lib,(value));if(!(dst))goto done;}while(0)
    SYM(init,"nvmlInit_v2");SYM(shutdown,"nvmlShutdown");SYM(handle,"nvmlDeviceGetHandleByIndex_v2");
    SYM(limit,"nvmlDeviceGetPowerManagementLimit");SYM(constraints,"nvmlDeviceGetPowerManagementLimitConstraints");
    SYM(power,"nvmlDeviceGetPowerUsage");SYM(getName,"nvmlDeviceGetName");
    if(init()!=0)goto done;
    if(handle(0,&device)!=0)goto shutdown_done;
    (void)getName(device,name,(unsigned)nameSize);
    /* These calls cause NVML to request the PMGR policy table. The linked
     * ioctl interposer performs the fixed semantic transaction exactly once. */
    (void)constraints(device,&minimum,maximum);(void)limit(device,current);(void)power(device,&usage);
    if(!semantic_transaction_seen()||semantic_transaction_result())goto shutdown_done;
    if(constraints(device,&minimum,maximum)!=0||limit(device,current)!=0)goto shutdown_done;
    r=0;
shutdown_done:shutdown();
done:dlclose(lib);return r;
#undef SYM
}

int main(void)
{
    char cmdline[4096],marker[128],ucc[1024],name[128]="NVIDIA GPU",message[512];unsigned current=0,maximum=0;
    sigset_t set;
    if(geteuid()!=0){line("[FAILED]","root required");return 1;}
    if(read_text("/proc/cmdline",cmdline,sizeof(cmdline))||!token_present(cmdline,TOKEN)){line("[FAILED]","dedicated boot token absent");return 1;}
    line("[....]","Waiting for the NVIDIA module and UCC Max TGP ownership state");
    if(wait_for_marker(marker,sizeof(marker))){line("[FAILED]","wrong or unavailable NVIDIA module marker");return 1;}
    if(!ac_online()){line("[FAILED]","AC power required");return 1;}
    if(process_running("nvidia-powerd")){line("[FAILED]","nvidia-powerd still owns Dynamic Boost");return 1;}
    if(wait_for_ucc(ucc,sizeof(ucc))){line("[FAILED]","UCC Max TGP is not yielding GPU ownership");return 1;}
    line("[SUCCESS]","UCC Max TGP is active; platform and cooling control remain available");
    if(make_record(ucc)){line("[FAILED]","cannot create one-attempt journal");return 1;}
    sigemptyset(&set);sigaddset(&set,SIGINT);sigaddset(&set,SIGTERM);sigaddset(&set,SIGHUP);sigprocmask(SIG_BLOCK,&set,NULL);
    line("[....]","Resolving the live GSP policy arena with the verified 4 KiB transport");
    setenv("CODEX_GSP_POWER_MODE","activate",1);setenv("CODEX_GSP_TARGET_MW","250000",1);
    if(run_nvml(&current,&maximum,name,sizeof(name))){finish_record("failed","semantic transaction failed",current,maximum);line("[FAILED]","semantic transaction failed");return 1;}
    if(current!=TARGET_MW||maximum!=TARGET_MW){finish_record("failed","NVML readback mismatch",current,maximum);line("[FAILED]","NVML readback mismatch");return 1;}
    if(!semantic_transaction_last()->active||semantic_transaction_last()->writes!=3){finish_record("failed","driver state mismatch",current,maximum);line("[FAILED]","driver state mismatch");return 1;}
    snprintf(message,sizeof(message),"%s exposes a verified %u W limit",name,current/1000);line("[SUCCESS]",message);
    finish_record("verified_250w",NULL,current,maximum);line("[SUCCESS]","Semantic TGP transaction completed; Dynamic Boost source is disabled");
    line("[....]","Holding the success screen for 5 seconds");sleep(5);return 0;
}
