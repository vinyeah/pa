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
#include "cJSON.h"

extern int terminate;
extern struct app_config *cfg;

#define TMP_HANDLE_DIR "/tmp/.pa_temp_handle"
#define READED_FLAG_FILE    "./.readed"

struct handle_param {
    int sta_count;
    time_t start;
};


static char readed_flag[64] = {0};

static int
update_readed_flag(char *s)
{
    FILE *f = NULL;
    strcpy(readed_flag, s);
    if((f = fopen(READED_FLAG_FILE, "w"))){
        fprintf(f, "%s", readed_flag);
        fclose(f);
    }
    return 0;
}

static int 
begin_handle(struct handle_param *hparam)
{
    FILE *f = NULL;
    char buf[1024*10] = {0};
    int i = 0;
    memset(hparam, 0, sizeof(*hparam));
    call_system("mkdir -p "TMP_HANDLE_DIR);
    sprintf(buf, "%s/%s", TMP_HANDLE_DIR, audit_type_array[PA_TYPE_SJRZ].type);
    if((f = fopen(buf, "r"))){
        while(fgets(buf, sizeof(buf), f)){
            hparam->sta_count ++;
        }
        fclose(f);
    }
    hparam->start = time(NULL);

    memset(readed_flag, 0, sizeof(readed_flag));
    if((f = fopen(READED_FLAG_FILE, "r"))){
        fread(readed_flag, 1, sizeof(readed_flag) - 1, f);
        fclose(f);
        for(i = strlen(readed_flag) - 1; i > 0; i --)
            if(readed_flag[i] == '\r' || readed_flag[i] == '\n')
                readed_flag[i] = 0;
    }
    return 0;
}


static int 
end_handle(struct handle_param *hparam)
{
    flush_temp_input(&hparam, NULL, FLUSH_MODE_FORCE);
    return 0;
}

static int
flush_temp_input(struct handle_param *hparam, char *type, int mode)
{
    int ret = 0;
    char buf[512] = {0};
    if(mode == FLUSH_MODE_AUTO){
        if(hparam->sta_count == 0)
            hparam->start = time(NULL);
        if(++hparam->sta_count >= BATCH_NUM)
            goto do_flush;
    } if(mode == FLUSH_MODE_TIME){
        if(hparam->sta_count && (time(NULL) - hparam->start) > BATCH_TIME){
            goto do_flush;
        }
    } else if(mode == FLUSH_MODE_FORCE){
        goto do_flush;
    }
    return 0;
do_flush:
    if(!type)
        type = audit_type_array[PA_TYPE_SJRZ].type;
    sprintf(buf, "%s/%s", TMP_HANDLE_DIR, type);
    if(!(ret = putfile_to_output(buf, PA_TYPE_SJRZ, cfg->src_id))){
        hparam->sta_count = 0;
        sprintf(buf, "rm -f %s/%s", TMP_HANDLE_DIR, type);
        call_system(buf);
    }
    return ret;
}


static int
write_temp_handle_file(char *type, char *out, struct handle_param *hparam)
{
    char buf[512] = {0};
    sprintf(buf, "%s/%s", TMP_HANDLE_DIR, type);
    if(append_buf_to_file(buf, out)){
        blog(LOG_ERR, "write data to temp file error");
        return -1;
    }
    return flush_temp_input(hparam, type, FLUSH_MODE_AUTO);
}

//http post data format: 
//version: 2bytes
//data_type: 3bytes
//dev_mac:12bytes
//json data.
static int
handle_input_line(char *buf, struct handle_param *hparam)
{
    char mac[6] = {0};
    char *p = NULL;
    long flen = 0;
    char ver[3] = {0};
    char apmacstr[13] = {0};
    char stamac[6] = {0};
    char stamacstr[13] = {0};
    char type[4] = {0};
    cJSON *json = NULL;
    cJSON *item = NULL;
    cJSON *obj = NULL;
    int size = 0;
    char *out = NULL;
    int i = 0;
    struct mac_node *mn = NULL;
    time_t online;
    time_t offline;

    blog(LOG_ERR, "handle line:%s", buf);
    if(!(p = strchr(buf, ' '))){
        blog(LOG_ERR, "can't find time");
        return -1;
    }
    *p = 0;

    if(strcmp(buf, readed_flag) <= 0){
        blog(LOG_DEBUG, "old data:[%s] readed:[%s]", buf, readed_flag);
        return -1;
    }

    update_readed_flag(buf);

    if(!(p = strchr(p + 1, '|'))){
        blog(LOG_ERR, "can't find delim '|'");
        return -1;
    }
    memcpy(ver, p + 1, 2);
    memcpy(type, p + 1 + 2, 3);
    memcpy(apmacstr, p + 1 + 2 + 3, 12);
    p += 1 + 2 + 3 + 12;
    if(*p != '[' || *(p + 1) != '{'){
        blog(LOG_ERR, "can't find delim '{'");
        return -1;
    }
    blog(LOG_DEBUG, "json:%s", p);
    if(!(json = cJSON_Parse(p))){
        blog(LOG_ERR, "parse json error");
        return -1;
    }

    blog(LOG_DEBUG, "parse json succeed");

    memset(mac, 0, sizeof(mac));
    tc_str2mac(apmacstr, mac);
    if(!(mn = dev_find(mac))){
        blog(LOG_ERR, "no such dev data:%s", apmacstr);
        goto fail;
    }

    blog(LOG_DEBUG, "got dev by mac");

    size = cJSON_GetArraySize(json);
    blog(LOG_DEBUG, "record items:%d", size);
    for(i = 0; i < size; i++){
        if(!(item = cJSON_GetArrayItem(json, i))){
            blog(LOG_ERR, "get item %d fail", i);
            continue;
        }
        if(!(obj = cJSON_GetObjectItem(item, "NET_ENDING_MAC"))){
            blog(LOG_ERR, "can't find MAC");
            continue;
        }
        blog(LOG_DEBUG, "mac:%s", obj->valuestring);
        tc_str2mac(obj->valuestring, stamac);
        tc_mac2str(stamac, stamacstr, 0);
        if(!(obj = cJSON_GetObjectItem(item, "ONLINE_TIME"))){
            blog(LOG_ERR, "can't find online time");
            continue;
        }
        blog(LOG_DEBUG, "online:%d", obj->valueint);
        online = obj->valueint;
        offline = online;

        blog(LOG_DEBUG, "appending attribute");

        if((obj = cJSON_GetObjectItem(item, "OFFLINE_TIME"))){
            offline = obj->valueint;
        }
        cJSON_AddStringToObject(item, "SERVICE_CODE", mn->service_code);
        cJSON_AddStringToObject(item, "COMPANY_ID", cfg->org_code);
        sprintf(buf, "%s%s%ld", mn->service_code, stamacstr, offline-online);
        cJSON_AddStringToObject(item, "SESSION_ID", buf);
        cJSON_AddStringToObject(item, "AP_NUM", mn->ap_num);
        cJSON_AddStringToObject(item, "AP_MAC", mn->macstr_dash);
        if(!(out = cJSON_PrintUnformatted(item))){
            blog(LOG_ERR, "print json error");
            continue;
        }
        blog(LOG_DEBUG, "save to temp file");
        write_temp_handle_file(type, out, hparam);
    }
    blog(LOG_DEBUG, "handle record finished");
    return 0;
fail:
    if(json)
        cJSON_Delete(json);
    return -1;
}


//http post data format: 
//version: 2bytes
//data_type: 3bytes
//dev_mac:12bytes
//json data.
static int
handle_input_file(char *fname, struct handle_param *hparam)
{
    FILE *f = NULL;
    char buf[1024*1024] = {0};
    char *p = NULL;

    sprintf(buf, "%s/%s", cfg->ngx_log_path, fname);
    if(!(f = fopen(buf, "r"))){
        blog(LOG_ERR, "open file failed:%s", buf);
        return -1;
    }
    while(!feof(f)){
        if(!(p = fgets(buf, sizeof(buf) - 1, f))){
            if(!feof(f)){
                blog(LOG_ERR, "read file error");
                break;
            }
            break;
        }
        blog(LOG_DEBUG, "get line from file : %s", p);
        handle_input_line(buf, hparam);
    }
    fclose(f);
    return 0;
}

#if 0
void *
pa_input_worker(void *arg)
{
    DIR *dp;
    char buf[512] = {0};
    struct dirent *entry;
    struct stat statbuf;
    struct handle_param hparam;

    blog(LOG_DEBUG, "reading input file from nginx:%s", cfg->ngx_log_path);
    begin_handle(&hparam);
    while(!terminate){
        if((dp = opendir(cfg->ngx_log_path)) == NULL) {
            blog(LOG_ERR, "failed to open dir %s", buf);
            return NULL;
        }
        while((entry = readdir(dp)) != NULL) {
            if(strcmp(".", entry->d_name) == 0 ||
                    strcmp("..", entry->d_name) == 0)
                continue;

            sprintf(buf, "%s/%s", cfg->ngx_log_path, entry->d_name);

            if(lstat(buf, &statbuf)){
                blog(LOG_ERR, "get file stat fail:%s, err:%s", buf, strerror(errno));
                break;
            }

            if(strstr(entry->d_name, ".ok"))
                continue;
            if(memcmp(entry->d_name, "input", 5))
                continue;

            if(S_ISDIR(statbuf.st_mode))
                continue;

            if(handle_input_file(entry->d_name, &hparam)){
                blog(LOG_ERR, "handle input file failed:%s", entry->d_name);
            } else {
                sprintf(buf, "mkdir -p %s/bak; mv %s/%s %s/bak/", cfg->ngx_log_path, cfg->ngx_log_path, entry->d_name, cfg->ngx_log_path);
                call_system(buf);
            }
        }
        closedir(dp);
        flush_temp_input(&hparam, NULL, FLUSH_MODE_TIME);
        sleep(10);
    }
    end_handle(&hparam);
    return NULL;
}

#else

void *
pa_input_worker(void *arg)
{
    char buf[1024*1024] = {0};
    char buf2[1024*1024] = {0};
    FILE *f = NULL;
    struct handle_param hparam;
    time_t start = time(NULL);
    int i, j;

    blog(LOG_DEBUG, "reading input file from nginx:%s", cfg->ngx_log_path);
    begin_handle(&hparam);

    while(!terminate){
        if(!f){
            sprintf(buf, "%s/"NGX_LOG_NAME, cfg->ngx_log_path);
            f = fopen(buf, "r");
        }
        if(f){
            while(fgets(buf, sizeof(buf) - 1, f)){
                buf[sizeof(buf) - 1] = 0;
                blog(LOG_DEBUG, "get line from file : %s", buf);
                for(i = 0,j = 0; buf[i];){
                    if(buf[i] == '\\' && memcmp(buf + i, "\\x22", 4) == 0){
                        buf2[j++] = '"';
                        i += 4;
                    } else {
                        buf2[j++] = buf[i++];
                    }
                }
                while(buf2[j] != ']')j--;
                buf2[j + 1] = 0;
                handle_input_line(buf2, &hparam);
            }
        }
        if(time(NULL) - start > NGX_LOG_SWITH){
            blog(LOG_DEBUG, "told nginx to swith log file");
            sprintf(buf, "cd %s && mv "NGX_LOG_NAME" ./bak/%ld && kill -USR1 `cat nginx.pid`", cfg->ngx_log_path, time(NULL));
            call_system(buf);
            start = time(NULL);
            if(f){
                fclose(f);
                f = NULL;
            }
        }
        flush_temp_input(&hparam, NULL, FLUSH_MODE_TIME);
        sleep(10);
    }
    end_handle(&hparam);
    return NULL;
}

#endif

