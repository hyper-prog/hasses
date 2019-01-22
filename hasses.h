/* Asynchronous SSE Server
 * Author: Peter Deak (hyper80@gmail.com)
 * License: GPL
 */
#ifndef HASSES_HASSES_H
#define HASSES_HASSES_H

#define VERSION "1.210"

/* The client timeout check interval in seconds */ 
#define CLIENT_CHK_TIME_INTERVAL  60

/* A connected handshaked sse client connection will be dropped after this seconds */
#define CLIENT_HANDSHAKED_TIMEOUT   3600

/* A not fully handshaked so not sse client connection will be dropped after this seconds */
#define CLIENT_NOTHSHAKED_TIMEOUT   20

/* Client database bank size. (Client array size in one list element */
#define BANKSIZE         256

/* Epoll_create() parameter, maximum watched descriptor. */
#define MAXEVENTS        10000

/* Maximum reading message size, from network side. (Url req, etc) */
#define MAX_READ_SIZE    6144

struct Hasses_Settings
{
    int  loglevel;
    int  use_ssl;
    int  reinit_allowed;
    int  paramuid;
    int  nodaemon;
    char match_url[64];
    char fifofile[128];
    char logfile[128];
    char pidfile[128];
    char paramuser[64];
    char certfile[128];
    char pkeyfile[128];
    char corsbase[128];
};

struct Hasses_Statistics
{
    time_t startDaemon;
    unsigned long maxclients;
    unsigned long allclient;
    unsigned long allreinit;
    unsigned long allmessage;
    unsigned long allsmessage;
};

void toLog(int level, const char * format, ...);
void checkTimeouts(void);
int close_client(int d);
int commands(char *input);
int get_reinit_allowed(void);
void diffsec_to_str(int diff_sec,char *buffer,int max);
void beforeExit(void);
void parse_comm_messages(char *fms);
void parse_comm_message(char *fm);

struct CommCli {
    int fd;
    struct CommCli *next;
};

void commclient_add(int fd);
void commclient_del(int fd);
int  commclient_check(int fd);
void commclient_debug(void);
#endif
