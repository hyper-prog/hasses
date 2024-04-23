/* Asynchronous SSE Server
 * Author: Peter Deak (hyper80@gmail.com)
 * License: GPL
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <unistd.h>

#include "cdata.h"
#include "hasses.h"
#include "cio.h"

struct CliBank
{
    struct CliConn *clis;
    int next;
    int max;
    struct CliBank *n;
};

struct CliCursor
{
    struct CliBank *bank;
    int current;
};

static const char client_status_name[][5] = { "UNK", "NEW", "SSL", "HEA", "SSE", "END" };
struct CliBank *bFirst = NULL; //Fist CliConn bank
struct CliBank *bCurr  = NULL; //Current CliConn bank

struct CliCursor cursor;

int client_init(void)
{
    bFirst = (struct CliBank*)malloc(sizeof(struct CliBank));
    bFirst->clis = (struct CliConn*)malloc(sizeof(struct CliConn) * BANKSIZE);
    bFirst->next = 0;
    bFirst->max = BANKSIZE-1;
    bFirst->n = NULL;
    bCurr = bFirst;
    client_start();
    return 0;
}

int client_start(void)
{
    cursor.bank = bFirst;
    cursor.current = -1;
    return 0;
}

struct CliConn * client_current(void)
{
    if(cursor.current < 0)
        return NULL;
    if(cursor.current >= cursor.bank->next)
        return NULL;
    return cursor.bank->clis + cursor.current;
}

struct CliConn * client_next(void)
{
    ++(cursor.current);
    if(cursor.current >= cursor.bank->next)
    {
        if(cursor.bank->n == NULL)
            return NULL;
        cursor.bank = cursor.bank->n;
        if(cursor.bank->next == 0)
            return NULL;
        cursor.current = 0;
    }
    return cursor.bank->clis + cursor.current;
}

struct CliConn * client_get(int descr)
{
    int i;
    struct CliBank *b = bFirst;
    while(b != NULL)
    {
        for(i=0;i<b->next;++i)
        {
            if(b->clis[i].descr == descr)
            {
                cursor.bank = b;
                cursor.current = i;
                return b->clis + i;
            }
        }
        b = b->n;
    }
    client_start();
    return NULL;
}

struct CliConn * client_add(int descr)
{
    struct CliConn *newitem;
    if(bCurr->next <= bCurr->max)
    {
        newitem = bCurr->clis + bCurr->next;
        ++bCurr->next;
    }
    else
    {
        //Allocate a new bank
        bCurr->n = (struct CliBank*)malloc(sizeof(struct CliBank));
        bCurr->n->clis = (struct CliConn*)malloc(sizeof(struct CliConn) * BANKSIZE);
        bCurr->n->next = 1;
        bCurr->n->max = BANKSIZE-1;
        bCurr->n->n = NULL;
        bCurr = bCurr->n;
        newitem = bCurr->clis;
    }

    newitem->descr = descr;
    newitem->cio = NULL;
    newitem->err = 0;
    newitem->created = time(NULL);
    newitem->firstc = newitem->created;
    newitem->status = STATUS_UNKNOWN;
    newitem->allsubs = 0;
    newitem->subs = NULL;
    newitem->message = 0;
    newitem->reinit = 0;
    h_strlcpy(newitem->info,"",64);
    h_strlcpy(newitem->uniq_id,"",64);
    h_strlcpy(newitem->agent,"",192);
    cursor.bank = bCurr;
    cursor.current = bCurr->next - 1;
    return newitem;
}

int client_del(int descr)
{
    struct CliConn *toDelete;
    toDelete = client_get(descr);
    if(toDelete == NULL) //Can't find the item
        return 1;

    //Clear subsribed tokens to free memory
    client_subscribe_clear(toDelete);

    //Find the last item, and copy to the toDelete
    memcpy(toDelete,bCurr->clis + (bCurr->next - 1),sizeof(struct CliConn));
    if(bCurr->next > 0)
        --bCurr->next;

    //If we deleted the last item in a bank need to delete the last (now enpty) bank
    if(bCurr->next == 0 && bCurr != bFirst)
    {
        struct CliBank *b = bFirst;
        while(b != NULL)
        {
            if(b->n == bCurr)
            {
                if(cursor.bank == bCurr)
                {
                    cursor.bank = b;
                    cursor.current = cursor.bank->next - 1;
                }
                free(b->n);
                b->n = NULL;
                bCurr = b;
                break;
            }
            b = b->n;
        }
    }
    //Cursor leaved at toDelete which set by client_get.
    return 0;
}

int client_count(void)
{
    int c=0;
    struct CliBank *b = bFirst;
    while(b != NULL)
    {
        c += b->next;
        b = b->n;
    }
    return c;
}

int sub_count(char *sub_to_search)
{
    toLog(0,sub_to_search);

    int i=0;
    client_start();
    while(client_next()) {

        struct CliSubsribe *sub = client_current()->subs;
        while(sub != NULL) {

            if(strcmp(sub->token, sub_to_search) == 0) {

                ++i;
                break;
            }

            sub = sub->next;
        }
    }

    return i;
}

int client_count_commstate(void)
{
    int i=0;
    client_start();
    while(client_next())
        if(client_current()->status == STATUS_COMM)
            ++i;
    return i;
}

void client_list(int level)
{
    char sbuf[512];
    char ciobuf[512];
    char dt1[64];
    char dt2[64];
    time_t now=time(NULL);

    int i=1;
    //toLog(level,"List of clients:\n");
    struct CliConn *c;
    client_start();
    while((c=client_next()) != NULL)
    {
        client_subscribe_list(c,sbuf,512);

        diffsec_to_str(now - c->created,dt1,64);
        diffsec_to_str(now - c->firstc,dt2,64);
      
        toLog(level,"#%d - <%d> info: %s S:%s Err: %d Messages: %d Reinit: %d SSL:%s\n",
                        i,c->descr,c->info,client_status_name[c->status],c->err,c->message,c->reinit,
                        (c->cio == NULL?"No":"Yes"));
        toLog(level,"  Connection time: %s  Handshaked time: %s UniqId:%s\n",dt2,dt1,c->uniq_id);
        if(strlen(c->agent) > 0)
            toLog(level,"  Agent: %s\n",c->agent);
        if(cio_info_text(c,ciobuf,512))
            toLog(level,"  %s\n",ciobuf);
        toLog(level,"  Subscribes: %s\n",sbuf);

        ++i;
    }
}

void client_subscribe_list(struct CliConn *cli,char *sbuf,int max)
{
    int tl;
    int left = max;
    char *br = sbuf;
    struct CliSubsribe *s = cli->subs;
    sbuf[0] = '\0';
    if(cli->allsubs)
    {
        h_strlcpy(br,"*",left);
        --left;
        ++br;
    }
    while(s != NULL)
    {
        if(left <= 0)
            return;
        if(sbuf[0] != '\0')
        {
            h_strlcpy(br,",",left);
            --left;
            ++br;
        }
        tl = strlen(s->token);
        h_strlcpy(br,s->token,left);
        br += tl;
        left -= tl;
        s = s->next;
    }
}

void client_subscribe_add(struct CliConn *cli,char *ss)
{
    struct CliSubsribe *s,*ns;

    if(strcmp(ss,"*") == 0)
    {
        cli->allsubs = 1;
        return;
    }
    ns = (struct CliSubsribe *)malloc(sizeof(struct CliSubsribe));
    ns->next = NULL;
    h_strlcpy(ns->token,ss,31);

    if(cli->subs == NULL)
        cli->subs = ns;
    else
    {
        s = cli->subs;
        while(s->next != NULL)
            s = s->next;
        s->next = ns;
    }
}

int client_subscribe_exists(struct CliConn *cli,char *ss,int mmatch,char *rejectId)
{
    struct CliSubsribe *s;

    if(strcmp(rejectId,"") != 0 && strcmp(rejectId,cli->uniq_id) == 0)
        return 0;

    if(cli->allsubs || strcmp(ss,"*") == 0)
        return 1;

    s = cli->subs;
    while(s != NULL)
    {
        if(strncmp(s->token,ss,mmatch) == 0)
            return 1;
        s = s->next;
    }
    return 0;
}

void client_subscribe_clear(struct CliConn *cli)
{
    struct CliSubsribe *s,*del;

    cli->allsubs = 0;
    s = cli->subs;
    while(s != NULL)
    {
        del = s;
        s = s->next;
        free(del);
    }
    cli->subs = NULL;
}

//end.
