#ifndef __PA_COMM_H_
#define __PA_COMM_H_

#include "cJSON.h"

#define SERVICE_CODE_LEN  14


#define PLACE_DIR   "place"
#define DEV_DIR   "dev"
#define OUTPUT_DIR   "output"
#define DONE_DIR   "done"

#define PA_INTERVAL     300



#define GATHER_TYPE     "139"   //wifi
#define BATCH_NUM       100
#define BATCH_TIME      300
#define NGX_LOG_SWITH   300
#define NGX_LOG_FLUSH   10


#define JSON_MAP_SIZE   30


enum {
    NGX_LOG_STA_SNIFFER = 0,
    NGX_LOG_OTHER,
    NGX_LOG_MAX,
};


enum {
    PA_TYPE_WLRZ = 0,   //特征采集
    PA_TYPE_FJFJ,   //网络虚拟身份轨迹
    PA_TYPE_JSTX,   //即时通讯内容
    PA_TYPE_XWRZ,   //上网日志
    PA_TYPE_SJRZ,   //终端上下线日志
    PA_TYPE_PTNR,   //普通内容
    PA_TYPE_SGJZ,   //搜索关键字
    PA_TYPE_CSZL,   //场所资料
    PA_TYPE_CSZT,   //场所状态
    PA_TYPE_SBZL,   //设备资料
    PA_TYPE_JSJZT,  //终端计算机状态
    PA_TYPE_SBGJ,   //采集设备移动轨迹
    PA_TYPE_RZSJ,   //认证数据
    PA_TYPE_SJTZ,   //手机特征数据
    PA_TYPE_PNFJ,   //普通内容附件
    PA_TYPE_MAX
};


enum {
    FLUSH_MODE_AUTO = 0,
    FLUSH_MODE_TIME,
    FLUSH_MODE_FORCE,
};

struct json_map {
    char area[7];
    cJSON *json;
};


struct audit_type {
    char *type;
    char *name;
};

struct handle_param {
    int count;
    time_t start;
};

int
putfile_to_output(char *src, int type, char *src_id);
cJSON *
get_json_map(struct json_map *map, char *area);
int
get_audit_id_by_type(char *type);

int 
ngx_begin_handle();
int 
ngx_end_handle();
void
ngx_input_worker(int ngx_log_type, void *(*line_callback)(char *line));
void
switch_ngx_log(void *arg);
int
ngx_write_temp_handle_file(int type, char *out);

extern struct audit_type audit_type_array[];
extern struct handle_param handle_param_array[];
#endif
