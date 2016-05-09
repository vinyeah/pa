#ifndef __DEVMAP_H_
#define __DEVMAP_H_

#include "pacomm.h"
#include "dev.h"
#include "list.h"
#include "rbtree.h"



#define MAP_SIZE    5000

/* special define for mac rbtree implement */
struct map_node {
    rbtree_node_t node;
    struct list_head list;
    u8 mac[6];
    struct mac_node *dest;
    int flag;
};


struct mac_node*
devmap_find(u8 *mac);

int
devmap_load_map();

int
devmap_init();
#endif
