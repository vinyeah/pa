#include "comm.h"


#define MAX_BUF 1024

#ifdef __MINGW32__
static HANDLE g_hMutex = INVALID_HANDLE_VALUE;
#endif

u8 g_mode;
int g_loglv = LOG_DEBUG;
int g_logdst = LOG_TO_CONSOLE;
//int g_logdst = LOG_TO_FILE;
//int g_loglv = LOG_ERR;

time_t __tt;
struct tm * __tm;
char __tstr[32];

static const char* log_dst_name[] = {
    "file",
    "console",
#ifndef __MINGW32__
    "syslog",
#endif
    NULL,
};

struct loglv_map_t {
    int lv;
    char name[16];
};

static const struct loglv_map_t loglv_map[] = {
    {LOG_DEBUG, "LOG_DEBUG"},
    {LOG_INFO, "LOG_INFO"},
    {LOG_NOTICE, "LOG_NOTICE"},
    {LOG_WARNING, "LOG_WARNING"},
    {LOG_ERR, "LOG_ERR"},
    {LOG_CRIT, "LOG_CRIT"},
    {LOG_ALERT, "LOG_ALERT"},
    {LOG_EMERG, "LOG_EMERG"},
};



const char*                     
get_logdst_name(int logdst)
{
    if(logdst < 0 || logdst >= LOG_DST_MAX)
        return "unknow log dst";
    return log_dst_name[logdst];
}

const char*
get_loglevel_name(int loglv)
{
    int i = 0;
    for(i = 0; i < sizeof(loglv_map)/(sizeof(struct loglv_map_t)); i ++){
        if(loglv_map[i].lv == loglv)
            return loglv_map[i].name;
    }
    return "unknow log level";
}



int
get_timestamp(struct timeval *now)
{
#ifdef __MINGW32__
    struct timeval ts;
#else
    struct timespec ts;
#endif
    clock_gettime(CLOCK_MONOTONIC, &ts);

    now->tv_sec = ts.tv_sec;
#ifdef __MINGW32__
    now->tv_usec = ts.tv_usec;
#else
    now->tv_usec = ts.tv_nsec/1000;
#endif
    return 0;
}

int 
timestamp_diff_sec(struct timeval *t1, struct timeval *t2) 
{
    return ((t1->tv_sec - t2->tv_sec)*1000*1000 + (t1->tv_usec - t2->tv_usec))/(1000*1000);
}


void
sock_reuse(int fd)
{
    int reuseaddr=-1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                (const void *) &reuseaddr, sizeof(int)) < 0){
                blog(LOG_ERR, "set sock:%d resuse fail", fd);
    }
}

const char *
bhu_errormsg(bhu_err_t err)
{
#ifdef __MINGW32__
	    u_int          len;
	    static u_long  lang = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
	    static char err_buf[1024];

	    len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
	                        NULL, err, lang, (char *)err_buf, sizeof(err_buf) - 1, NULL);

	    if (len == 0 && lang && GetLastError() == ERROR_RESOURCE_LANG_NOT_FOUND) {
	        lang = 0;
	        len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
	                            NULL, err, lang, (char *)err_buf, sizeof(err_buf) - 1, NULL);
	    }

	    if (len == 0) {
	        sprintf(err_buf, "FormatMessage() error:(%d)", GetLastError());
            return err_buf;
	    }

	    err_buf[len] = 0;
	    return err_buf;
#else
	return strerror(err);
#endif
}


#ifdef __MINGW32__
int
sock_nonblocking(int s)
{
    unsigned long  nb = 1;

    return ioctlsocket(s, FIONBIO, &nb);
}

#else // __MINGW32__

int closesocket(int fd)
{
    return close(fd);
}

int
sock_nonblocking(int s)
{
    return fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
}
#endif // __MINGW32__



#define init_buf(b, s) {if (b == NULL)b = malloc(s);if (b == NULL)return NULL;}

void
tc_strtrim(char *str)
{
    int i = 0;
    int j = 0;
    if(str){
        for(i = strlen(str) - 1; i >= 0; i --){
            if(str[i] == ' ' || str[i] == '\t')
                str[i] = 0;
            else 
            	break;
        }
        for(j = 0; j < i; j ++){
            if(str[j] == ' ' || str[j] == '\t')
                continue;
            else
                break;
        }
        if(j > 0){
            memmove(str, str + j, (i - j) + 1);
            str[i-j+1] = 0;
        }
    }
}

u32
tc_str2ip (const char *str)
{
    return ntohl(inet_addr(str));
}

char *
tc_mac2str (const u8 *mac, char *buf, char delim)
{
    char *format = NULL;
    init_buf(buf, 6*3);
    if(delim == ':')
        format = "%02X:%02X:%02X:%02X:%02X:%02X";
    else if(delim == '-')
        format = "%02X-%02X-%02X-%02X-%02X-%02X";
    else if(delim == 0)
        format = "%02X%02X%02X%02X%02X%02X";

    sprintf(buf, format, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

u8 *
tc_str2mac (const char *str, u8 *buf)
{
    unsigned int mac[6];
    int num1=0, num2=0, i=0;
    init_buf(buf, 6);
    memset(buf, 0, 6);
    for(i=0; str[i]!='\0'; i++) {
        if(str[i] == ':')
            num1 ++;
        if(str[i] == '-')
            num2 ++;
    }
    if(i == 12)
        i = sscanf(str, "%2x%2x%2x%2x%2x%2x",
                mac, mac+1, mac+2, mac+3, mac+4, mac+5);
    else if(num1 == 5)
        i = sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x",
                mac, mac+1, mac+2, mac+3, mac+4, mac+5);
    else if(num2 == 5)
        i = sscanf(str, "%2x-%2x-%2x-%2x-%2x-%2x",
                mac, mac+1, mac+2, mac+3, mac+4, mac+5);
    else
        return buf;

    if (i < 6)
        return NULL;

    *(uint8_t *)buf = mac[0];
    *(uint8_t *)(buf+1)  = mac[1];
    *(uint8_t *)(buf+2)  = mac[2];
    *(uint8_t *)(buf+3)  = mac[3];
    *(uint8_t *)(buf+4)  = mac[4];
    *(uint8_t *)(buf+5)  = mac[5];
    return buf;
}


void 
bhu_reset_pkt_buf(bhu_str_t *str, size_t reserved)
{
    size_t size = str->pkt.tail - str->pkt.head;

    str->pkt.start = str->pkt.head + ((reserved > size)?size:reserved);
    str->pkt.pos = str->pkt.start;
}

int
bhu_head_extend(bhu_str_t *str, int s)
{
    if((str->pkt.start - str->pkt.head) < s)
        return -1;
    str->pkt.start -= s;
    return 0;
}

int
bhu_verify_available_room(bhu_str_t *str, int s)
{
    int size = 0;
    int new_size = 0;
    int data_len = 0;
    int head_room = 0;
    u_char *data = NULL;

    if(str->pkt.tail - str->pkt.pos >=s)
        return 0;

    size = str->pkt.tail - str->pkt.head; 
    new_size = size * 2;
    while(new_size - (str->pkt.pos - str->pkt.head) < s)
        new_size *= 2;
    if((data = realloc(str->pkt.head, new_size)) == NULL)
        return -1;

    data_len = str->pkt.pos - str->pkt.start;
    head_room = str->pkt.start - str->pkt.head;

    str->pkt.head = data;
    str->pkt.tail = data + new_size;
    str->pkt.start = data + head_room;
    str->pkt.pos = str->pkt.start + data_len;
    
    return 0;
}


static bhu_str_t *
bhu_alloc_str(size_t len)
{
    bhu_str_t *str = NULL;
    u_char *data = NULL;

    if((str = malloc(sizeof(bhu_str_t))) == NULL)
        return NULL;

    memset(str, 0, sizeof(*str));

    if((data = malloc(len)) == NULL)
        goto err;

    str->pkt.head = data;
    str->pkt.tail = data + len;

    bhu_reset_pkt_buf(str, 0);

    INIT_LIST_HEAD(&str->node);

    return str;
err:
    free(str);
    return NULL;
}

bhu_str_t *
bhu_str_get_new(int reserved)
{
    bhu_str_t *str;
    if((str = bhu_alloc_str(BHU_DEFAULT_REFD_STR_LEN))){
        bhu_reset_pkt_buf(str, reserved);
    }
    return str;
}

bhu_str_t *
bhu_str_duplicate(bhu_str_t *str, int dup_len, int reserved)
{
    bhu_str_t *ret = NULL;
    int len = str->pkt.pos - str->pkt.start;

    if((ret = bhu_str_get_new(0)) == NULL)
        return NULL;

    len = (dup_len > len || dup_len <= 0)?len:dup_len;

    if(bhu_verify_available_room(ret, len + reserved))
    {   
        bhu_str_release(ret);
        return NULL;
    }   

    ret->pkt.start += reserved;
    ret->pkt.pos = ret->pkt.start + len;

    memcpy(ret->pkt.start, str->pkt.start, len);

    return ret;
}

void
bhu_str_release(bhu_str_t *str)
{
    if(str){
        if(!list_empty(&str->node))
            list_del_init(&str->node);
        if(str->pkt.head)
            free(str->pkt.head);
        free(str);
    }
}


void
bhu_str_list_free(struct list_head *list)
{
    struct list_head *pos, *n;
    bhu_str_t *str;

    list_for_each_safe(pos, n, list){
        str = list_entry(pos, bhu_str_t, node);
        list_del(&str->node);
        bhu_str_release(str);
    }
}


FILE *log_file = NULL;


void 
__blog(int level,const char *fmt,...)
{
        if(level > g_loglv)
                    return;

#ifdef __MINGW32__
        WaitForSingleObject(g_hMutex, INFINITE);
#endif
        //    if(level == LOG_DEBUG){
        va_list ap; 
        va_start(ap,fmt);
        if(g_logdst == LOG_TO_FILE){
            if(log_file){
                vfprintf(log_file, fmt,ap);
                fflush(log_file);
            } else {
                vprintf(fmt,ap);
            }   
        }   
#ifndef __MINGW32__
        else if(g_logdst == LOG_TO_SYSLOG){
            vsyslog(level, fmt, ap);
        }   
#endif
        else {
            vprintf(fmt,ap);
        }   
        va_end(ap);
        //   }    
#ifdef __MINGW32__
        ReleaseMutex(g_hMutex);
#endif
}
            


void
__log_buffer(const char *buf, int len)
{
    char tmp[256];
    int i = 0;
    int j = 0;
    char c = 0;
    int pos = 0;
    int ct = 0;
    memset(tmp, 0, sizeof(tmp));
    while (i < len) {
        for(j = 0, ct = 0; j < 16; j ++){
            if((i + ct) < len){
                pos += sprintf(tmp + pos, "%02x ", (unsigned char)*(buf + i + ct));
                ct ++; 
            }   
            else
                pos += sprintf(tmp + pos, "   ");

            if((j + 1) % 8 == 0)
                pos += sprintf(tmp + pos, "  ");
        }   
        pos += sprintf(tmp + pos, "  ");
        for(j = 0; j < 16; j ++){
            if(j < ct) 
                c = *(buf + i + j); 
            else 
                c = ' ';
            if(c <= 31 || c >= 127)
                c = '.';
            pos += sprintf(tmp + pos, "%c", c); 
        }   
        blog(LOG_DEBUG, "%s", tmp);
        pos = 0;
        i += ct; 
    }
}   

int
blog_init()     
{   
#ifdef __MINGW32__
    g_hMutex = CreateMutex(NULL, FALSE, "Mutex");
    if(!g_hMutex){
        printf("ERROR, failed to carete mutex\n");
    }   
#endif 
    log_file = fopen(
            "./apptunnel.log"
            , "a+");
    if(log_file == NULL){
        printf("ERROR, failed to open log file\n");
    }
    return 0;
} 

void
call_system(char *cmd)
{
    blog(LOG_DEBUG, "exec:[%s]", cmd);
    system(cmd);
}

char *
set_log(char *cmd)
{
    char buf[128] = {0};
    int lv = atoi(cmd);
    int i = 0;

    strncpy(buf, cmd, sizeof(buf) - 1);
    tc_strtrim(buf);
    if(strlen(buf) > 0){
        if(lv == 0 && strcmp(buf, "0") != 0){
            for(i = 0; i < LOG_DST_MAX; i ++){
                if(log_dst_name[i] == NULL)
                    continue;
                if(strcasecmp(buf, log_dst_name[i]) == 0){
                    g_logdst = i;
                    break;
                }
            }
        } else {
            g_loglv = lv;
            if(g_loglv < LOG_EMERG)
                g_loglv = LOG_EMERG;
            if(g_loglv >= LOG_DEBUG)
                g_loglv = LOG_DEBUG;
        }
    }
    sprintf(buf, "log level switch to: [%s][%s]\n", get_logdst_name(g_logdst), get_loglevel_name(g_loglv));
    return strdup(buf);
}

int
buf_to_file(char *fname, char *buf, int len)
{
    FILE *f = NULL;

    if(!(f = fopen(fname, "w"))){
        blog(LOG_ERR, "failed to open file for wirte:%s", fname);
        return -1;
    }
    fwrite(buf, 1, len, f);
    fclose(f);
    return 0;
}


char *
file_to_buf(char *path, long *len_out)
{
#if 0
    long size = 1024;
    char *s = NULL;
    char *t = NULL;
    int readed = 0;
    int read = 0;
    FILE *f = fopen(path, "r");
    if(!f)
        return NULL;
    s = (char*)malloc(size);
    if(!s)
        goto fail;
    while(!feof(f)){
        if(size - readed < 1024){
            size *= 2;
            if(!(t = realloc(s, size)))
                goto fail;
            s = t;
        }
        read = fread(s + readed, 1, size-readed-1, f);
        if(read == -1){
            goto fail;
        }
        readed += read;
    }
    s[readed] = 0;
    fclose(f);
    return s;
fail:
    if(s)
        free(s);
    if(f)
        fclose(f);
    return NULL;
#else
    FILE *f = fopen(path, "rb");
    long len = 0;
    char *s = NULL;
    if(!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(!(s = (char*)malloc(len + 1))){
        fclose(f);
        return NULL;
    }
    fread(s, 1, len, f);
    fclose(f);
    s[len] = 0;
    if(len_out)
        *len_out = len;
    return s;
#endif
}


int 
append_buf_to_file(char *file, char *buf)
{
    FILE *f = NULL;
    if(!(f = fopen(file, "a+")))
        return -1;
    fprintf(f, "%s\n", buf);
    fclose(f);
    return 0;
}

char *random_uuid( char buf[37] )
{
    const char *c = "89ab";
    char *p = buf;
    int n;
    for( n = 0; n < 16; ++n )
    {
        int b = rand()%255;
        switch( n )
        {
            case 6:
                sprintf(p, "4%x", b%15 );
                break;
            case 8:
                sprintf(p, "%c%x", c[rand()%strlen(c)], b%15 );
                break;
            default:
                sprintf(p, "%02x", b);
                break;
        }

        p += 2;
        switch( n )
        {
            case 3:
            case 5:
            case 7:
            case 9:
                *p++ = '-';
                break;
        }
    }
    *p = 0;
    return buf;
}



char *
file_get_cmd (const char *cmd)
{
#define CMD_BUF_START_SZ	128
    int ret, buf_sz=0, buf_st=0;
    FILE *file;
    char *buf = NULL;
    char *buf2;

    file = popen(cmd, "r");
    if (file == NULL)
        return NULL;

    while (!feof(file)) {
        if (buf == NULL) {
            buf = malloc(CMD_BUF_START_SZ);
            if (buf == NULL)
                goto err;
            buf_sz = CMD_BUF_START_SZ;
            buf_st = 0;
        } else {
            buf_sz *= 2;
            buf2 = realloc(buf, buf_sz);
            if (buf2 == NULL) {
                free(buf);
                buf = NULL;
                goto err;
            }
            buf = buf2;
        }
        ret = fread(buf+buf_st, 1, buf_sz-buf_st-1, file);
        buf_st += ret;
    }
    buf[buf_st] = 0;
err:
    pclose(file);

    return buf;
}


/* Compare to file_get_cmd, rm end character '\n'*/
char *
file_get_cmd_ex (const char *cmd)
{
	char *buf = file_get_cmd(cmd);
	if (buf != NULL) {
		if (buf[strlen(buf)-1] == '\n') {
			buf[strlen(buf)-1] = 0;
		}
	}
	return buf;
}
