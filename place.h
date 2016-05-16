#ifndef __PLACE_H_
#define __PLACE_H_

#include "comm.h"
#include "rbtree.h"

struct place_list {
    struct list_head node;
    char city_code[7];
    struct list_head list;
};

struct place {
    struct list_head node;
    char service_code[SERVICE_CODE_LEN + 1];
    char create_time[20];
    char change_flag;
//    int ap_num;
};

int
place_init();
int
place_status_init();


#endif
