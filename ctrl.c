#include "comm.h"
#include "ctrl.h"

typedef char *(* CTRL_HANDLER)(int fd, char *buf);

#define MAX_BUF 1024

struct sock_pending {
    int sock;
    ELOOP_TIMER_ID  timer;
    int send_len;
    int sent_len;
    bhu_str_t buf;
};      


char *
ctrl_status(int fd, char *request)
{
   return NULL; 
}


char *
ctrl_cmd_func(int fd, char *request)
{
    char *reply = NULL;
    if (strncmp(request, "status", 6) == 0) {
        reply = ctrl_status(fd, request);
    } else if (strncmp(request, "log", 3) == 0) {
        reply = set_log((request + 4));
    } else {
        reply = strdup("Invalid Command\n");
    }
    return reply;
} 

    

void 
ctrl_free_sock_pending(struct sock_pending *p){
    if(p){
        if(p->buf.pkt.head){
            free(p->buf.pkt.head);
            p->buf.pkt.head = NULL;
        }
        free(p);
    }
}



void    
ctrl_sock_write_timeout(void *arg)
{       
    struct sock_pending *p = (struct sock_pending *)arg;
    blog(LOG_DEBUG, "write timeout for sock %d\n", p->sock);

    if(p->timer){
        if(eloop_timer_del(p->timer))
            blog(LOG_DEBUG, "remove sock write from eloop failed\n");
        p->timer = NULL;
    }
    if(eloop_event_del(p->sock, ELOOP_EVENT_FLAG_WRITE)){
        blog(LOG_DEBUG, "remove sock write from eloop failed\n");
    }
    shutdown(p->sock, 2);
    closesocket(p->sock);
    ctrl_free_sock_pending(p);
    return;
}       
  
void                
ctrl_sock_writer(void *arg)     
{                               
    struct sock_pending *p = (struct sock_pending *)arg;
    int len = 0;        
    blog(LOG_DEBUG, "fd:%d", p->sock);
    while(p->sent_len < p->send_len){
        len = send(p->sock, p->buf.pkt.start + p->sent_len, p->send_len - p->sent_len, 0);
        if(len > 0) 
            p->sent_len += len;
        else if(len == -1){
            if(errno == EAGAIN)
                return;
            goto out;
        } else {
            goto out;
        }
    }
out:    
    if(p->timer){
        if(eloop_timer_del(p->timer))
            blog(LOG_ERR, "remove timer from eloop failed");
        p->timer = NULL;
    }   
    if(eloop_event_del(p->sock, ELOOP_EVENT_FLAG_WRITE)){
        blog(LOG_ERR, "remove sock write from eloop failed");
    }
    shutdown(p->sock, 2);
    closesocket(p->sock);
    ctrl_free_sock_pending(p);
    return;
}



void
ctrl_sock_reader(void *arg)
{
    /* read and reply */
    int done, i;
    char request[MAX_BUF];
    ssize_t read_bytes, len;
    int fd = (intptr_t)arg;
    char *reply = NULL;
    int send_len = 0;
    int sent_len = 0;
    struct sock_pending *p = NULL;

    /* Init variables */
    read_bytes = 0;
    done = 0;           
    memset(request, 0, sizeof(request));

    if(eloop_event_del(fd, ELOOP_EVENT_FLAG_READ)){
        blog(LOG_ERR, "remove sock read from eloop failed");
    }

    /* Read.... */  
    while (!done && read_bytes < (sizeof(request) - 1)) {
        len = recv(fd, request + read_bytes, sizeof(request) - read_bytes, 0);
        if(len <= 0){
            blog(LOG_ERR, "read error, fd:%d, %s!", fd, bhu_errormsg(bhu_socket_errno));
            goto out;
        }           

        /* Have we gotten a command yet? */
        for (i = read_bytes; i < (read_bytes + len); i++) {
            if (request[i] == '\r' || request[i] == '\n') {
                request[i] = '\0';
                done = 1;
            }       
        }                       

        /* Increment position */
        read_bytes += len;
    }               

    reply = ctrl_cmd_func(fd, request);

    if((intptr_t)reply == 1){
        //hdr_func will handler write 
        return ;
    }

    sock_nonblocking(fd);

    if(reply){
        send_len = strlen(reply);

        while(sent_len < send_len){
            len = send(fd, reply + sent_len, send_len - sent_len, 0);
            if(len > 0)
                sent_len += len;
            else if(len == -1){
                if(errno == EAGAIN){
                    /* need to re-write later */
                    if((p = malloc(sizeof(*p))) == NULL){
                        blog(LOG_ERR, "out of memory");
                        goto out;
                    }
                    memset(p, 0, sizeof(*p));
                    p->sock = fd;
                    p->send_len = send_len;
                    p->sent_len = sent_len;
                    p->buf.pkt.head = reply;
                    p->buf.pkt.tail = reply + send_len;
                    p->buf.pkt.start = reply;
                    p->buf.pkt.pos = reply + send_len;
                    reply = NULL;

                    if(eloop_event_add (fd,
                                ctrl_sock_writer, p,
                                ELOOP_EVENT_FLAG_WRITE)){
                        blog(LOG_ERR, "add sock for write failed");
                        goto out;
                    }

                    if((p->timer = eloop_timer_add_sec(0, 30*5, ctrl_sock_write_timeout, p)) == NULL){
                        if(eloop_event_del(fd, ELOOP_EVENT_FLAG_WRITE)){
                            blog(LOG_ERR, "remove sock write from eloop failed");
                        }
                        blog(LOG_ERR, "failed to initilize pending write");
                        goto out;
                    }
                    return;
                } else {
                    goto out;
                }
            } else { /* len = 0 */
                goto out;
            }
        }
    }


    if (!done) {
        blog(LOG_ERR, "Invalid ctrl request.");
    }

out:
    if(reply)
        free(reply);
    shutdown(fd, 2);
    closesocket(fd);
    ctrl_free_sock_pending(p);
    return;
}





void    
ctrl_listen_reader(void *arg)
{   
    int sock = (intptr_t)arg;
    int client_sock; 

    blog(LOG_DEBUG, "fd:%d", sock);

    if((client_sock = accept(sock, NULL, NULL)) == BHU_INVALID_SOCKET){
        blog(LOG_ERR, "accept error");
        return;
    }   

    blog(LOG_DEBUG, "accepted sock:%d", client_sock);

    if(eloop_event_add (client_sock,
                ctrl_sock_reader, (void*)(intptr_t)client_sock,
                ELOOP_EVENT_FLAG_READ)){
        blog(LOG_ERR, "%s, %d, add accepted sock to eloop failed\n", __func__, __LINE__);
    }
    return;
}


int         
ctrl_init(struct app_config *cfg)
{
    int sock;
    struct sockaddr_in sin;

    memset(&sin, 0, sizeof(sin));

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == BHU_INVALID_SOCKET){
        blog(LOG_ERR, "socket, err:%s", strerror(bhu_socket_errno));
        return -1;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = cfg->ctrl_port;
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    sock_reuse(sock);

    if (bind(sock, (struct sockaddr *)&sin, sizeof(struct sockaddr))) {
        blog(LOG_ERR, "Could not bind control socket: %s", strerror(bhu_socket_errno));
        goto fail;
    }

    if (listen(sock, 5)) {
        blog(LOG_ERR, "Could not listen on control socket: %s", strerror(bhu_socket_errno));
        goto fail;
    }

    if(eloop_event_add(sock,
                ctrl_listen_reader, (void*)(intptr_t)sock,
                ELOOP_EVENT_FLAG_READ)){
        blog(LOG_ERR, "add listen sock to eloop failed\n");
        goto fail;
    }

    blog(LOG_ERR, "ctrl listen sock on port:%d", ntohs(cfg->ctrl_port));

    return 0;
fail:
    if(sock != BHU_INVALID_SOCKET)
        closesocket(sock);
    return -1;
}

