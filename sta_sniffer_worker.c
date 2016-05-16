#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "config.h"
#include "dev.h"
#include "pacomm.h"
#include "sta_sniffer_worker.h"
#include "cJSON.h"

extern int terminate;
extern struct app_config *cfg;

void *
handle_sta_sniffer_input_line(char *buf)
{
    char mac[6] = {0};
    char out[1024*8] = {0};
    char *p = NULL;
    long len = 0;
    char stamac[6] = {0};
    char stamacstr[13] = {0};
    cJSON *json = NULL;
    cJSON *item = NULL;
    cJSON *ary = NULL;
    cJSON *obj = NULL;
    int size = 0;
    int i = 0;
    int j = 0;
    struct mac_node *mn = NULL;
    time_t online;
    time_t offline;

    blog(LOG_DEBUG, "handle line:%s", buf);

    if(!(json = cJSON_Parse(buf))){
        blog(LOG_ERR, "parse json error");
        return NULL;
    }

    blog(LOG_DEBUG, "parse json succeed");

    if(!(p = cJSON_GetObjectItemString(json, "dev"))){
        blog(LOG_ERR, "no dev property found");
        goto out;
    };

    memset(mac, 0, sizeof(mac));
    tc_str2mac(p, mac);
    if(!(mn = dev_find(mac))){
        blog(LOG_ERR, "no such dev data:%s", p);
        goto out;
    }

    blog(LOG_DEBUG, "got dev by mac");
    if(!(ary = cJSON_GetObjectItem(json, "item"))){
        blog(LOG_ERR, "no data item found");
        goto out;
    }
    size = cJSON_GetArraySize(ary);
    blog(LOG_DEBUG, "record items:%d", size);
    for(i = 0; i < size; i++){
        if(!(item = cJSON_GetArrayItem(ary, i))){
            blog(LOG_ERR, "get item %d fail", i);
            continue;
        }
        sprintf(out, "{");
        len = 1;

        if(!(p = cJSON_GetObjectItemString(item, "mac"))){
            blog(LOG_ERR, "no sta mac found");
            continue;
        }
        tc_str2mac(p, stamac);
        tc_mac2str(stamac, stamacstr, '-');
        len += sprintf(out + len, "\"MAC\":\"%s\",\"TYPE\":1", stamacstr);
        if(!(p = cJSON_GetObjectItemString(item, "snifftime"))){
            blog(LOG_ERR, "no sniff time found");
            continue;
        }
        len += sprintf(out + len, ",\"START_TIME\":%s", p);
        online = atol(p);
        offline = online;
        if(!(p = cJSON_GetObjectItemString(item, "duration"))){
            offline += atol(p);
        }
        len += sprintf(out + len, ",\"END_TIME\":%ld", offline);

        len += sprintf(out + len, ",\"POWER\":\"0\",\"BSSID\":\"\",\"ESSID\":\"\"");

        if((obj = cJSON_GetObjectItem(item, "ssid"))){
            len += sprintf(out + len, ",\"HISTORY_ESSID\":\"");
            for(j = 0; j < cJSON_GetArraySize(obj); j++){
                if(j)
                    len += sprintf(out + len, ",");
                len += sprintf(out + len, "\"%s\"", cJSON_GetArrayItem(ary, j)->valuestring);
            }
            len += sprintf(out + len, "\"");
        } else {
            len += sprintf(out + len, ",\"HISTORY_ESSID\":\"\"");
        }
        if((p = cJSON_GetObjectItemString(item, "model_name"))){
            len += sprintf(out + len, ",\"MODEL\":\"%s\"", p);
        } else {
            len += sprintf(out + len, ",\"MODEL\":\"\"");
        }
        len += sprintf(out + len, ",\"OS_VERSION\":\"\",\"IMEI\":\"\",\"IMSI\":\"\",\"STATION\":\"\"");

        len += sprintf(out + len, ",\"XPOINT\":\"0\",\"YPOINT\":\"0\"");
        len += sprintf(out + len, ",\"PHONE\":\"\"");

        len += sprintf(out + len, ",\"DEVMAC\":\"%s\"", mn->macstr_dash);
        len += sprintf(out + len, ",\"DEVICENUM\":\"%s\"", mn->ap_num);
        len += sprintf(out + len, ",\"SERVICE_CODE\":\"%s\"", mn->service_code);

        len += sprintf(out + len, ",\"PROTOCOL_TYPE\":\"\",\"ACCOUNT\":\"\",\"FLAG\":\"\",\"URL\",\"\"");

        len += sprintf(out + len, ",\"COMPANY_ID\":\"%s\"", cfg->org_code);

        if((obj = cJSON_GetObjectItem(item, "rfband"))){
            len += sprintf(out + len, ",\"AP_CHANNEL\":\"%d\"", obj->valueint);
        } else {
            len += sprintf(out + len, ",\"AP_CHANNEL\":\"\"");
        }

        len += sprintf(out + len, ",\"AP_ENCRYTYPE\":\"\"");
        len += sprintf(out + len, ",\"CONSULT_XPOINT\":\"\"");
        len += sprintf(out + len, ",\"CONSULT_YPOINT\":\"\"");


        len += sprintf(out + len, "}");

        blog(LOG_DEBUG, "save to temp file");
        ngx_write_temp_handle_file(PA_TYPE_WLRZ, out);
    }
    blog(LOG_DEBUG, "handle record finished");
out:
    if(json)
        cJSON_Delete(json);
    return NULL;
}




void *
sta_sniffer_input_worker(void *arg)
{
    ngx_input_worker(NGX_LOG_STA_SNIFFER, handle_sta_sniffer_input_line);
    return NULL;
}


