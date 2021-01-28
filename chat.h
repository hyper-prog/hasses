/* Asynchronous SSE Server
 * Author: Peter Deak (hyper80@gmail.com)
 * License: GPL
 */
#ifndef HASSES_CHAT_H
#define HASSES_CHAT_H

struct CliConn;
struct Hasses_Settings;
struct Hasses_Statistics;

void chat_init(struct Hasses_Settings *se,struct Hasses_Statistics *st);

int chat_received(struct CliConn *client,char *message,const char *url_to_handle);
int chat_parseparam(struct CliConn *client,char *parameters);
int sendmessages(char *buf);
int chat_chunk_encoding(char *outbuffer,char *message);
void create_time_line(char *buffer);

int chat_send_handshake(struct CliConn *client);
int chat_send_badreq(struct CliConn *client);
int chat_send_notfound(struct CliConn *client);
int chat_send_notsupported(struct CliConn *client);
int chat_send_head(struct CliConn *client);
int chat_send_options(struct CliConn *client);

char *chop(char *str);
int get_pos(char *str,char c);
int emptyStr(char *str);
char* nextNotWhitespace(char *str);
int startWithStr(char *str,const char *pattern);

#endif
//end.
