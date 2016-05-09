#include "comm.h"
#include "config.h"
#include "ctrl.h"
#include "inifile.h"

#ifdef __MINGW32__
#define APP_CONFIG_FILE "c:\\bhu\\bms\\tunnul\\config.ini"
#else
#define APP_CONFIG_FILE "config.ini"
#endif


struct app_config *
load_config(char *confile)
{
    struct app_config *cfg = NULL;
    const char *section = "audit";
    char buf[128];

    if((cfg = (struct app_config *)malloc(sizeof(*cfg))) == NULL){
        blog(LOG_ERR, "oom");
        return NULL;
    }
    memset(cfg, 0, sizeof(*cfg));

    if (confile == NULL) {
        confile = APP_CONFIG_FILE;
    }

    if(!read_profile_string(section, "ftp_server", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "server addr can't be empty");
        goto fail;
    }
    cfg->server = strdup(buf);

    if(!read_profile_string(section, "ftp_port", buf, sizeof(buf) - 1, "21", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    cfg->port = atoi(buf);

    if(!read_profile_string(section, "ftp_user", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "username can't be empty");
        goto fail;
    }
    cfg->user = strdup(buf);

    if(!read_profile_string(section, "ftp_pass", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "password can't be empty");
        goto fail;
    }
    cfg->pass = strdup(buf);

    if(!read_profile_string(section, "ctrl_port", buf, sizeof(buf) - 1, APPCTRL_DEFAULT_PORT, confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    cfg->ctrl_port = htons(atoi(buf));

    if(!read_profile_string(section, "ngx_log_path", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "ngx_log_path can't be empty");
        goto fail;
    }
    cfg->ngx_log_path = strdup(buf);

    if(!read_profile_string(section, "orion_data_path", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "orion_data_path can't be empty");
        goto fail;
    }
    cfg->orion_data_path = strdup(buf);


    if(!read_profile_string(section, "data_path", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "data path can't be empty");
        goto fail;
    }
    cfg->data_path = strdup(buf);

    if(!read_profile_string(section, "src_id", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "src_id can't be empty");
        goto fail;
    }
    cfg->src_id = strdup(buf);

    if(!read_profile_string(section, "org_code", buf, sizeof(buf) - 1, "", confile)){
        blog(LOG_ERR, "read config file failed");
        goto fail;
    }
    if(strlen(buf) <= 0){
        blog(LOG_ERR, "org_code can't be empty");
        goto fail;
    }
    cfg->org_code = strdup(buf);
    return cfg;
fail:
    if(cfg)
        free(cfg);
    return NULL;
}

