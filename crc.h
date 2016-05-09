#ifndef _CRC_H_
#define _CRC_H_

#include "comm.h"


extern uint32_t   crc32_table256[];

static INLINE uint32_t
crc32_long(u8 *p, size_t len)
{
    uint32_t  crc;

    crc = 0xffffffff;

    while (len--) {
        crc = crc32_table256[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    }

    return crc ^ 0xffffffff;
}


#endif 
