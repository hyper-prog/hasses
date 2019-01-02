/* Asynchronous SSE Server
 * Author: Peter Deak (hyper80@gmail.com)
 * License: GPL
 */
#ifndef HASSES_CDATA_H
#define HASSES_CDATA_H

#include <time.h>

#define STATUS_UNKNOWN 0
#define STATUS_NEW     1 //New connection, no communication yet
#define STATUS_SSLACPT 2 //Under ssl accept SSL_accept() needs read or write
#define STATUS_HEADER  3 //Processing http header parts, no special encoding yet
#define STATUS_COMM    4 //Http handshake done, http chunked encoding
#define STATUS_END     5 //Communication terminated

struct CliSubsribe
{
    char token[32];
    struct CliSubsribe *next;
};

struct CliConn
{
    int    descr;     //socket desctiptor
    void   *cio;      //cio stuff

    char   info[30];  //ip & port
    char   agent[192]; //user agent
    char   err;       //Error status
    time_t created;   //Creation time (reset on reinit) TTL wokrs on this.
    time_t firstc;    //First connection time
    int    status;    //Connection status
    struct CliSubsribe *subs; //Tokens of subscribes
    int    allsubs;   //Listening on all token if this 1
    int    message;   //message count
    int    reinit;    //reinit count
    char   uniq_id[64]; //unique client id (optional) Can be set by parameter "id"
};

int client_init(void);

int client_start(void);
struct CliConn * client_current(void);
struct CliConn * client_next(void);

struct CliConn * client_get(int descr);

struct CliConn * client_add(int descr);
int client_del(int descr);

int client_count(void);
int client_count_commstate(void);
void client_list(int level);

void client_subscribe_add   (struct CliConn *cli,char *ss);
int  client_subscribe_exists(struct CliConn *cli,char *ss,int mmatch,char *rejectId);
void client_subscribe_clear (struct CliConn *cli);
void client_subscribe_list  (struct CliConn *cli,char *sbuf,int max);

#endif
//end code.
