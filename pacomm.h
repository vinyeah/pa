#ifndef __PA_COMM_H_
#define __PA_COMM_H_

#include "cJSON.h"

#define SERVICE_CODE_LEN  14


#define PLACE_DIR   "place"
#define DEV_DIR   "dev"
#define OUTPUT_DIR   "output"
#define DONE_DIR   "done"

#define PA_TYPE_CSZL   "008"
#define PA_TYPE_SBZL   "010"
#define PA_TYPE_SJRZ   "005"

#define PA_INTERVAL     300



#define GATHER_TYPE     "139"   //wifi
#define BATCH_NUM       100
#define BATCH_TIME      300
#define NGX_LOG_SWITH   60
#define NGX_LOG_NAME    "pa.access.log"


#define JSON_MAP_SIZE   30


enum {
    FLUSH_MODE_AUTO = 0,
    FLUSH_MODE_TIME,
    FLUSH_MODE_FORCE,
};

struct json_map {
    char area[7];
    cJSON *json;
};

int
putfile_to_output(char *src, char *type, char *src_id);
cJSON *
get_json_map(struct json_map *map, char *area);
#endif
