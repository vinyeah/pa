#ifndef _COMM_H
#define _COMM_H

/* 共同拥有的头文件 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

/* 平台依赖的头文件 */
#ifdef __MINGW32__
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <poll.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ifaddrs.h>

#endif // __MINGW32__

#include "list.h"
#include "eloop.h"
#include "bhu_version.h"


typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;


void sock_reuse(int fd);
int sock_nonblocking(int sock);

/* 平台依赖的函数 */
#ifdef __MINGW32__

#else // __MINGW32__
int closesocket(int fd);

#endif // __MINGW32__



#ifdef __MINGW32__
#define CLOCK_MONOTONIC 0
enum {
    LOG_EMERG = 0,
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG
};
#endif


enum {
    LOG_TO_FILE = 0,
    LOG_TO_CONSOLE,
#ifndef __MINGW32__
    LOG_TO_SYSLOG,
#endif
    LOG_DST_MAX,
};
extern int g_loglv;



#define MAC_FMT             "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_STR(x)           (x)[0],(x)[1],(x)[2],(x)[3],(x)[4],(x)[5]


#define BHU_DEFAULT_RESERVED_LEN     32
#define BHU_DEFAULT_REFD_STR_LEN     1400

#define BHU_STR_POOL_SIZE            (64 * 1024)

#define BHU_STR_POOL_KEEP            10




extern time_t __tt;
extern struct tm * __tm;
extern char __tstr[32];

#define log_mk_time() {__tstr[0] = 0; __tt = time(NULL); __tm = localtime(&__tt); if(__tm){strftime(__tstr, sizeof(__tstr)-1, "%Y-%m-%d %H:%M:%S", __tm);}}
#ifdef __MINGW32__
#define blog(level, fmt, args...)  { if(g_loglv >= level){ log_mk_time(); __blog(level, "[%s][%ld] %s,%s,%d:"fmt"\n", __tstr, GetCurrentThreadId(), get_loglevel_name(level), __func__, __LINE__, ##args);}}
#else
#define blog(level, fmt, args...)  { if(g_loglv >= level){ log_mk_time(); __blog(level, "%s, %s,%s,%d:"fmt"\n", __tstr, get_loglevel_name(level), __func__, __LINE__, ##args);}}
//#define blog(level, fmt, args...)
#endif
#define log_buffer(prompt, buf, len) { if(g_loglv >= LOG_DEBUG) { blog(LOG_DEBUG, "%s, buf:%p, len:%d\n", prompt, buf, len); __log_buffer(buf, len);} }



#ifndef __MINGW32__
#define BHU_INVALID_SOCKET      -1
#define bhu_socket_errno        errno
#define BHU_EAGAIN              EAGAIN
#define BHU_EINPROGRESS         EINPROGRESS
#define INLINE                  inline
typedef int						bhu_err_t;
#else
#define BHU_INVALID_SOCKET      INVALID_SOCKET
#define bhu_socket_errno        WSAGetLastError()
#define BHU_EAGAIN              WSAEWOULDBLOCK
#define BHU_EINPROGRESS         WSAEINPROGRESS
#define INLINE                  __inline
typedef DWORD					bhu_err_t;
#endif


typedef struct bhu_pkt_buf {
    char          *head;      /* the real start of allocated buf address */
    char          *tail;      /* the end of allocated buf address */
    char          *start;     /* the start of data */
    char          *pos;       /* current pos of data */
} bhu_pkt_buf_t;


typedef struct {
    struct list_head node;
	bhu_pkt_buf_t    pkt;
    struct timeval   tm;
#ifdef BHU_CLOUD
    u8  sent_flag;
#endif
} bhu_str_t;




const char* 
get_loglevel_name(int loglv);
int
blog_init();
void 
__blog(int level,const char *fmt,...);

void
__log_buffer(const char *buf, int len);


char *
set_log(char *cmd);




const char *
bhu_errormsg(bhu_err_t err);

void
tc_strtrim(char *str);

char *
tc_mac2str (const u8 *mac, char *buf, char delim);

u8 *
tc_str2mac (const char *str, u8 *buf);

u32
tc_str2ip (const char *str);



void 
bhu_reset_pkt_buf(bhu_str_t *str, size_t reserved);
int
bhu_head_extend(bhu_str_t *str, int s);
int
bhu_verify_available_room(bhu_str_t *str, int s);
bhu_str_t *
bhu_str_get_new(int reserved);
bhu_str_t *
bhu_str_duplicate(bhu_str_t *str, int dup_len, int reserved);
void
bhu_str_release(bhu_str_t *str);



int
get_timestamp(struct timeval *now);

int 
timestamp_diff_sec(struct timeval *t1, struct timeval *t2);
void
call_system(char *cmd);
char *
file_to_buf(char *path, long *len);
int 
append_buf_to_file(char *file, char *buf);

char *random_uuid( char buf[37] );
char *
file_get_cmd_ex (const char *cmd);

extern int terminate;

#endif // _COMM_H
