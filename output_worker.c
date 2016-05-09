#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "config.h"
#include "dev.h"
#include "pacomm.h"
#include "ftp.h"
#include "output_worker.h"

extern int terminate;
extern struct app_config *cfg;

#define OK_FLAG     "/tmp/.pa_ok_flag"

int
get_dest(char *type, char *src_id, char *dest)
{
    char *typename = NULL;
    char dbuf[128] = {0}; 
    char tbuf[128] = {0};
    struct tm * tm;
    time_t tt = time(NULL);
    time_t dt = (tt/PA_INTERVAL)*PA_INTERVAL; 

    blog(LOG_DEBUG, "src:%s", src_id);

    if(strcmp(type, PA_TYPE_CSZL) == 0)
        typename = "CSZL";
    else if(strcmp(type, PA_TYPE_SBZL) == 0)
        typename = "SBZL";
    else if(strcmp(type, PA_TYPE_SJRZ) == 0)
        typename = "SJRZ";

    tm = localtime(&dt); 
    if(tm){
        strftime(dbuf, sizeof(dbuf) - 1, "%Y%m%d/%H/%M", tm);
    }

    tm = localtime(&tt); 
    strftime(tbuf, sizeof(tbuf) - 1, "%Y%m%d%H%M%S", tm);
    sprintf(dest, "%s/%s/%s%03d_"GATHER_TYPE"_%s_%s_%s.log", typename, dbuf, tbuf,
                                1+(int) (999.0*rand()/(RAND_MAX+1.0)),
                                src_id,
                                cfg->org_code,
                                type);
}

#if 0
int
transfer_file(char *path, char *type, char *src_id)
{
    char buf[512] = {0};
    char buf2[512] = {0};
    char dest[128] = {0};
    char *p = NULL;
    char *s = NULL;
    int sock = -1;
    int len = 0;
    int ret = 0;
    void *data = NULL;
    unsigned long long dlen = 0;

    if(access(OK_FLAG, F_OK)){
        sprintf(buf, "touch %s", OK_FLAG);
        call_system(buf);
    }

/*
    sprintf(buf, "%s/"OUTPUT_DIR"/", cfg->data_path);
    len = strlen(buf);
    if(memcmp(path, buf, len))
        return -1;
    strcpy(buf, path + len);
    len = strlen(buf);
    p = buf;
    while(*p == '/') p ++;
*/

    get_dest(type, src_id, dest);
    blog(LOG_DEBUG, "dest:[%s]\n", dest);

    p = dest;
    if((sock = ftp_connect(cfg->server, cfg->port, cfg->user, cfg->pass)) == BHU_INVALID_SOCKET){
        blog(LOG_ERR, "failed to connect to ftp server:%s", cfg->server);
        return -1;
    }

    ftp_type(sock, 'I');

    while((s = strchr(p, '/'))){
        *s = 0;
        ftp_mkd(sock, p);
        ftp_cwd(sock, p);
        p = s + 1;
    }

    if((ftp_list(sock, p, &data, &dlen) == 0) && dlen > 0){
        free(data);
        dlen = 0;
        sprintf(buf2, "%s.ok", p);
        if(!((ftp_list(sock, buf2, &data, &dlen) == 0) && dlen > 0)){
            ftp_deletefile(sock, p);
        }
    }
    //mkdir finish, ready to put file
    if((ret = ftp_storfile(sock, path, p, NULL, NULL)) != 0){
        blog(LOG_ERR, "failed to put file to server");
        goto fail;
    }

    strcat(p, ".ok");
    if((ret = ftp_storfile(sock, OK_FLAG, p, NULL, NULL)) != 0){
        blog(LOG_ERR, "failed to create ok flag file");
        goto fail;
    }
    ftp_quit(sock);
    return 0;
fail:
    if(sock != BHU_INVALID_SOCKET)
        ftp_quit(sock);
    return -1;
}

#else

int
transfer_file(char *path, char *type, char *src_id)
{
    char *typename = NULL;
    char buf[512] = {0};
    char date[32] = {0};
    char hour[32] = {0};
    char min[32] = {0};
    char dest[128] = {0};
    char tbuf[32] = {0};
    struct tm * tm;
    time_t tt = time(NULL);
    time_t dt = (tt/PA_INTERVAL)*PA_INTERVAL; 

    blog(LOG_DEBUG, "src:%s", src_id);

    if(strcmp(type, PA_TYPE_CSZL) == 0)
        typename = "CSZL";
    else if(strcmp(type, PA_TYPE_SBZL) == 0)
        typename = "SBZL";
    else if(strcmp(type, PA_TYPE_SJRZ) == 0)
        typename = "SJRZ";

    tm = localtime(&dt); 
    if(tm){
        strftime(date, sizeof(date) - 1, "%Y%m%d", tm);
        strftime(hour, sizeof(hour) - 1, "%H", tm);
        strftime(min, sizeof(min) - 1, "%M", tm);
    }

    tm = localtime(&tt); 
    strftime(tbuf, sizeof(tbuf) - 1, "%Y%m%d%H%M%S", tm);
    sprintf(dest, "%s%03d_"GATHER_TYPE"_%s_%s_%s.log", tbuf,
                                1+(int) (999.0*rand()/(RAND_MAX+1.0)),
                                src_id,
                                cfg->org_code,
                                type);

    blog(LOG_DEBUG, "dest:[%s]\n", dest);

    sprintf(buf, "chmod +x ./ftpsend.sh; ./ftpsend.sh %s %s %s %s %s %s", path, typename, date, hour, min, dest);
    call_system(buf);

    return 0;
}
#endif

int
scan_dir(char *path, char *current_name)
{
    DIR *dp;
    char buf[512] = {0};
    char src_id[32] = {0};
    char *p = NULL;
    struct dirent *entry;
    struct stat statbuf;


    if((dp = opendir(path)) == NULL) {
        blog(LOG_ERR, "failed to open dir %s", path);
        return -1;
    }
    while((entry = readdir(dp)) != NULL) {
        if(entry->d_name[0] == '.')
            continue;

        sprintf(buf, "%s/%s", path, entry->d_name);

        if(lstat(buf, &statbuf)){
            blog(LOG_ERR, "get file stat fail:%s, err:%s", buf, strerror(errno));
            break;
        }
        if(S_ISDIR(statbuf.st_mode)){
            scan_dir(buf, entry->d_name);
        } else {
            blog(LOG_DEBUG, "scan dir, find file :%s", entry->d_name);
            if(strstr(entry->d_name, ".ok"))
                continue;
            sprintf(buf, "%s/%s.ok", path, entry->d_name);
            if(access(buf, F_OK) == 0)
                continue;
            sprintf(buf, "%s/%s", path, entry->d_name);
            p = strchr(entry->d_name, '_');
            memset(src_id, 0, sizeof(src_id));
            memcpy(src_id, entry->d_name, p - entry->d_name);
            if(!transfer_file(buf, current_name, src_id)){
                sprintf(buf, "mv %s/%s %s/"DONE_DIR" &", path, entry->d_name, cfg->data_path);
                call_system(buf);
            } else {
                blog(LOG_ERR, "failed to transfer file to server");
            }
        }
    }
    closedir(dp);
    return 0;
}

void *
pa_output_worker(void *arg)
{
    char buf[512] = {0};
    sprintf(buf, "mkdir -p %s/"DONE_DIR, cfg->data_path);
    call_system(buf);
    sprintf(buf, "%s/"OUTPUT_DIR, cfg->data_path);
    while(!terminate){
        scan_dir(buf, NULL);
        sleep(30);
    }
    return NULL;
}

