#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "config.h"
#include "dev.h"
#include "devmap.h"
#include "pacomm.h"
#include "orion_input_worker.h"
#include "cJSON.h"

extern int terminate;
extern struct app_config *cfg;

#define TMP_INPUT_DIR "/tmp/.pa_tmp_input"


/*
2016-05-06 16:00:00,014 - DF{"mac":"84:82:f4:18:34:80","ts":1462521600014,"act":"DF"}
2016-05-06 16:00:00,023 - HF{"mac":"84:82:f4:27:98:28","ts":1462521600023,"hmac":"b0:e0:3c:4d:e2:59","huptime":"33","tx_bytes":0,"rx_bytes":208,"act":"HF"}
2016-05-06 16:00:00,035 - HO{"mac":"84:82:f4:27:cc:4c","ts":1462521600035,"hmac":"b8:55:10:02:c2:67","hname":"","hip":"0.0.0.0","act":"HO"}
2016-05-06 16:00:00,049 - HF{"mac":"84:82:f4:27:cc:4c","ts":1462521600049,"hmac":"b8:55:10:02:c2:67","huptime":"2","tx_bytes":0,"rx_bytes":2433,"act":"HF"}
*/
static int
handle_input_line(char *buf, struct handle_param *hparam)
{
    char *p = NULL;
    char mac[6] = {0};
    char apmacstr[13] = {0};
    char stamac[6] = {0};
    char stamacstr[13] = {0};
    char stamacstr_dash[18] = {0};
    cJSON *json = NULL;
    cJSON *item = NULL;
    cJSON *obj = NULL;
    int i = 0;
    struct mac_node *mn = NULL;
    long t = 0;
    char out[1024*2] = {0};
    int len = 0;
    long uptime = 0;
    char act = 0;

    blog(LOG_ERR, "handle line:%s", buf);
    if(!(p = strstr(buf, " - "))){
        blog(LOG_ERR, "can't find delim");
        return -1;
    }
    *p = 0;

    /*
    if(strcmp(buf, readed_flag) <= 0){
        blog(LOG_DEBUG, "old data:[%s] readed:[%s]", buf, readed_flag);
        return -1;
    }

    update_readed_flag(buf);
    */

    if(p[3] != 'H'){
        blog(LOG_DEBUG, "not a sta message");
        goto fail;
    }
    act = p[4];
    p += 5;
    if(!(json = cJSON_Parse(p))){
        blog(LOG_ERR, "parse json error");
        goto fail;
    }
    blog(LOG_DEBUG, "parse json succeed");

    if((obj = cJSON_GetObjectItem(json, "mac"))){
        tc_str2mac(obj->valuestring, mac);
        if(!(mn = devmap_find(mac))){
            blog(LOG_DEBUG, "no dest mac available, drop");
            goto fail;
        }
    } else {
        blog(LOG_ERR, "can't find dev mac");
        goto fail;
    }

    blog(LOG_DEBUG, "got dest dev by mac");
    memcpy(mac, mn->mac, 6);

    if(!(obj = cJSON_GetObjectItem(json, "hmac")) || !obj->valuestring || strlen(obj->valuestring) <= 0)
        goto fail;
    tc_str2mac(obj->valuestring, stamac);
    tc_mac2str(stamac, stamacstr_dash, '-');
    tc_mac2str(stamac, stamacstr, 0);

    if(!(obj = cJSON_GetObjectItem(json, "ts")))
        goto fail;
    t = obj->valuedouble/1000;

    if((obj = cJSON_GetObjectItem(json, "huptime")) && obj->valuestring && strlen(obj->valuestring))
        uptime = atoi(obj->valuestring);

    sprintf(out, "{");
    len ++;
    len += sprintf(out + len, "\"SERVICE_CODE\":\"%s\"", mn->service_code);
    len += sprintf(out + len, ",\"USER_NAME\":\"\"");
    len += sprintf(out + len, ",\"CERTIFICATE_TYPE\":\"\"");
    len += sprintf(out + len, ",\"CERTIFICATE_CODE\":\"\"");

    if(act == 'O'){
        len += sprintf(out + len, ",\"ONLINE_TIME\":%ld", t);
        len += sprintf(out + len, ",\"OFFLINE_TIME\":0");
    } else {
        len += sprintf(out + len, ",\"ONLINE_TIME\":%ld", t - uptime);
        len += sprintf(out + len, ",\"OFFLINE_TIME\":%ld", t);
    }
    if(!(obj = cJSON_GetObjectItem(json, "hname")) || !obj->valuestring || strlen(obj->valuestring) <= 0)
        goto fail;
    len += sprintf(out + len, ",\"NET_ENDING_NAME\":\"%s\"", obj->valuestring);
    t = 0;
    if((obj = cJSON_GetObjectItem(json, "hip")) && obj->valuestring)
        t = tc_str2ip(obj->valuestring);
    len += sprintf(out + len, ",\"NET_ENDING_IP\":%ld", t);
    len += sprintf(out + len, ",\"NET_ENDING_MAC\":\"%s\"", stamacstr_dash);
    len += sprintf(out + len, ",\"ORG_NAME\":\"\"");
    len += sprintf(out + len, ",\"COUNTRY\":\"\"");
    len += sprintf(out + len, ",\"NOTE\":\"\"");
    sprintf(buf, "%s%s%ld", mn->service_code, stamacstr, uptime);
    len += sprintf(out + len, ",\"SESSION_ID\":\"%s\"", buf);
    len += sprintf(out + len, ",\"MOBILE_PHONE\":\"\"");
    len += sprintf(out + len, ",\"SRC_V4_IP\":0");
    len += sprintf(out + len, ",\"SRC_V6_IP\":\"\"");
    len += sprintf(out + len, ",\"SRC_V4START_PORT\":0");
    len += sprintf(out + len, ",\"SRC_V4END_PORT\":0");
    len += sprintf(out + len, ",\"SRC_V6START_PORT\":0");
    len += sprintf(out + len, ",\"SRC_V6END_PORT\":0");
    len += sprintf(out + len, ",\"AP_NUM\":\"%s\"", mn->ap_num);
    len += sprintf(out + len, ",\"AP_MAC\":\"%s\"", mn->macstr_dash);
    len += sprintf(out + len, ",\"AP_XPOINT\":\"\"");
    len += sprintf(out + len, ",\"AP_YPOINT\":\"\"");
    len += sprintf(out + len, ",\"POWER\":\"\"");
    len += sprintf(out + len, ",\"XPOINT\":\"\"");
    len += sprintf(out + len, ",\"YPOINT\":\"\"");
    len += sprintf(out + len, ",\"AUTH_TYPE\":\"\"");
    len += sprintf(out + len, ",\"AUTH_CODE\":\"\"");
    len += sprintf(out + len, ",\"COMPANY_ID\":\"%s\"", cfg->org_code);
    sprintf(out + len, "}");
    ngx_write_temp_handle_file(PA_TYPE_SJRZ, out);

    blog(LOG_DEBUG, "handle record finished");
    cJSON_Delete(json);
    return 0;
fail:
    if(json)
        cJSON_Delete(json);
    return -1;
}


static int
handle_input_file(char *fname, struct handle_param *hparam)
{
    FILE *f = NULL;
    char buf[1024*1024] = {0};
    char *p = NULL;

    blog(LOG_DEBUG, "handling file:%s", fname);
    if(!(f = fopen(fname, "r"))){
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

void *
pa_orion_input_worker(void *arg)
{
    DIR *dp;
    char buf[512] = {0};
    int i = 0;
    char *p = NULL;
    struct dirent *entry;
    struct stat statbuf;
    struct handle_param hparam;

    blog(LOG_DEBUG, "reading orion input:%s ", cfg->orion_data_path);
    while(!terminate){
        for(i = 1; i <= 2; i ++){
            sprintf(buf, "cd %s/i%d/ 2>/dev/null && ls -lt *.zip 2>/dev/null | awk '{print $9}' | tail -n 1", cfg->orion_data_path, i);
            if(!(p = file_get_cmd_ex(buf))){
                continue;
            }
            if(strlen(p) > 0 && strstr(p, "2016") && strstr(p, ".zip")){
                blog(LOG_DEBUG, "find input file to handle:%s", p);
                sprintf(buf, "mkdir -p "TMP_INPUT_DIR"; rm -f "TMP_INPUT_DIR"/* ; cp %s/i%d/%s "TMP_INPUT_DIR"/; cd "TMP_INPUT_DIR"/ ; unzip * ; rm 2016*.zip; mv 2016* tt",
                        cfg->orion_data_path, i, p
                        );
                call_system(buf);
                sprintf(buf, TMP_INPUT_DIR"/tt");
                if(access(buf, F_OK) == 0){
                    blog(LOG_ERR, "got extraced file, succeed");
                    if(handle_input_file(buf, &hparam)){
                        blog(LOG_ERR, "handle input file failed:%s", entry->d_name);
                    }
                    sprintf(buf, "rm -f %s/i%d/%s", cfg->orion_data_path, i, p);
                    call_system(buf);
                } else {
                    blog(LOG_ERR, "can't got extraced file");
                }
            }
            free(p);
        }
        sleep(3);
    }
    return NULL;
}


