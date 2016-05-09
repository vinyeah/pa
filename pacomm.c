#include <sys/stat.h>
#include <sys/types.h>
#include "comm.h"
#include "pacomm.h"
#include "config.h"

extern struct app_config *cfg;



int
putfile_to_output(char *src, char *type, char *src_id)
{
    char dbuf[512] = {0}; 
    char buf[1024] = {0};
    char tbuf[32] = {0};
    char uuid[38] = {0};

    struct tm * tm;
    time_t tt = time(NULL);
    time_t dt = (tt/PA_INTERVAL)*PA_INTERVAL; 

    blog(LOG_DEBUG, "adding file to output path");

    sprintf(dbuf, "%s/"OUTPUT_DIR"/%s/", cfg->data_path, type);
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
