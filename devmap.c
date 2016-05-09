#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "comm.h"
#include "devmap.h"
#include "pacomm.h"
#include "list.h"
#include "config.h"
#include "crc.h"
#include "cJSON.h"

#define DEV_MAP_FILE    "./dev_map"

extern struct app_config *cfg;
rbtree_t devmap_root;
rbtree_node_t devmap_sentinel;
int map_count = 0;
int dest_mac_count = 0;
static struct list_head *dest_dev_list = NULL;
static struct list_head map_list;

void
map_rbtree_insert_value(rbtree_node_t *temp,
        rbtree_node_t *node, rbtree_node_t *sentinel);

struct map_node*
map_rbtree_find(rbtree_t *rbtree, u8 *mac);

int
map_rbtree_add(rbtree_t *rbtree, struct map_node *node);

int
map_rbtree_del(rbtree_t *rbtree, struct map_node *node);

/* special for mac rbtree implement */
void
map_rbtree_insert_value(rbtree_node_t *temp,
        rbtree_node_t *node, rbtree_node_t *sentinel)
{
    struct map_node *n, *t;
    rbtree_node_t  **p;

    for ( ;; ) {

        n = (struct map_node *) node;
        t = (struct map_node *) temp;

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

struct map_node*
map_rbtree_find(rbtree_t *rbtree, u8 *mac)
{
    int_t           rc;
    struct map_node     *n;
    rbtree_node_t  *node, *sentinel;
    uint32_t hash = crc32_long(mac, 6);

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        n = (struct map_node *) node;

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
map_rbtree_add(rbtree_t *rbtree, struct map_node *rbnode)
{
    memset(&rbnode->node, 0, sizeof(rbnode->node));
    rbnode->node.key = crc32_long(rbnode->mac, 6); 
    rbtree_insert(rbtree, &rbnode->node);
    return 0;

}

int
map_rbtree_del(rbtree_t *rbtree, struct map_node *rbnode)
{
    rbtree_delete(rbtree, &rbnode->node);
    return 0;
}

static void
_save_map()
{
    FILE *f = NULL;
    struct list_head *p = NULL;
    struct map_node *m = NULL;
    if(list_empty(&map_list)){
        return;
    }
    if(!(f = fopen(DEV_MAP_FILE, "w"))){
        blog(LOG_ERR, "open map file for write error");
        return;
    }
    list_for_each(p, &map_list){
        m = list_entry(p, struct map_node, list);
        if(m->flag)
            continue;
        fprintf(f, "%02x%02x%02x%02x%02x%02x,%02x%02x%02x%02x%02x%02x\n",
                m->mac[0],
                m->mac[1],
                m->mac[2],
                m->mac[3],
                m->mac[4],
                m->mac[5],
                m->dest->mac[0],
                m->dest->mac[1],
                m->dest->mac[2],
                m->dest->mac[3],
                m->dest->mac[4],
                m->dest->mac[5]
               );
    }
    fclose(f);
}


struct mac_node*
devmap_find(u8 *mac)
{
    struct list_head *p = NULL;
    int i = 0;
    int index = 0;
    struct map_node* m = map_rbtree_find(&devmap_root, mac);
    if(m)
        return m->dest;
    else if(map_count < MAP_SIZE){
        index = 1+(int) (1.0*dest_mac_count*rand()/(RAND_MAX+1.0)) - 1;
        if(index < 0)
            index = 0;
        list_for_each(p, dest_dev_list){
            if(i == index){
                if(!(m = malloc(sizeof(*m)))){
                    return NULL;
                }
                blog(LOG_DEBUG, "adding a new map");
                memset(m, 0, sizeof(*m));
                memcpy(m->mac, mac, 6);
                m->dest = list_entry(p, struct mac_node, list);
                map_rbtree_add(&devmap_root, m);
                list_add(&m->list, &map_list);
                map_count ++;
                _save_map();
                return m->dest;
            }
           i ++; 
        }
    }
    return NULL;
}


int
devmap_load_map(struct list_head *list)
{
    FILE *f = NULL;
    struct list_head *p = NULL;
    struct map_node *n = NULL;
    struct mac_node *m = NULL;
    char buf[512] = {0};
    char *s = NULL;
    int i = 0;
    u8 src_mac[6] = {0};
    u8 dest_mac[6] = {0};


    list_for_each(p, list){
        m = list_entry(p, struct mac_node, list);
        if(!(n = malloc(sizeof(*n)))){
            return -1;
        }
        memset(n, 0, sizeof(*n));
        memcpy(n->mac, m->mac, 6);
        n->dest = m;
        n->flag = 1;
        map_rbtree_add(&devmap_root, n);
        list_add(&n->list, &map_list);
        dest_mac_count ++;
    }
    blog(LOG_WARNING, "loaded dest dev, count:%d", dest_mac_count);

    map_count = 0;
    if((f = fopen(DEV_MAP_FILE, "r"))){
        while(fgets(buf, sizeof(buf) - 1, f)){
            buf[sizeof(buf) - 1] = 0;
            i = strlen(buf) - 1;
            while(i && (buf[i] == '\r' || buf[i] == '\n'))buf[i] = 0;
            if(!(s = strchr(buf, ','))){
                continue;
            }
            *s = 0;
            tc_str2mac(buf, src_mac);
            tc_str2mac(s + 1, dest_mac);
            if((m = dev_find(dest_mac))){
                if(!(n = malloc(sizeof(*n)))){
                    goto fail;
                }
                memset(n, 0, sizeof(*n));
                memcpy(n->mac, src_mac, 6);
                n->dest = m;
                map_rbtree_add(&devmap_root, n);
                list_add(&n->list, &map_list);
                map_count ++;
            }
        }
        fclose(f);
    }
    blog(LOG_WARNING, "dev map loaded, count:%d", map_count);
    dest_dev_list = list;
    return 0;
fail:
    if(f)
        fclose(f);
    return -1;
}



int
devmap_init()
{
    INIT_LIST_HEAD(&map_list);
    rbtree_init(&devmap_root, &devmap_sentinel, map_rbtree_insert_value);
    return 0;
}

