#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "dev.h"
#include "pacomm.h"
#include "config.h"
#include "crc.h"
#include "cJSON.h"

#define TMP_DEV_FILE  "/tmp/.pa_tmp_dev"

extern struct app_config *cfg;
rbtree_t dev_root;
rbtree_node_t dev_sentinel;
struct list_head dev_list;

void
mac_rbtree_insert_value(rbtree_node_t *temp,
        rbtree_node_t *node, rbtree_node_t *sentinel);

struct mac_node*
mac_rbtree_find(rbtree_t *rbtree, u8 *mac);

int
mac_rbtree_add(rbtree_t *rbtree, struct mac_node *node);

int
mac_rbtree_del(rbtree_t *rbtree, struct mac_node *node);

/* special for mac rbtree implement */
void
mac_rbtree_insert_value(rbtree_node_t *temp,
        rbtree_node_t *node, rbtree_node_t *sentinel)
{
    struct mac_node *n, *t;
    rbtree_node_t  **p;

    for ( ;; ) {

        n = (struct mac_node *) node;
        t = (struct mac_node *) temp;

        if (node->key != temp->key) {

            p = (node->key < temp->key) ? &temp->left : &temp->right;

        } else {
            p = (memcmp(n->mac, t->mac, 6) < 0) 
                ? &temp->left : &temp->right;
        }    

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    rbt_red(node);
}

struct mac_node*
mac_rbtree_find(rbtree_t *rbtree, u8 *mac)
{
    int_t           rc;
    struct mac_node     *n;
    rbtree_node_t  *node, *sentinel;
    uint32_t hash = crc32_long(mac, 6);

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        n = (struct mac_node *) node;

        if (hash != node->key) {
            node = (hash < node->key) ? node->left : node->right;
            continue;
        }

        rc = memcmp(mac, n->mac, 6);

        if (rc < 0) {
            node = node->left;
            continue;
        }

        if (rc > 0) {
            node = node->right;
            continue;
        }

        return n;
    }
    return NULL;
}

int
mac_rbtree_add(rbtree_t *rbtree, struct mac_node *rbnode)
{
    memset(&rbnode->node, 0, sizeof(rbnode->node));
    rbnode->node.key = crc32_long(rbnode->mac, 6); 
    rbtree_insert(rbtree, &rbnode->node);
    return 0;

}

int
mac_rbtree_del(rbtree_t *rbtree, struct mac_node *rbnode)
{
    rbtree_delete(rbtree, &rbnode->node);
    return 0;
}


struct mac_node*
dev_find(u8 *mac)
{
    return mac_rbtree_find(&dev_root, mac);
}

cJSON *
load_dev_data(char *name)
{
    cJSON *json = NULL;
    cJSON *obj = NULL;
    cJSON *ret = NULL;
    char *s = NULL;
    struct mac_node *node = NULL;
    char *out = NULL;
    
    blog(LOG_DEBUG, "parsing file :%s", name);
    s = file_to_buf(name, NULL);
    if(!s)
        return NULL;
    blog(LOG_DEBUG, "parsing json");
    if(!(json = cJSON_Parse(s))){
        blog(LOG_ERR, "parse file:%s error", name);
        goto out;
    }
    if(!(node = malloc(sizeof(*node))))
        goto out;

    memset(node, 0, sizeof(*node));
    if(!(obj = cJSON_GetObjectItem(json, "MAC"))){
        goto out;
    }
    if(!tc_str2mac(obj->valuestring, node->mac))
        goto out;
   
    if(!(obj = cJSON_GetObjectItem(json, "SERVICE_CODE"))){
        goto out;
    }
    strncpy(node->service_code, obj->valuestring, sizeof(node->service_code) - 1);
    if(mac_rbtree_find(&dev_root, node->mac)){
        blog(LOG_ERR, "duplicated mac found on file %s", name);
        goto out;
    }
    tc_mac2str(node->mac, node->macstr, 0); 
    tc_mac2str(node->mac, node->macstr_dash, '-'); 
    sprintf(node->ap_num, "%s%s", cfg->org_code, node->macstr);

    mac_rbtree_add(&dev_root, node);
    list_add(&node->list, &dev_list);

    ret = json;
    json = NULL;
    node = NULL;
out: 
    if(node)
        free(node);
    if(s)
        free(s); 
    if(json)
        cJSON_Delete(json);
    if(out)
        free(out);

    return ret;
}

int
append_dev_data(cJSON *json)
{
    cJSON *obj = NULL;
    char buf[128] = {0};
    u8 mac[6] = {0};
    int len = 0;

    if(!(obj = cJSON_GetObjectItem(json, "MAC"))){
        return -1;
    }
    if(!tc_str2mac(obj->valuestring, mac)){
        blog(LOG_DEBUG, "failed to change string to mac");
        return -1;
    }
    len = sprintf(buf, "%s", cfg->org_code);
    cJSON_AddStringToObject(json, "SECURITY_FACTORY_ORGCODE", buf);
    tc_mac2str(mac, buf + len, 0);
    buf[len + 12] = 0;
    cJSON_AddStringToObject(json, "EQUIPMENT_NUM", buf);
    cJSON_AddStringToObject(json, "EQUIPMENT_NAME", buf);
    cJSON_AddStringToObject(json, "EQUIPMENT_TYPE", "00");


    //not mandatory data
    cJSON_AddNumberToObject(json, "IP", 0);
    cJSON_AddStringToObject(json, "VENDOR_NAME", "");
    cJSON_AddStringToObject(json, "VENDOR_NUM", "");
    cJSON_AddStringToObject(json, "INSTALL_DATE", "");
    cJSON_AddStringToObject(json, "INSTALL_POINT", "");
    cJSON_AddStringToObject(json, "LONGITUDE", "");
    cJSON_AddStringToObject(json, "LATITUDE", "");
    cJSON_AddStringToObject(json, "SUBWAY_STATION", "");
    cJSON_AddStringToObject(json, "SUBWAY_LINE_INFO", "");
    cJSON_AddStringToObject(json, "SUBWAY_VEHICLE_INFO", "");
    cJSON_AddStringToObject(json, "SUBWAY_COMPARTMENT_NUM", "");
    cJSON_AddStringToObject(json, "CAR_CODE", "");
    cJSON_AddNumberToObject(json, "UPLOAD_TIME_INTERVAL", 0);
    cJSON_AddNumberToObject(json, "COLLECTION_RADIUS", 0);
    cJSON_AddStringToObject(json, "CREATER", "");
    cJSON_AddStringToObject(json, "LAST_CONNECT_TIME", "");
    cJSON_AddStringToObject(json, "REMARK", "");
    cJSON_AddStringToObject(json, "WDA_VERSION", "");
    cJSON_AddStringToObject(json, "FIRMWARE_VERSION", "");

    return 0;
}


#define DEV_BATCH_NUM 100

int order_dev_json_str(char *fname, cJSON *json, int id)
{
    FILE *f = NULL;
    cJSON *item = NULL;
    int size = 0;
    int len = 0;
    int i = 0;
    char *t = NULL;

    if(!(f = fopen(fname, "w"))){
        blog(LOG_ERR, "file open error!");
        return -1;

    }

    size = cJSON_GetArraySize(json);
    blog(LOG_DEBUG, "record items:%d", size);
    fprintf(f, "[");
    for(i = 0; i < size; i++){
        if(i < id || i-id + 1>DEV_BATCH_NUM)
            continue;
        if(!(item = cJSON_GetArrayItem(json, i))){
            blog(LOG_ERR, "get item %d fail", i);
            continue;
        }
        if(i != id){
            fprintf(f, ",");
        }
        fprintf(f, 
                "{\"EQUIPMENT_NUM\":\"%s\","
                "\"EQUIPMENT_NAME\":\"%s\","
                "\"MAC\":\"%s\","
                "\"IP\":0,"
                "\"SECURITY_FACTORY_ORGCODE\":\"%s\","
                "\"VENDOR_NAME\":\"\","
                "\"VENDOR_NUM\":\"\","
                "\"SERVICE_CODE\":\"%s\","
                "\"PROVINCE_CODE\":\"%s\","
                "\"CITY_CODE\":\"%s\","
                "\"AREA_CODE\":\"%s\","
                "\"INSTALL_DATE\":\"\","
                "\"INSTALL_POINT\":\"\","
                "\"EQUIPMENT_TYPE\":\"%s\","
                "\"LONGITUDE\":\"\","
                "\"LATITUDE\":\"\","
                "\"SUBWAY_STATION\":\"\","
                "\"SUBWAY_LINE_INFO\":\"\","
                "\"SUBWAY_VEHICLE_INFO\":\"\","
                "\"SUBWAY_COMPARTMENT_NUM\":\"\","
                "\"CAR_CODE\":\"\","
                "\"UPLOAD_TIME_INTERVAL\":0,"
                "\"COLLECTION_RADIUS\":0,"
                "\"CREATE_TIME\":\"%s\","
                "\"CREATER\":\"\","
                "\"LAST_CONNECT_TIME\":\"\","
                "\"REMARK\":\"\","
                "\"WDA_VERSION\":\"\","
                "\"FIRMWARE_VERSION\":\"\"}",
                (cJSON_GetObjectItem(item, "EQUIPMENT_NUM"))->valuestring,
                (cJSON_GetObjectItem(item, "EQUIPMENT_NAME"))->valuestring,
                (cJSON_GetObjectItem(item, "MAC"))->valuestring,
                (cJSON_GetObjectItem(item, "SECURITY_FACTORY_ORGCODE"))->valuestring,
                (cJSON_GetObjectItem(item, "SERVICE_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "PROVINCE_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "CITY_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "AREA_CODE"))->valuestring,
                (cJSON_GetObjectItem(item, "EQUIPMENT_TYPE"))->valuestring,
                (cJSON_GetObjectItem(item, "CREATE_TIME"))->valuestring
                    );
    }
    fprintf(f, "]");
    fclose(f);
    return 0;
}

int
dev_init()
{
    DIR *dp;
    char buf[512] = {0};
    char *tmp_file = "/tmp/.pa_tmp_dev";
    struct dirent *entry;
    struct stat statbuf;
    cJSON *json = NULL;
    cJSON *item = NULL;
    cJSON *obj = NULL;
    int i = 0;
    struct json_map json_map[JSON_MAP_SIZE];
    int size = 0;
    int j = 0;

    INIT_LIST_HEAD(&dev_list);
    rbtree_init(&dev_root, &dev_sentinel, mac_rbtree_insert_value);
    memset(json_map, 0, sizeof(json_map));

    blog(LOG_DEBUG, "reading config from dev dir:%s", cfg->data_path);
    sprintf(buf, "%s/%s", cfg->data_path, DEV_DIR);
    if((dp = opendir(buf)) == NULL) {
        blog(LOG_ERR, "failed to open dir %s", buf);
        return -1;
    }
    while((entry = readdir(dp)) != NULL) {
        if(entry->d_name[0] == '.')
            continue;

        sprintf(buf, "%s/%s/%s", cfg->data_path, DEV_DIR, entry->d_name);

        if(lstat(buf, &statbuf)){
            blog(LOG_ERR, "get file stat fail:%s, err:%s", buf, strerror(errno));
            continue;
        }

        if(strstr(entry->d_name, ".ok"))
            continue;

        if(S_ISDIR(statbuf.st_mode))
            continue;

        //load dev data
        if(!(item = load_dev_data(buf))){
            blog(LOG_ERR, "load file fail:%s", buf);
            continue;
        }

        blog(LOG_DEBUG, "check ok flag");
        sprintf(buf, "%s/"DEV_DIR"/%s.ok", cfg->data_path, entry->d_name);
        if(access(buf, F_OK) == 0){
            goto next;
        }
        blog(LOG_DEBUG, "appending data to dev item");
        if(append_dev_data(item)){
            blog(LOG_ERR, "failed to append json data");
            goto next;
        }
        if(!(obj = cJSON_GetObjectItem(item, "CITY_CODE"))){
            cJSON_Delete(item);
            goto next;
        }

        if((json = get_json_map(json_map, obj->valuestring))){
            cJSON_AddItemToArray(json, item); 
        }

        sprintf(buf, "touch %s/"DEV_DIR"/%s.ok", cfg->data_path, entry->d_name);
        call_system(buf);
next:
        continue;
    }

    closedir(dp);

    blog(LOG_DEBUG, "generate dest dev file");

    for(i = 0; i < JSON_MAP_SIZE; i ++){
        if(json_map[i].area[0]){
            blog(LOG_DEBUG, "handle json of :%s", json_map[i].area);
            size = cJSON_GetArraySize(json_map[i].json);
            if(size <= 0)
                continue;
            for(j = 0; j < size; j += DEV_BATCH_NUM){
                //save to temp file
                if(!(order_dev_json_str(TMP_DEV_FILE, json_map[i].json, j))){
                    //generate to output dir
                    if(putfile_to_output(TMP_DEV_FILE, PA_TYPE_SBZL, json_map[i].area)){
                        blog(LOG_ERR, "failed to put file to putput");
                    }
                }
            }
            cJSON_Delete(json_map[i].json);
        }
    }
    blog(LOG_DEBUG, "dev init finished");
    devmap_init();
    devmap_load_map(&dev_list);
    return 0;
}

