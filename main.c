#include "comm.h"
#include "config.h"
#include "ctrl.h"
#include "input_worker.h"
#include "output_worker.h"
#include "pacomm.h"

struct app_config *cfg = NULL;
int terminate = 0;


#ifdef __MINGW32__
long __stdcall filterCallBack(EXCEPTION_POINTERS *excp)
{
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void
safe_exit(int sig) 
{
    printf("-------------%s, %d-----------------\n", __func__, __LINE__);
    terminate = 1;
    exit(0);
}

void
sig_init(){
//    signal(SIGSEGV, safe_exit);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
}

static 
void usage(const char *exename)
{
    printf("Usage: %s \n", exename);
    printf("BHU Build Version:%s\n", BHU_SVN_NO);
}

int
main(int argc, char **argv)
{
    char *confile = NULL;
    int c;
    pthread_t output_thread_id = -1;

    while (-1 != (c = getopt(argc, argv, "vVhc:"))) {
        switch(c) {
        case 'v':
        case 'V':
        case 'h':
            usage(argv[0]);
            exit(1);
            break;
        case 'c':
            confile = optarg;
        default:
            break;
        }
    }

    srand((int)time(NULL));

    if(blog_init()){
        printf("init log failed\n");
        return -1;
    }

    sig_init();

    if((cfg = load_config(confile)) == NULL){
        blog(LOG_ERR, "read config file error");
        goto fail;
    }

    eloop_init(1024);

    if(ctrl_init(cfg)){
        blog(LOG_ERR, "create ctrl listen sock failed");
    }

    if(place_init()){
        goto fail;
    }
    if(dev_init()){
        goto fail;
    }
    if(place_status_init()){
        goto fail;
    }

    if(input_worker_init()){
        goto fail;
    }

    blog(LOG_DEBUG, "creating output worker thread");
    if(pthread_create(&output_thread_id, NULL, pa_output_worker, NULL) != 0){
        blog(LOG_ERR, "failed to creat thread");
        goto fail;
    }
    pthread_detach(output_thread_id);

    eloop_run();
    return 0;
fail:
    return -1;
}

