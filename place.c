#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "pacomm.h"
#include "config.h"
#include "place.h"
#include "crc.h"
#include "cJSON.h"

extern struct app_config *cfg;

#define TMP_PLACE_FILE  "/tmp/.pa_tmp_place"

#if 0
int
place_init()
{
    DIR *dp;
    char buf[512] = {0};
    struct dirent *entry;
    struct stat statbuf;
    cJSON *json = NULL;
    cJSON *item = NULL;
    char *ft = NULL;
    char *out = NULL;

    blog(LOG_DEBUG, "reading config from place dir:%s", cfg->data_path);
    sprintf(buf, "%s/%s", cfg->data_path, PLACE_DIR);
    if((dp = opendir(buf)) == NULL) {
        blog(LOG_ERR, "failed to open dir %s", buf);
        return -1;
    }
    json = cJSON_CreateArray();
    while((entry = readdir(dp)) != NULL) {
        if(strcmp(".", entry->d_name) == 0 ||
                strcmp("..", entry->d_name) == 0)
            continue;

        sprintf(buf, "%s/%s/%s", cfg->data_path, PLACE_DIR, entry->d_name);

        if(lstat(buf, &statbuf)){
            blog(LOG_ERR, "get file stat fail:%s, err:%s", buf, strerror(errno));
            closedir(dp);
            return -1;
        }

        if(strstr(entry->d_name, ".ok"))
            continue;

        if(S_ISDIR(statbuf.st_mode))
            continue;

        sprintf(buf, "%s/"PLACE_DIR"/%s.ok", cfg->data_path, entry->d_name);
        if(access(buf, F_OK) == 0)
            continue;

        sprintf(buf, "%s/"PLACE_DIR"/%s", cfg->data_path, entry->d_name);
        if(!(ft = file_to_buf(buf, NULL))){
            blog(LOG_ERR, "load file fail:%s", buf);
            continue;
        }

        if(!(item = cJSON_Parse(ft))){
            blog(LOG_ERR, "parse json error");
            free(ft);
            continue;
        }
        cJSON_AddItemToArray(json, item); 
        free(ft);

        //generate to output dir
        sprintf(buf, "touch %s/"PLACE_DIR"/%s.ok", cfg->data_path, entry->d_name);
        call_system(buf);
    }
    closedir(dp);
    if(cJSON_GetArraySize(json) > 0){
        //save to temp file
        if(!(out = cJSON_PrintUnformatted(json))){
            blog(LOG_ERR, "failed to get json out");
        } else {
            if(buf_to_file(TMP_PLACE_FILE, out, strlen(out))){
                blog(LOG_ERR, "save json out file failed");
            } else {
                if(putfile_to_output(TMP_PLACE_FILE, PA_TYPE_CSZL)){
                    blog(LOG_ERR, "failed to put file to putput");
                }
            }
        }
    }
    if(json)
        cJSON_Delete(json);
    if(out)
        free(out);
    return 0;
}


#else

#define PLACE_BATCH_NUM 100

char *order_place_json_str(cJSON *json, long ori_len, int id)
{
    char *out = (char*)malloc(ori_len*2);
    cJSON *item = NULL;
    int size = 0;
    int len = 0;
    int i = 0;
    if(!out){
        blog(LOG_ERR, "oom");
        return NULL;
    }

    size = cJSON_GetArraySize(json);
    blog(LOG_DEBUG, "record items:%d", size);
    sprintf(out, "[");
    len ++;
    for(i = 0; i < size; i++){
        if(i < id || i-id>PLACE_BATCH_NUM)
            continue;
        if(!(item = cJSON_GetArrayItem(json, i))){
            blog(LOG_ERR, "get item %d fail", i);
            continue;
        }
        if(i){
            sprintf(out + len, ",");
            len ++;
        }
        len += sprintf(out + len, 
                "{\"SERVICE_CODE\":\"%s\","
                "\"SERVICE_NAME\":\"%s\","
                "\"ADDRESS\":\"\","
                "\"ZIP\":\"\","
                "\"BUSINESS_NATURE\":\"%s\","
                "\"PRINCIPAL\":\"\","
                "\"PRINCIPAL_TEL\":\"\","
                "\"INFOR_MAN\":\"\","
                "\"INFOR_MAN_TEL\":\"\","
                "\"INFOR_MAN_EMAIL\":\"\","
                "\"PRODUCER_CODE\":\"\","
                "\"STATUS\":\"%s\","
                "\"ENDING_NUMBER\":\"\","
                "\"SERVER_NUMBER\":\"\","
                "\"EXIT_IP\":\"\","
                "\"AUTH_ACCOUNT\":\"\","
                "\"NET_TYPE\":\"\","
                "\"PRACTITIONER_NUMBER\":\"\","
                "\"NET_MONITOR_DEPARTMENT\":\"\","
                "\"NET_MONITOR_MAN\":\"\","
                "\"NET_MONITOR_MAN_TEL\":\"\","
                "\"REMARK\":\"\","
                "\"SERVICE_TYPE\":\"%s\","
                "\"PROVINCE_CODE\":\"%s\","
                "\"CITY_CODE\":\"%s\","
                "\"AREA_CODE\":\"%s\","
                "\"CITY_TYPE\":\"\","
                "\"POLICE_CODE\":\"\","
                "\"MAIL_ACCOUNT\":\"\","
                "\"MOBILE_ACCOUNT\":\"\","
                "\"XPOINT\":\"%s\","
                "\"YPOINT\":\"%s\","
                "\"GIS_XPOINT\":\"\","
                "\"GIS_YPOINT\":\"\","
                "\"TERMINAL_FACTORY_ORGCODE\":\"\","
                "\"ORG_CODE\":\"\","
                "\"IP_TYPE\":\"\","
                "\"BAND_WIDTH\":\"\","
                "\"NET_LAN\":\"\","
                "\"NET_LAN_TERMINAL\":\"\","
                "\"IS_SAFE\":\"\","
                "\"WIFI_TERMINAL\":\"\","
                "\"PRINCIPAL_CERT_TYPE\":\"\","
                "\"PRINCIPAL_CERT_CODE\":\"\","
                "\"PERSON_NAME\":\"\","
                "\"PERSON_TEL\":\"\","
                "\"PERSON_QQ\":\"\","
                "\"INFOR_MAN_QQ\":\"\","
                "\"START_TIME\":\"\","
                "\"END_TIME\":\"\","
                "\"CREATE_TIME\":\"%s\","
                "\"CAP_TYPE\":\"%s\""
                "}",
                (cJSON_GetObjectItem(item, "SERVICE_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "SERVICE_NAME"))->valuestring,
                (cJSON_GetObjectItem(item, "BUSINESS_NATURE"))->valuestring,
                (cJSON_GetObjectItem(item, "STATUS"))->valuestring,
                (cJSON_GetObjectItem(item, "SERVICE_TYPE"))->valuestring,
                (cJSON_GetObjectItem(item, "PROVINCE_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "CITY_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "AREA_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "XPOINT"))->valuestring,
                (cJSON_GetObjectItem(item, "YPOINT"))->valuestring,
                (cJSON_GetObjectItem(item, "CREATE_TIME"))->valuestring,
                (cJSON_GetObjectItem(item, "CAP_TYPE"))->valuestring
                );
    }
    sprintf(out + len, "]");
    return out;
}


int
place_init()
{
    DIR *dp;
    char buf[512] = {0};
    struct dirent *entry;
    struct stat statbuf;
    cJSON *json = NULL;
    cJSON *item = NULL;
    cJSON *obj = NULL;
    char *ft = NULL;
    char *out = NULL;
    char *p = NULL;
    long len = 0;
    int size = 0;
    int i = 0;


    blog(LOG_DEBUG, "reading config from place dir:%s", cfg->data_path);
    sprintf(buf, "%s/%s", cfg->data_path, PLACE_DIR);
    if((dp = opendir(buf)) == NULL) {
        blog(LOG_ERR, "failed to open dir %s", buf);
        return -1;
    }
    while((entry = readdir(dp)) != NULL) {
        if(entry->d_name[0] == '.')
            continue;

        sprintf(buf, "%s/%s/%s", cfg->data_path, PLACE_DIR, entry->d_name);

        if(lstat(buf, &statbuf)){
            blog(LOG_ERR, "get file stat fail:%s, err:%s", buf, strerror(errno));
            continue;
        }

        if(strstr(entry->d_name, ".ok"))
            continue;

        if(S_ISDIR(statbuf.st_mode))
            continue;

        sprintf(buf, "%s/"PLACE_DIR"/%s.ok", cfg->data_path, entry->d_name);
        if(access(buf, F_OK) == 0)
            continue;

        sprintf(buf, "%s/"PLACE_DIR"/%s", cfg->data_path, entry->d_name);

        if(!(p = file_to_buf(buf, &len))){
            blog(LOG_ERR, "failed to load json file");
            continue;
        }

        if((json = cJSON_Parse(p))){
            size = cJSON_GetArraySize(json);
            for(i = 0; i < size; i +=PLACE_BATCH_NUM){
                if((out = order_place_json_str(json, len, i))){
                    if(buf_to_file(TMP_PLACE_FILE, out, strlen(out))){
                        blog(LOG_ERR, "save json out file failed");
                    } else {
                        //if(putfile_to_output(buf, PA_TYPE_CSZL, entry->d_name)){
                        if(putfile_to_output(TMP_PLACE_FILE, PA_TYPE_CSZL, entry->d_name)){
                            blog(LOG_ERR, "failed to put file to putput");
                        } else {
                            //generate to output dir
                            sprintf(buf, "touch %s/"PLACE_DIR"/%s.ok", cfg->data_path, entry->d_name);
                            call_system(buf);
                        }
                    }
                    free(out);
                } else {
                    blog(LOG_ERR, "order json error");
                }
           }
           cJSON_Delete(json);
        } else {
            blog(LOG_ERR, "parse json error");
        }

        free(p);
    }
    closedir(dp);
    return 0;
}

#endif
