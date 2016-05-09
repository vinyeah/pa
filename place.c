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

        if(putfile_to_output(buf, PA_TYPE_CSZL, entry->d_name)){
            blog(LOG_ERR, "failed to put file to putput");
        } else {
            //generate to output dir
            sprintf(buf, "touch %s/"PLACE_DIR"/%s.ok", cfg->data_path, entry->d_name);
            call_system(buf);
        }
    }
    closedir(dp);
    return 0;
}

#endif
