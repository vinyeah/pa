#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "list.h"
#include "pacomm.h"
#include "config.h"
#include "place.h"
#include "crc.h"
#include "cJSON.h"

extern struct app_config *cfg;
struct list_head place_list;

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

#define PLACE_BATCH_NUM 4000

char *order_place_json_str(cJSON *json, int id)
{
    char *out = (char*)malloc(1024*2*PLACE_BATCH_NUM);//ori_len*2);
    cJSON *item = NULL;
    int size = 0;
    int len = 0;
    int i = 0;
    if(!out){
        blog(LOG_ERR, "oom");
        return NULL;
    }

    blog(LOG_DEBUG, "id:%d", id);
    size = cJSON_GetArraySize(json);
    blog(LOG_DEBUG, "record items:%d", size);
    sprintf(out, "[");
    len ++;
    for(i = 0; i < size; i++){
        if(i < id || i-id + 1>PLACE_BATCH_NUM)
            continue;
        if(!(item = cJSON_GetArrayItem(json, i))){
            blog(LOG_ERR, "get item %d fail", i);
            continue;
        }
        if(i != id){
            sprintf(out + len, ",");
            len ++;
        }

        len += sprintf(out + len, 
                "{\"SERVICE_CODE\":\"%s\","
                "\"SERVICE_NAME\":\"%s\","
                "\"ADDRESS\":\"%s\","
                "\"ZIP\":\"100000\","
                "\"BUSINESS_NATURE\":\"%s\","
                "\"PRINCIPAL\":\"\","
                "\"PRINCIPAL_TEL\":\"\","
                "\"INFOR_MAN\":\"\","
                "\"INFOR_MAN_TEL\":\"\","
                "\"INFOR_MAN_EMAIL\":\"\","
                "\"PRODUCER_CODE\":\"06\","
                "\"STATUS\":\"%s\","
                "\"ENDING_NUMBER\":\"1\","
                "\"SERVER_NUMBER\":\"1\","
                "\"EXIT_IP\":\"\","
                "\"AUTH_ACCOUNT\":\"\","
                "\"NET_TYPE\":\"08\","
                "\"PRACTITIONER_NUMBER\":\"\","
                "\"NET_MONITOR_DEPARTMENT\":\"\","
                "\"NET_MONITOR_MAN\":\"\","
                "\"NET_MONITOR_MAN_TEL\":\"\","
                "\"REMARK\":\"%s\","
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
                "\"TERMINAL_FACTORY_ORGCODE\":\"%s\","
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
                "\"START_TIME\":\"8:00\","
                "\"END_TIME\":\"18:00\","
                "\"CREATE_TIME\":\"%s\","
                "\"CAP_TYPE\":\"%s\""
                "}",
                (cJSON_GetObjectItem(item, "SERVICE_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "SERVICE_NAME"))->valuestring,
                (cJSON_GetObjectItem(item, "SERVICE_NAME"))->valuestring,
                (cJSON_GetObjectItem(item, "BUSINESS_NATURE"))->valuestring,
                (cJSON_GetObjectItem(item, "STATUS"))->valuestring,
                cfg->org_code,
                (cJSON_GetObjectItem(item, "SERVICE_TYPE"))->valuestring,
                (cJSON_GetObjectItem(item, "PROVINCE_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "CITY_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "AREA_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "XPOINT"))->valuestring,
                (cJSON_GetObjectItem(item, "YPOINT"))->valuestring,
                cfg->org_code,
                (cJSON_GetObjectItem(item, "CREATE_TIME"))->valuestring,
                (cJSON_GetObjectItem(item, "CAP_TYPE"))->valuestring
                );
    }
    sprintf(out + len, "]");
    return out;
}


struct place *
load_place(char *path, cJSON **json_item)
{
    cJSON *item = NULL;
    cJSON *obj = NULL;
    char *p = NULL;
    char *s = NULL;
    long len = 0;
    struct place *place = NULL;
    struct place_list *tmp = NULL;
    struct list_head *pl = NULL;
    struct list_head *m = NULL;

    blog(LOG_DEBUG, "loading:%s", path);
    *json_item = NULL;
    if(!(p = file_to_buf(path, &len))){
        blog(LOG_ERR, "failed to load json file:%s", path);
        return NULL;
    }

    //   blog(LOG_DEBUG, "load place from file: %s", p);
    if(!(item = cJSON_Parse(p))){
        blog(LOG_ERR, "failed to parse json file:%s", path);
        goto fail;
    }
    if(!(s = cJSON_GetObjectItemString(item, "CITY_CODE"))){
        blog(LOG_ERR, "no city_code");
        goto fail;
    }
    list_for_each(m, &place_list){
        tmp = list_entry(m, struct place_list, node);
        if(strcmp(tmp->city_code, s) == 0){
            pl = &(tmp->list);
            break;
        }
    }
    if(!pl){
        blog(LOG_DEBUG, "create place list for :%s", s);
        if(!(tmp = malloc(sizeof(*tmp)))){
            blog(LOG_ERR, "oom");
            goto fail;
        }
        memset(tmp, 0, sizeof(*tmp));
        INIT_LIST_HEAD(&tmp->list);
        safe_strncpy(tmp->city_code, s);
        list_add_tail(&tmp->node, &place_list);
    }

    if(!(s = cJSON_GetObjectItemString(item, "SERVICE_CODE"))){
        blog(LOG_ERR, "no service_code");
        goto fail;
    }
    if(!(place = (struct place *)malloc(sizeof(*place)))){
        blog(LOG_ERR, "oom");
        goto fail;
    }
    memset(place, 0, sizeof(*place));
    strcpy(place->service_code, s);
    if((s = cJSON_GetObjectItemString(item, "CREATE_TIME")))
        safe_strncpy(place->create_time, s);
    list_add_tail(&place->node, &tmp->list);
    free(p);
    *json_item = item;
    return place;
fail:
    if(p)
        free(p);
    if(item)
        cJSON_Delete(item);
    return NULL;
}


#if 0
int
load_dev_call_back(char *service_code)
{
    struct list_head *m = NULL;
    struct list_head *n = NULL;
    struct place *place = NULL;
    struct place_list *pl = NULL;

    list_for_each(n, &place_list){
        pl = list_entry(n, struct place_list, node);
        list_for_each(m, &pl->place_list){
            place = list_entry(n, struct place, node);
            if(strcmp(place->service_code, service_code) == 0){
                place->ap_num ++;
                goto out;
            }
        }
    }
out:
    return 0;
}
#endif


int
place_status_init()
{
    struct list_head *m = NULL;
    struct list_head *n = NULL;
    struct place *place = NULL;
    struct place_list *pl = NULL;
    int count = 0;
    FILE *f = NULL;
    list_for_each(n, &place_list){
        pl = list_entry(n, struct place_list, node);
        if(!(f = fopen(TMP_PLACE_FILE, "w"))){
            blog(LOG_ERR, "failed to open tmp file");
            return -1;
        }
        count = 0;
        fprintf(f, "[");
        list_for_each(m, &pl->list){
            place = list_entry(m, struct place, node);
            if(place->change_flag){
                if(count)
                    fprintf(f, ",");
                fprintf(f, 
                        "{"
                        "\"SERVICE_CODE\":\"%s\","
                        "\"SERVICE_ONLINE_STATUS\":1,"
                        "\"DATA_ONLINE_STATUS\":1,"
                        "\"EQUIPMENT_RUNNING_STATUS\":1,"
                        "\"ACTIVE_PC\":0,"
                        "\"REPORT_PC\":0,"
                        "\"ONLINE_PERSON\":0,"
                        "\"VITRUAL_NUM\":0,"
                        "\"EXT_IP\":\"\","
                        "\"UPDATE_TIME\":\"%s\""
                        "}",
                        place->service_code,
                        place->create_time
                       );
                count ++;
            }
        }
        fprintf(f, "]");
        fclose(f);
        if(count){
            if(putfile_to_output(TMP_PLACE_FILE, PA_TYPE_CSZT, pl->city_code)){
                blog(LOG_ERR, "failed to put file to putput");
            }
        }
    }
    return 0;
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
    char *ft = NULL;
    char *out = NULL;
    long len = 0;
    int size = 0;
    int i = 0;
    int j = 0;
    struct place *place = NULL;
    struct json_map json_map[JSON_MAP_SIZE];

    INIT_LIST_HEAD(&place_list);

    memset(json_map, 0, sizeof(json_map));
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

        if(!(place = load_place(buf, &item))){
            continue;
        }

        sprintf(buf, "%s/"PLACE_DIR"/%s.ok", cfg->data_path, entry->d_name);
        if(access(buf, F_OK) == 0){
            if(item){
                cJSON_Delete(item);
                item = NULL;
            }
            continue;
        }
        place->change_flag = 1;

        if((json = get_json_map(json_map, cJSON_GetObjectItemString(item, "CITY_CODE")))){
            cJSON_AddItemToArray(json, item); 
        }

        //generate to output dir
        sprintf(buf, "touch %s/"PLACE_DIR"/%s.ok", cfg->data_path, entry->d_name);
        call_system(buf);
    }
    closedir(dp);

    blog(LOG_DEBUG, "generate dest place file");

    for(i = 0; i < JSON_MAP_SIZE; i ++){
        if(!json_map[i].area[0])
            continue;
        blog(LOG_DEBUG, "handle json of :%s", json_map[i].area);
        size = cJSON_GetArraySize(json_map[i].json);
        if(size <= 0)
            continue;

        blog(LOG_DEBUG, "size:%d", size);
        //save to temp file
        for(j = 0; j < size; j += PLACE_BATCH_NUM){
            if(!(out = order_place_json_str(json_map[i].json, j)))
                continue;
            if(buf_to_file(TMP_PLACE_FILE, out, strlen(out))){
                blog(LOG_ERR, "save json out file failed");
            } else {
                if(putfile_to_output(TMP_PLACE_FILE, PA_TYPE_CSZL, json_map[i].area)){
                    blog(LOG_ERR, "failed to put file to putput");
                }
            }
            free(out);
        }
        cJSON_Delete(json_map[i].json);
    }
    blog(LOG_DEBUG, "place init finished");
    return 0;
}

#endif
