#include "comm.h"
#include "ctrl.h"

#define COMMAND_MAX_LENGTH 1024

struct app_config *cfg = NULL;
int terminate = 0;


typedef struct { 
    int     port; 
    int     command;
    char    *param;
} s_config;

static s_config config;

static void init_config(void);
static void parse_commandline(int, char **);
static int send_request(int, char *);
static void appctl_print(char *);
static void appctl_status(void);
#ifndef __MINGW32__
static int connect_to_client(int);
#endif // __MINGW32__

static void usage(const char *exename)
{
    printf("Usage: exename [options] command [arguments]\n");
    printf("\n");
    printf("options:\n");
    printf("  -p <port>            Port to the socket\n");
    printf("commands:\n");
    printf("  status               View the status of client\n");
    printf("  log <loglevel|logdst>         Set log level(number) or log dst(file,console"
    #ifndef __MINGW32__
    ",syslog"
    #endif
    ")\n");
    printf("BHU Build Version:%s\n", BHU_SVN_NO);
}

static void init_config(void)
{
    config.port = atoi(APPCTRL_DEFAULT_PORT);
    config.command = APPCTL_UNDEF;
    config.param = "";
}

static void parse_commandline(int argc, char **argv)
{
    extern int optind;
    int c, i;

    while (-1 != (c = getopt(argc, argv, "vVt:p:h"))) {
        switch(c) {
        case 'v':
        case 'V':
        case 'h':
            usage(argv[0]);
            exit(1);
            break;
        case 'p':
            config.port = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            exit(1);
            break;
        }
    }

    if ((argc - optind) <= 0) {
        usage(argv[0]);
        exit(1);
    }

    if (strcmp(*(argv + optind), "status") == 0) {
        config.command = APPCTL_STATUS;
    } else if(strcmp(*(argv + optind), "log") == 0) {
        config.command = APPCTL_LOG;
        config.param = malloc(COMMAND_MAX_LENGTH-4);
        if (NULL == config.param){
            blog(LOG_ERR, "malloc() Error");
            exit(1);
        }
        memset(config.param, 0, sizeof(COMMAND_MAX_LENGTH-4));
        for (i=optind+1; i<argc; i++){
            strcat(config.param, argv[i]);
            strcat(config.param, " ");
        }
    }else{
        usage(argv[0]);
        exit(1);
    }
}

#ifndef __MINGW32__
static int connect_to_client(
#if 1
int port
#else
char *sock_name
#endif
)
{
    int sock;
	struct sockaddr_in sin;

	/* Connect to socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin))){
		fprintf(stderr, "appctl: App Tunnel probably not started (Error: %s)\n", bhu_errormsg(bhu_socket_errno));
		exit(1);
	}
    return sock;
}
#endif // __MINGW32__

static int send_request(int sock, char *request)
{
    ssize_t    len, written;

    len = 0;
    while (len != strlen(request)) {
        written = send(sock, (request + len), strlen(request) - len, 0);
        if (written == -1) {
            fprintf(stderr, "Write to sock failed: %s\n", bhu_errormsg(bhu_socket_errno));
            exit(1);
        }
        len += written;
    }

    return((int)len);
}

/* Perform a appctl action, printing to stdout the client response.
 *  Action given by cmd.
 */
static void appctl_print(char *cmd)
{
    int i;
    int sock;
    char buffer[4096];
    char request[128];
    int len;

#ifdef __MINGW32__
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(config.port);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");

    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2,2), &wsadata) == SOCKET_ERROR) {
        blog(LOG_ERR, "WSAStartup() Error");
        exit(1);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0){
        blog(LOG_ERR, "socket() Error");
        exit(1);
    }
    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin))){
        fprintf(stderr, "appctl: BHU Tunnel probably not started (Error: %s)\n", bhu_errormsg(bhu_socket_errno));
        exit(1);
    }
#else
    sock = connect_to_client(config.port);
#endif // __MINGW32__

    snprintf(request, sizeof(request)-strlen(APPCTL_TERMINATOR), "%s", cmd);
    strcat(request, APPCTL_TERMINATOR);

    len = send_request(sock, request);
    while ((len = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        for (i=0; i<len; i++){
            if ('\0' == buffer[i]){
                while ('\0' == buffer[i]){
                    i++;
                }
                printf("\n");
            }
            if (i >= len){
                continue;
            }
            printf("%c", buffer[i]);
        }
    }

    if(len<0) {
        fprintf(stderr, "appctl: Error reading socket: %s\n", bhu_errormsg(bhu_socket_errno));
    }

    shutdown(sock, 2);
    closesocket(sock);
#ifdef __MINGW32__
    WSACleanup();
#endif // __MINGW32__
}

static void appctl_status(void)
{
    char cmd[COMMAND_MAX_LENGTH] = "status ";
    strcat(cmd, config.param);
    appctl_print(cmd);
}


static void appctl_log(void)
{
    char cmd[COMMAND_MAX_LENGTH] = "log ";
    strcat(cmd, config.param);
    appctl_print(cmd);
}

int main(int argc, char **argv)
{
    /* Init configuration */
    init_config();
    parse_commandline(argc, argv);

    switch(config.command) {
    case APPCTL_STATUS:
        appctl_status();
        break;
    case APPCTL_LOG:
        appctl_log();
        break;

    default:
        /* XXX NEVER REACHED */
        fprintf(stderr, "Unknown opcode: %d\n", config.command);
        exit(1);
        break;
    }
    exit(0);
}
