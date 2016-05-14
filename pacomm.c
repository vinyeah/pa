#include <sys/stat.h>
#include <sys/types.h>
#include "comm.h"
#include "pacomm.h"
#include "config.h"


extern struct app_config *cfg;

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
