#ifndef _CONFIG_H_
#define _CONFIG_H_


struct app_config{
    char *server;
    int port;
    char *user;
    char *pass;
    char *org_code;
    char *ngx_log_path;
    char *data_path;
    char *orion_data_path;
    char *src_id;   //数据产生源标识
    u16 ctrl_port;
};


struct app_config *
load_config(char *confile);
#endif //end of _CONFIG_H_
