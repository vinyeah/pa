#include <sys/stat.h>
#include <pthread.h>
#include <sys/types.h>
#include "comm.h"
#include "pacomm.h"
#include "config.h"

extern int terminate;
extern struct app_config *cfg;
struct handle_param handle_param_array[PA_TYPE_MAX] = {0};
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

#define TMP_HANDLE_DIR "/tmp/.pa_temp_handle"
#define READED_FLAG_FILE    "./.readed"


struct ngx_log_param {
    char *name;
    FILE *f;
    char readed[64];
};


struct ngx_log_param ngx_log_param_array[NGX_LOG_MAX] = {
    {
        .name = "sta_sniffer.access.log",
        .f = NULL,
        .readed = {0},
    },
    {
        .name = "other.access.log",
        .f = NULL,
        .readed = {0},
    },
};


struct audit_type audit_type_array[] = {
    [PA_TYPE_WLRZ] = { .type = "001", .name = "WLRZ" },
    [PA_TYPE_FJFJ] = { .type = "002", .name = "FJGJ" },
    [PA_TYPE_JSTX] = { .type = "003", .name = "JSTX" },
    [PA_TYPE_XWRZ] = { .type = "004", .name = "XWRZ" },
    [PA_TYPE_SJRZ] = { .type = "005", .name = "SJRZ" },
    [PA_TYPE_PTNR] = { .type = "006", .name = "PTNR" },
    [PA_TYPE_SGJZ] = { .type = "007", .name = "SGJZ" },
    [PA_TYPE_CSZL] = { .type = "008", .name = "CSZL" },
    [PA_TYPE_CSZT] = { .type = "009", .name = "CSZT" },
    [PA_TYPE_SBZL] = { .type = "010", .name = "SBZL" },
    [PA_TYPE_JSJZT] = { .type = "011", .name = "JSJZT" },
    [PA_TYPE_SBGJ] = { .type = "012", .name = "SBGJ" },
    [PA_TYPE_RZSJ] = { .type = "013", .name = "RZSJ" },
    [PA_TYPE_SJTZ] = { .type = "014", .name = "SJTZ" },
    [PA_TYPE_PNFJ] = { .type = "999", .name = "PNFJ" },
};

int
get_audit_id_by_type(char *type)
{
    int i = 0;
    for(i = 0; i < sizeof(audit_type_array)/sizeof(audit_type_array[0]); i ++){
        if(strcmp(audit_type_array[i].type, type) == 0)
            return i;
    }
    return -1;
}



int
putfile_to_output(char *src, int type, char *src_id)
{
    char dbuf[512] = {0}; 
    char buf[1024] = {0};
    char tbuf[32] = {0};
    char uuid[38] = {0};
    char *typestr = audit_type_array[type].type;

    struct tm * tm;
    time_t tt = time(NULL);
    time_t dt = (tt/PA_INTERVAL)*PA_INTERVAL; 

    blog(LOG_DEBUG, "adding file to output path");


    printf("[%s][%s][%s]\n", src, typestr, src_id);

    sprintf(dbuf, "%s/"OUTPUT_DIR"/%s/", cfg->data_path, typestr);
    tm = localtime(&tt); 
    strftime(tbuf, sizeof(tbuf) - 1, "%Y%m%d%H%M%S", tm);
#if 0
    tm = localtime(&dt); 
    if(tm){
        strftime(dbuf + len, sizeof(dbuf) - 1 - len, "%Y%m%d/%H/%M", tm);
    }
    blog(LOG_DEBUG, "path:%s", dbuf);
    sprintf(buf, "mkdir -p %s", dbuf);
    call_system(buf);

    tm = localtime(&tt); 
    strftime(tbuf, sizeof(tbuf) - 1, "%Y%m%d%H%M%S", tm);
    sprintf(fbuf, "%s%03d_"GATHER_TYPE"_%s_%s_%s.log", tbuf,
                                1+(int) (999.0*rand()/(RAND_MAX+1.0)),
                                cfg->src_id,
                                cfg->org_code,
                                type);

    sprintf(buf, "%s/%s.ok", dbuf, fbuf);
    unlink(buf);

    sprintf(buf, "cp %s %s/%s", src, dbuf, fbuf);
    call_system(buf);
#endif
    sprintf(buf, "mkdir -p %s", dbuf);
    call_system(buf);

    random_uuid(uuid);
    sprintf(buf, "cp %s %s/%s_%s_%s", src, dbuf, src_id, tbuf, uuid);
    //sprintf(buf, "zip %s/%s_%s_%s %s", dbuf, src_id, tbuf, uuid, src);
    call_system(buf);

    return 0;
}

cJSON *
get_json_map(struct json_map *map, char *area)
{
    int i = 0;
    blog(LOG_DEBUG, "finding json for [%s]", area);
    if(!area || strlen(area) < 6)
        return NULL;
    for(i = 0; i < JSON_MAP_SIZE; i ++){
        if(!map[i].area[0]){
            strcpy(map[i].area, area);
            map[i].area[4] = '0';
            map[i].area[5] = '0';
            map[i].json = cJSON_CreateArray();
            return map[i].json;
        } else if(memcmp(map[i].area, area, 4) == 0){
            return map[i].json;
        }
    }
    return NULL; 
}


int
ngx_flush_temp_input(int type, int mode)
{
    int ret = -1;
    char buf[512] = {0};
    struct handle_param *hparam = NULL;
    hparam = &handle_param_array[type];
    pthread_mutex_lock(&lock);
    if(mode == FLUSH_MODE_AUTO){
        if(hparam->count == 0)
            hparam->start = time(NULL);
        if(++hparam->count >= BATCH_NUM)
            goto do_flush;
    } if(mode == FLUSH_MODE_TIME){
        if(hparam->count && (time(NULL) - hparam->start) > BATCH_TIME){
            goto do_flush;
        }
    } else if(mode == FLUSH_MODE_FORCE && hparam->count){
        goto do_flush;
    }
    pthread_mutex_unlock(&lock);
    return 0;
do_flush:
    sprintf(buf, "%s/%s", TMP_HANDLE_DIR, audit_type_array[type].type);
    if(append_buf_to_file(buf, "]", NULL)){
        blog(LOG_ERR, "append ] to file failed");
        goto out;
    }
    putfile_to_output(buf, type, cfg->src_id);
    hparam->count = 0;
    sprintf(buf, "rm -f %s/%s", TMP_HANDLE_DIR, audit_type_array[type].type);
    call_system(buf);
out:
    pthread_mutex_unlock(&lock);
    return ret;
}


int
ngx_write_temp_handle_file(int type, char *out)
{
    char buf[512] = {0};
    int ret = 0;
    sprintf(buf, "%s/%s", TMP_HANDLE_DIR, audit_type_array[type].type);
    if(!handle_param_array[type].count){
        buf_to_file(buf, "[", 1);
        ret = append_buf_to_file(buf, out, NULL);
    } else {
        ret = append_buf_to_file(buf, out, ",");
    }
    if(ret){
        blog(LOG_ERR, "write data to temp file error");
        return -1;
    }
    return ngx_flush_temp_input(type, FLUSH_MODE_AUTO);
}

static int
ngx_update_readed_flag(int log_type, char *s)
{
    FILE *f = NULL;
    char buf[256] = {0};
    sprintf(buf, "%s/%s.readed", TMP_HANDLE_DIR, ngx_log_param_array[log_type].name);
    strcpy(ngx_log_param_array[log_type].readed, s);
    if((f = fopen(buf, "w"))){
        fprintf(f, "%s", ngx_log_param_array[log_type].readed);
        fclose(f);
    }
    return 0;
}

int 
ngx_begin_handle()
{
    FILE *f = NULL;
    char buf[1024*10] = {0};
    int i = 0;
    int j = 0;
    memset(handle_param_array, 0, sizeof(handle_param_array));
    call_system("mkdir -p "TMP_HANDLE_DIR);
    for(i = 0; i < PA_TYPE_MAX; i ++){
        sprintf(buf, "%s/%s", TMP_HANDLE_DIR, audit_type_array[PA_TYPE_SJRZ].type);
        if((f = fopen(buf, "r"))){
            while(fgets(buf, sizeof(buf) - 1, f)){
                buf[sizeof(buf) - 1] = 0;
                for(j = 0; buf[j]; j ++)
                    if(buf[j] == '}')
                        handle_param_array[i].count ++;
            }
            fclose(f);
        }
        handle_param_array[i].start = time(NULL);
    }

    for(i = 0; i < NGX_LOG_MAX; i ++){
        sprintf(buf, "%s/%s.readed", TMP_HANDLE_DIR, ngx_log_param_array[i].name);
        if((f = fopen(buf, "r"))){
            fread(ngx_log_param_array[i].readed, 1, sizeof(ngx_log_param_array[i].readed) - 1, f);
            fclose(f);
            for(j = strlen(ngx_log_param_array[i].readed) - 1; i > 0; i --)
                if(ngx_log_param_array[i].readed[j] == '\r' || ngx_log_param_array[i].readed[j] == '\n')
                    ngx_log_param_array[i].readed[i] = 0;
        }
    }
    return 0;
}

void
switch_ngx_log(void *arg)
{
    char buf[512] = {0};
    int i = 0;
    blog(LOG_DEBUG, "told nginx to swith log file");
    for(i = 0; i < NGX_LOG_MAX; i++){
        sprintf(buf, "cd %s && mv %s ./bak/%s.%ld && kill -USR1 `cat nginx.pid`", cfg->ngx_log_path, ngx_log_param_array[i].name, ngx_log_param_array[i].name, time(NULL));
        call_system(buf);
    }

}

int 
ngx_end_handle()
{
    int i = 0;
    for(i = 0; i < PA_TYPE_MAX; i ++){
        ngx_flush_temp_input(i, FLUSH_MODE_FORCE);
    }
    return 0;
}

void
ngx_input_worker(int ngx_log_type, void *(*line_callback)(char *line))
{
    char buf[1024*1024] = {0};
    char buf2[1024*1024] = {0};
    int i, j;
    char *p = NULL;

    blog(LOG_DEBUG, "reading input file from nginx:%s", buf);

    while(!terminate){
        if(!ngx_log_param_array[ngx_log_type].f){
            sprintf(buf, "%s/%s", cfg->ngx_log_path, ngx_log_param_array[ngx_log_type].name);
            ngx_log_param_array[ngx_log_type].f = fopen(buf, "r");
        }
        if(ngx_log_param_array[ngx_log_type].f){
            while(fgets(buf, sizeof(buf) - 1, ngx_log_param_array[ngx_log_type].f)){
                buf[sizeof(buf) - 1] = 0;
                blog(LOG_DEBUG, "get line from file : %s", buf);
                if(!(p = strchr(buf, '|')))
                    continue;
                *p = 0;
                ngx_update_readed_flag(ngx_log_type, buf);
                for(i = p + 1 - buf,j = 0; buf[i];){
                    if(buf[i] == '\\' && memcmp(buf + i, "\\x22", 4) == 0){
                        buf2[j++] = '"';
                        i += 4;
                    } else {
                        buf2[j++] = buf[i++];
                    }
                }
                while(buf2[j] != ']')j--;
                buf2[j + 1] = 0;
                line_callback(buf2);
            }
        }
        sleep(1);
    }
}

