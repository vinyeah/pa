#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "config.h"
#include "dev.h"
#include "pacomm.h"
#include "input_worker.h"
#include "orion_input_worker.h"
#include "sta_sniffer_worker.h"
#include "cJSON.h"
#include "eloop.h"


void
_flush_ngx_log(void *arg)
{
    int i = 0;
    for(i = 0; i < PA_TYPE_MAX; i ++){
        ngx_flush_temp_input(i, FLUSH_MODE_TIME);
    }

    eloop_timer_add_sec(0, 
		NGX_LOG_FLUSH,
        _flush_ngx_log,
        NULL
        );
}

void
_switch_ngx_log(void *arg)
{
    switch_ngx_log(NULL);

    eloop_timer_add_sec(0, 
		NGX_LOG_SWITH,
        _switch_ngx_log,
        NULL
        );
}

int
input_worker_init()
{
    pthread_t input_thread_id = -1;

    ngx_begin_handle();

    blog(LOG_DEBUG, "creating orion input worker thread");
    if(pthread_create(&input_thread_id, NULL, pa_orion_input_worker, NULL) != 0){
        blog(LOG_ERR, "failed to creat thread");
        goto fail;
    }
    pthread_detach(input_thread_id);

    blog(LOG_DEBUG, "creating ngx input worker thread");
    if(pthread_create(&input_thread_id, NULL, sta_sniffer_input_worker, NULL) != 0){
        blog(LOG_ERR, "failed to creat thread");
        goto fail;
    }
    pthread_detach(input_thread_id);

    eloop_timer_add_sec(0, 
		NGX_LOG_SWITH,
        _switch_ngx_log,
        NULL
        );

    eloop_timer_add_sec(0, 
		10,
        _flush_ngx_log,
        NULL
        );

    return 0;
fail:
    return -1;
}

