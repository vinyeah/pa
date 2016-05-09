#ifndef __DEV_H_
#define __DEV_H_

#include "pacomm.h"
#include "rbtree.h"
#include "list.h"


/* special define for mac rbtree implement */
struct mac_node {
    rbtree_node_t node;
    struct list_head list;
    u8 mac[6];
    char service_code[SERVICE_CODE_LEN + 1];
    char ap_num[22];
    u8 macstr[13];
    u8 macstr_dash[18];
};


struct mac_node*
dev_find(u8 *mac);


int
dev_init();
#endif
