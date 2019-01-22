/* Asynchronous SSE Server
 * Author: Peter Deak (hyper80@gmail.com)
 * License: GPL
 */

#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pwd.h>

#include "cdata.h"
#include "chat.h"
#include "hasses.h"
#include "cio.h"

/* Example chat:
-> GET /sse?subscribe=XXX HTTP/1.1
-> Host: 192.168.1.100:80
-> User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64; rv:34.0) Gecko/20100101 Firefox/34.0
-> Accept: text/event-stream
-> Accept-Language: en-US,en;q=0.5
-> DNT: 1
-> Referer: http://192.168.1.200/sse2/
-> Origin: http://192.168.1.200
-> Connection: keep-alive
-> Pragma: no-cache
-> Cache-Control: no-cache

<- HTTP/1.1 200 OK
<- Date: Wed, 14 Jan 2015 21:01:35 GMT
<- Server: Apache/2.2.22 (Debian)
<- X-Powered-By: PHP/5.4.35-0+deb7u2
<- Cache-Control: no-cache
<- Keep-Alive: timeout=5, max=100
<- Connection: Keep-Alive
<- Transfer-Encoding: chunked
<- Content-Type: text/event-stream
<- 
<- e8
<- id: 1421269313
<- data: Hello this is the first
*/

struct Hasses_Settings   hsettings;
struct Hasses_Statistics stats;

//other
char *input = NULL;
time_t last_cli_ttl_check = 0;
int epoll_descriptor;
time_t log_oldrawtime=1;
char   log_timebuf[80];

struct CommCli *commFirst = NULL; //First Communication client

void beforeExit(void)
{
    if(strlen(hsettings.pidfile) > 0)
        if(unlink(hsettings.pidfile) != 0)
            toLog(0,"Warning: Cannot delete pid file!\n");
}

void sigint_handler(int sig)
{
    beforeExit();
    toLog(0,"Received INT signal, Exiting...\n");
    exit(0);
}

int name_to_uid(char const *name)
{
    if(name==NULL || strlen(name) <= 0)
        return -1;
    struct passwd *pwd = getpwnam(name);
    if(pwd != NULL)
        return pwd->pw_uid;
    return -1;
}

int create_and_bind(int port)
{
    char portstr[10];
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd,optval=1;

    toLog(2,"Create/bind listening socket (%d)...\n",port);
    memset(&hints, 0, sizeof (struct addrinfo));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    snprintf(portstr,10,"%d",port);
    s = getaddrinfo (NULL,portstr, &hints, &result);
    if(s != 0)
    {
        toLog(0,"Error in getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        s = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));
        if(s != 0)
        {
            toLog(0,"Error, setsockopt() failure\n");
            return -1;
        }

        s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
        {
            /* We managed to bind successfully! */
            toLog(2,"Bind to port:%s <%d>\n",portstr,sfd);
            break;
        }
        close(sfd);
    }

    if (rp == NULL)
    {
        toLog(0,"Error, could not bind socket on port %d\n",port);
        return -1;
    }
    freeaddrinfo(result);
    toLog(2,"Socked created/binded.\n");
    return sfd;
}

int make_socket_non_blocking(int sfd)
{
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1)
    {
      toLog(0,"Error, fcntl() failure in make_socket_non_blocking (1)\n");
      return 1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1)
    {
        toLog(0,"Error, fcntl() in make_socket_non_blocking (2)\n");
        return 1;
    }
    return 0;
}

int close_client(int d)
{
    /* Closing the descriptor will make epoll remove it
       from the set of descriptors which are monitored.
       this below may unnecessary */
    epoll_ctl(epoll_descriptor,EPOLL_CTL_DEL,d,NULL);

    cio_client_close(client_get(d));
    close(d);

    //Remove from my list
    client_del(d);
    toLog(1,"Closed connection (Leaved clients: %d) <%d>\n",client_count(),d);
    return 0;
}

int close_communication_client(int d)
{
    /* Closing the descriptor will make epoll remove it
       from the set of descriptors which are monitored.
       this below may unnecessary */
    epoll_ctl(epoll_descriptor,EPOLL_CTL_DEL,d,NULL);

    close(d);
    commclient_del(d);
    toLog(2,"Closed communication connection <%d>\n",client_count(),d);
    return 0;
}

int printversion(void)
{
    printf("Hyper's async SSE (Server Sent Event) server\n"
           "Version: %s\n"
           "Compiled: %s\n"
           "Author: Peter Deak (hyper80@gmail.com)\n"
           "License: GPL\n",VERSION, __DATE__);
    return 0;
}

int printhelp(void)
{
    printf("Hyper's async SSE (Server Sent Event) server\n"
           "Usage:\n hasses -p=<SSE_PORT> -murl=<MATCHING_URL>\n"
                   "        [-cp=<COMM_PORT>] [-fifo=<FIFOFILE>]\n"
                   "        [-q|-debug] [-l=<LOGFILE>] [-pidfile=<PIDFILE>]\n"
                   "        [-ssl] [-cert-file=<PEMFILE>] [-privatekey-file=<KEYFILE>]\n"
                   "        [-cors-base=<URL>] [-ra] [-user=<USER>] [-nodaemon]\n"
           "Commands on communication channel or fifo file:\n"
           "  \"status\" - Print status/statistics to the log\n"
           "  \"clientlist\" - List clients to the log\n"
           "  \"loglevel_quiet\" - Set loglevel to minimal\n"
           "  \"loglevel_normal\" - Set loglevel to normal\n"
           "  \"loglevel_debug\" - Set loglevel to maximum\n"
           "  \"reinit_enable\" - Enable re-initialize opened connections\n"
           "  \"reinit_disable\" - Enable re-initialize opened connections\n"
           "  \"<token>=<message>\" - Send message to the subscribers of <token>\n"
           "  \"<token>=<message>;<token2>=<message2>\" - Send more messages\n"
           "  \"<token>-<rId>=<message>\" - Send message to the subscribers of <token> except <rId>\n"
           "  \"*=<message>;\" - Send message to all clients\n\n");
    return 0;
}

void attach_signal_handler(void)
{
    struct sigaction sa;

    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        toLog(0,"Error, Cannot set signal handler: sigaction() failure. Exiting...\n");
        beforeExit();
        exit(1);
    }
}

int create_open_fifo(const char *fifofile)
{
    int s;
    int fifo;

    toLog(2,"Create/open fifo file: %s\n",fifofile);
    //Create the FIFO (ignore the error if it already exists).
    if( mkfifo(fifofile, 0666 ) < 0 )
    {
        if(errno != EEXIST )
        {
            toLog(0,"Error creating fifo file:%s mkfifo() failure. Exiting...\n",fifofile);
            beforeExit();
            exit(1);
        }
    }

    fifo = open(fifofile,O_RDWR);
    if( fifo < 0 )
    {
        toLog(0,"Error opening fifo file:%s Exiting...\n",fifofile);
        beforeExit();
        exit(1);
    }

    toLog(2,"FIFO opened.\n" );

    s = make_socket_non_blocking (fifo);
    if(s != 0)
    {
        toLog(0,"Cannot setup fifo devide, Exiting...\n");
        beforeExit();
        exit(1);
    }

    toLog(2,"FIFO is non blocking.\n");
    return fifo;
}

int my_cio_high_read(struct CliConn *client,char *buffer)
{
    if(strlen(buffer) > 0)
    {
        toLog(2,"Received from %s <%d>:\n",client->info,client->descr);
        chat_received(client,buffer,hsettings.match_url);
    }
    return 0;
}

int my_cio_low_write(struct CliConn *client,char *buffer,int length)
{
    int s;
    if(client->err)
        return 1;

    s=write(client->descr,buffer,length);
    if(s != length)
    {
        client->err = 1;
        return 1;
    }
    return 0;
}

int main(int argi,char **argc)
{
    int s;
    int sfd = -1;
    int commfd = -1;

    int port = 0;
    int commport = 0;

    hsettings.loglevel = 1;
    hsettings.use_ssl = 0;
    hsettings.reinit_allowed = 0;
    hsettings.paramuid=-1;
    hsettings.nodaemon=0;

    strcpy(hsettings.pidfile,"");
    strcpy(hsettings.match_url,"");
    strcpy(hsettings.fifofile,"");
    strcpy(hsettings.logfile,"/var/log/hasses.log");
    strcpy(hsettings.paramuser,"");
    strcpy(hsettings.certfile,"");
    strcpy(hsettings.pkeyfile,"");
    strcpy(hsettings.corsbase,"*");

    stats.startDaemon = 0;
    stats.maxclients  = 0;
    stats.allclient   = 0;
    stats.allreinit   = 0;
    stats.allmessage  = 0;
    stats.allsmessage = 0;

    strcpy(log_timebuf,"error:");

    if(argi <= 1)
    {
        printhelp();
        return 0;
    }

    int p;
    for(p = 1 ; p < argi ; ++p)
    {
        if(!strcmp(argc[p],"-h") || !strcmp(argc[p],"-help"))
            return printhelp();

        if(!strcmp(argc[p],"-v") || !strcmp(argc[p],"-V") || !strcmp(argc[p],"-version"))
            return printversion();

        if(!strcmp(argc[p],"-q"))
        {
            hsettings.loglevel = 0;
            continue;
        }
        if(!strcmp(argc[p],"-debug"))
        {
            hsettings.loglevel = 2;
            continue;
        }

        if(!strcmp(argc[p],"-ra"))
        {
            hsettings.reinit_allowed = 1;
            continue;
        }

        if(!strcmp(argc[p],"-nodaemon"))
        {
            hsettings.nodaemon = 1;
            continue;
        }

        if(!strcmp(argc[p],"-ssl"))
        {
            hsettings.use_ssl = 1;
            continue;
        }

        if(!strncmp(argc[p],"-murl=",6))
        {
            strncpy(hsettings.match_url,argc[p]+6,64);
            continue;
        }

        if(!strncmp(argc[p],"-p=",3))
        {
            if(sscanf(argc[p]+3,"%d",&port) == 1 && port > 0)
                continue;
        }

        if(!strncmp(argc[p],"-cp=",4))
        {
            if(sscanf(argc[p]+4,"%d",&commport) == 1 && commport > 0)
                continue;
        }

        if(!strncmp(argc[p],"-l=",3))
        {
            strncpy(hsettings.logfile,argc[p]+3,128);
            continue;
        }

        if(!strncmp(argc[p],"-fifo=",6))
        {
            strncpy(hsettings.fifofile,argc[p]+6,128);
            continue;
        }

        if(!strncmp(argc[p],"-pidfile=",9))
        {
            strncpy(hsettings.pidfile,argc[p]+9,128);
            continue;
        }

        if(!strncmp(argc[p],"-cert-file=",11))
        {
            strncpy(hsettings.certfile,argc[p]+11,128);
            continue;
        }

        if(!strncmp(argc[p],"-privatekey-file=",17))
        {
            strncpy(hsettings.pkeyfile,argc[p]+17,128);
            continue;
        }

        if(!strncmp(argc[p],"-cors-base=",11))
        {
            strncpy(hsettings.corsbase,argc[p]+11,128);
            continue;
        }

        if(!strncmp(argc[p],"-user=",6))
        {
            strncpy(hsettings.paramuser,argc[p]+6,64);
            hsettings.paramuid = name_to_uid(hsettings.paramuser);
            if(hsettings.paramuid == -1)
            {
                fprintf(stderr,"Error unknown user/Cannot setuid to user: \"%s\"\n",hsettings.paramuser);
                exit(1);
            }
            continue;
        }

        if(!strncmp(argc[p],"-",1))
        {
            fprintf(stderr,"Error, unknown switch: \"%s\"\n",argc[p]);
            return 1;
        }
    }

    if(strlen(hsettings.match_url) == 0 ||
       (strlen(hsettings.fifofile) == 0 && commport == 0) || 
       port == 0 )
    {
        printhelp();
        return 0;
    }

    if(hsettings.logfile[0] != '/' || 
       (strlen(hsettings.fifofile) > 0 && hsettings.fifofile[0] != '/') ||
       (strlen(hsettings.pidfile) > 0 && hsettings.pidfile[0] != '/'))
    {
        fprintf(stderr,"WARNING: Use absolute path to specify fifo, log or pid files!\n");
        return 0;
    }

    stats.startDaemon = time(NULL);
    toLog(1,"\n=== daemon starting ===\n");
    toLog(1,"Pid: %d\n",getpid());
    if(hsettings.loglevel > 1)
    {
        toLog(2,"Parameters:\n");
        toLog(2," loglevel: %d\n",hsettings.loglevel);
        toLog(2," Match url: %s\n",hsettings.match_url);
        toLog(2," TCP Port (SSE): %d\n",port);
        if(commport > 0)
            toLog(2," TCP Port (Communication): %d\n",commport);
        toLog(2," Fifo file: %s\n",strlen(hsettings.fifofile) > 0 ? hsettings.fifofile : "-none-");
        toLog(2," Mode: %s\n",(hsettings.use_ssl?"SSL (https)":"Normal (http)"));
        if(hsettings.use_ssl)
        {
            toLog(2," SSL Cert key: %s\n",hsettings.certfile);
            toLog(2," SSL Prvt key: %s\n",hsettings.pkeyfile);
        }
        toLog(2," Log file: %s\n",hsettings.logfile);
        toLog(2," Pid file: %s\n",hsettings.pidfile);
        toLog(2," Set daemon user: %s\n",hsettings.paramuser);

        printf("Parameters:\n");
        printf(" loglevel: %d\n",hsettings.loglevel);
        printf(" Match url: %s\n",hsettings.match_url);
        printf(" TCP Port (SSE): %d\n",port);
        if(commport > 0)
            printf(" TCP Port (Communication): %d\n",commport);
        printf(" Fifo file: %s\n",strlen(hsettings.fifofile) > 0 ? hsettings.fifofile : "-none-");
        printf(" Mode: %s\n",(hsettings.use_ssl?"SSL (https)":"Normal (http)"));
        if(hsettings.use_ssl)
        {
            printf(" SSL Cert key: %s\n",hsettings.certfile);
            printf(" SSL Prvt key: %s\n",hsettings.pkeyfile);
        }
        printf(" Log file: %s\n",hsettings.logfile);
        printf(" Pid file: %s\n",hsettings.pidfile);
        printf(" Set daemon user: %s\n",hsettings.paramuser);
        fflush(stdout);
    }

    if(!hsettings.nodaemon)
    {
        if(hsettings.loglevel > 0)
            toLog(1,"Started, Entering daemon mode...\n");

        if(daemon(0,0) == -1)
        {
            toLog(0,"Error, daemon() failure, Exiting...\n");
            exit(1);
        }
        toLog(2,"Entered daemon mode.\n");
    }
    else
    {
        toLog(2,"\nWARNING: Daemon mode is disabled by -nodaemon switch!\n"
                "All messages writed to the standard output!\n"
                "THE HASSES DOES NOT USE THE LOG FILE!\n\n");
    }

    if(hsettings.paramuid >= 0)
    {
        if(setuid(hsettings.paramuid) == 0)
        {
            toLog(2,"SetUid done to uid: %d (%s)\n",
                hsettings.paramuid,hsettings.paramuser);
        }
        else
        {
            toLog(2,"SetUid done to uid: %d (%s) failed! Exiting...\n",
                hsettings.paramuid,hsettings.paramuser);
            exit(1);
        }
    }

    if(strlen(hsettings.pidfile) > 0)
    {
        FILE *pidf = NULL;
        if((pidf = fopen(hsettings.pidfile,"w")) != NULL)
        {
            fprintf(pidf,"%d",getpid());
            fclose(pidf);
        }
        else
            toLog(0,"Error, Cannot open pid file!\n");
    }

    attach_signal_handler();

    int fifo = -1;
    if(strlen(hsettings.fifofile) > 0)
        fifo = create_open_fifo(hsettings.fifofile);

    struct epoll_event event;
    struct epoll_event *events;

    input = (char *)malloc(sizeof(char) * MAX_READ_SIZE);

    cio_high_read_SET(my_cio_high_read);
    cio_low_write_SET(my_cio_low_write);

    client_init();
    chat_init(&hsettings,&stats);

    if(cio_init(hsettings.use_ssl, hsettings.certfile, hsettings.pkeyfile))
    {
        toLog(0,"Exiting due to previous error...\n");
        beforeExit();
        exit(1);
    }

    toLog(2,"Open SSE port to listen...\n");
    sfd = create_and_bind(port);
    if (sfd == -1)
    {
        toLog(0,"Exiting due to previous error...\n");
        beforeExit();
        exit(1);
    }
    s = make_socket_non_blocking(sfd);
    if(s == -1)
    {
        toLog(0,"Exiting due to previous error...\n");
        beforeExit();
        exit(1);
    }
    s = listen (sfd, SOMAXCONN);
    if(s == -1)
    {
        toLog(0,"Error, listen() failure. Exiting...\n");
        beforeExit();
        exit(1);
    }

    if(commport > 0)
    {
        toLog(2,"Open communication port to listen...\n");
        commfd = create_and_bind(commport);
        if (commfd == -1)
        {
            toLog(0,"Exiting due to previous error...\n");
            beforeExit();
            exit(1);
        }
        s = make_socket_non_blocking(commfd);
        if(s == -1)
        {
            toLog(0,"Exiting due to previous error...\n");
            beforeExit();
            exit(1);
        }
        s = listen (commfd, SOMAXCONN);
        if(s == -1)
        {
            toLog(0,"Error, listen() failure. Exiting...\n");
            beforeExit();
            exit(1);
        }
    }

    toLog(2,"Creating epoll...\n");
    epoll_descriptor = epoll_create1(0);
    if (epoll_descriptor == -1)
    {
        toLog(0,"Error, epoll_create1() failure, Exiting...\n");
        beforeExit();
        exit(1);
    }

    if(fifo >= 0)
    {
        event.data.fd = fifo;
        event.events = EPOLLIN | EPOLLET;
        s = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, fifo, &event);
        if(s == -1)
        {
            toLog(0,"Error, epoll_ctl() add fifo failure (1) Exiting...\n");
            beforeExit();
            exit(1);
        }
    }

    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;
    s = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, sfd, &event);
    if(s == -1)
    {
        toLog(0,"Error, epoll_ctl() add sse socket failure (2) Exiting...\n");
        beforeExit();
        exit(1);
    }

    if(commfd >= 0)
    {
        event.data.fd = commfd;
        event.events = EPOLLIN | EPOLLET;
        s = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, commfd, &event);
        if(s == -1)
        {
            toLog(0,"Error, epoll_ctl() add communication socket failure (3) Exiting...\n");
            beforeExit();
            exit(1);
        }
    }

    toLog(2,"Epoll created.\n");

    /* Buffer where events are returned */
    events = calloc (MAXEVENTS, sizeof event);

    /* The event loop */
    toLog(2,"Starting main event loop...\n");
    while(1)
    {
        int n,i;

        checkTimeouts();
        n = epoll_wait (epoll_descriptor, events, MAXEVENTS, -1);
        for (i = 0; i < n; i++)
        {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN)))
            {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */

                if(commclient_check(events[i].data.fd)) //communication client
                {
                    toLog(2,"Communication client HUP/ERR or gone, Closing...\n");
                    close_communication_client(events[i].data.fd);
                }
                else //sse client
                {
                    toLog(1, "Sse client HUP/ERR or gone, Closing...\n");
                    close_client(events[i].data.fd);
                }
                continue;
            }
            else if (sfd == events[i].data.fd)
            {
                /* We have a notification on the listening socket, which
                   means one or more incoming connections. */
                while (1)
                {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    in_len = sizeof(in_addr);
                    infd = accept(sfd, &in_addr, &in_len);
                    if(infd == -1)
                    {
                        if ((errno == EAGAIN) ||
                            (errno == EWOULDBLOCK))
                        {
                            /* We have processed all incoming connections. */
                            break;
                        }
                        else
                        {
                            toLog(1, "Error, accept() failure!\n");
                            break;
                        }
                    }

                    s = getnameinfo (&in_addr, in_len,
                                     hbuf, sizeof hbuf,
                                     sbuf, sizeof sbuf,
                                     NI_NUMERICHOST | NI_NUMERICSERV);
                    if(s == 0)
                        toLog(1,"Connect from %s on port %s as <%d>\n",hbuf,sbuf,infd);
                    else
                    {
                        toLog(1,"Error in getnameinfo() <%d>\n",infd);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    s = make_socket_non_blocking(infd);
                    if (s == -1)
                    {
                        toLog(1,"Cannot set new accepted socket to non blocking. I will close it!\n");
                        close(infd);
                        break;
                    }

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1)
                    {
                        toLog(0,"Error, epoll_ctl() add failure (3)\n");
                        beforeExit();
                        exit(1);
                    }

                    //Add to my list
                    int ccount;
                    client_add(infd);
                    snprintf(client_current()->info,30,"%s:%s",hbuf,sbuf);
                    client_current()->status = STATUS_NEW;

                    ccount = client_count();
                    toLog(2,"Added to the list (%d).\n",ccount);
                    if(stats.maxclients < ccount)
                        stats.maxclients = ccount;
                }
                continue;
            }
            else if (commfd == events[i].data.fd)
            {
                /* We have a notification on the communication listening socket, which
                   means one or more incoming connections for internal communication. */
                while (1)
                {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

                    in_len = sizeof(in_addr);
                    infd = accept(commfd, &in_addr, &in_len);
                    if(infd == -1)
                    {
                        if ((errno == EAGAIN) ||
                            (errno == EWOULDBLOCK))
                        {
                            /* We have processed all incoming connections. */
                            break;
                        }
                        else
                        {
                            toLog(1, "Error, accept() failure!\n");
                            break;
                        }
                    }

                    if(hsettings.loglevel >= 2)
                    {
                        s = getnameinfo (&in_addr, in_len,
                                         hbuf, sizeof hbuf,
                                         sbuf, sizeof sbuf,
                                         NI_NUMERICHOST | NI_NUMERICSERV);
                        if(s == 0)
                            toLog(2,"Communication connect from %s on port %s as <%d>\n",hbuf,sbuf,infd);
                        else
                        {
                            toLog(1,"Error in getnameinfo() <%d>\n",infd);
                        }
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    s = make_socket_non_blocking(infd);
                    if (s == -1)
                    {
                        toLog(1,"Cannot set new accepted socket to non blocking. I will close it!\n");
                        close(infd);
                        break;
                    }

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    s = epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1)
                    {
                        toLog(0,"Error, epoll_ctl() add failure (3)\n");
                        beforeExit();
                        exit(1);
                    }

                    commclient_add(infd);
                }
                continue;
            }
            else if (fifo == events[i].data.fd)
            {
                input[0] = '\0';
                char *input_p = input;
                while(1)
                {
                    ssize_t count;
                    count = read(events[i].data.fd, input_p, MAX_READ_SIZE-strlen(input));
                    if(count == -1)
                    {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN)
                        {
                            toLog(1,"Error, read() failure (1)\n");
                        }
                        break;
                    }
                    else if (count == 0)
                    {
                        break;
                    }

                    input_p[count]='\0';
                    input_p = input_p + count;
                }

                chop(input);
                toLog(2,"#FIFO received message: \"%s\"\n",input);
                parse_comm_messages(input);
            }
            else
            {
                /* We have data on the fd waiting to be read. Read and
                   display it. We must read whatever data is available
                   completely, as we are running in edge-triggered mode
                   and won't get a notification again for the same
                   data. */
                int done = 0;
                input[0] = '\0';
                char *input_p = input;

                ssize_t fullcount=0;
                ssize_t count;
                while(1)
                {
                    count = read(events[i].data.fd,input_p, MAX_READ_SIZE-strlen(input));
                    if(count == -1)
                    {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN)
                        {
                            toLog(1,"Error, read() failure (2) closing client...\n");
                            done = 1;
                        }
                        break;
                    }
                    else if (count == 0)
                    {
                        done = 1;
                        break;
                    }
                    fullcount += count;
                    input_p = input_p + count;
                }
                input[fullcount]='\0';

                if(commclient_check(events[i].data.fd)) //communication client
                {
                    if(done)
                    {
                        toLog(2,"Communication connection closed by peer:\n");
                        close_communication_client(events[i].data.fd);
                    }
                    else
                    {
                        chop(input);
                        toLog(2,"#COMM-TCP received message: \"%s\"\n",input);
                        parse_comm_messages(input);
                    }
                }
                else //sse client
                {
                    if(done)
                    {
                        toLog(2,"Connection closed by peer:\n");
                        close_client(events[i].data.fd);
                    }
                    else
                    {
                        cio_low_read(client_get(events[i].data.fd),input,fullcount);
                    }
                }
            }
        }
    }

    free (events);
    close (sfd);
    return 0;
}

void commclient_add(int fd)
{
    struct CommCli *n;
    struct CommCli *ccli = (struct CommCli *)malloc(sizeof(struct CommCli));
    ccli->fd = fd;
    ccli->next = NULL;
    if(commFirst == NULL)
        commFirst = ccli;
    else
    {
        for(n = commFirst;n->next != NULL;n = n->next);
        n->next = ccli;
    }
}

void commclient_del(int fd)
{
    struct CommCli *n;
    if(commFirst != NULL)
    {
        if(commFirst->fd == fd)
        {
            struct CommCli *tdel = commFirst;
            commFirst = commFirst->next;
            free(tdel);
            return;
        }
        for(n = commFirst;n->next != NULL;n = n->next)
            if(n->next->fd == fd)
            {
                struct CommCli *tdel = n->next;
                n->next = n->next->next;
                free(tdel);
                return;
            }
    }
}

void commclient_debug(void)
{
    struct CommCli *n;
    for(n = commFirst;n != NULL;n = n->next)
        toLog(2,"%d->",n->fd);
    toLog(2,"NULL\n");
}

int commclient_check(int fd)
{
    struct CommCli *n;
    for(n = commFirst;n != NULL;n = n->next)
        if(n->fd == fd)
            return 1;
    return 0;
}

//split by delimiter ; and call parse_comm_message on parts
void parse_comm_messages(char *fms)
{
    char *in_part=fms;
    int ii,input_length = strlen(fms);
    for(ii=0;ii<input_length;++ii)
        if(fms[ii] == ';')
        {
            fms[ii] = '\0';
            parse_comm_message(in_part);
            in_part = fms + ii + 1;
        }
    parse_comm_message(in_part);
}

void parse_comm_message(char *fm)
{
    if(strlen(fm) == 0)
        return;
    if(!commands(fm))
    {
        toLog(2,"Sending message to connected&subscribed clients...\n");
        sendmessages(fm);
        toLog(2,"finished sending.\n");
    }
}

int commands(char *input)
{
    if(strcmp(input,"clientlist") == 0)
    {
        toLog(0,"------- requested client list -------\n");
        client_list(0);
        toLog(0,"---------------- end ----------------\n");
        return 1;
    }
    if(strcmp(input,"loglevel_debug") == 0)
    {
        toLog(0,"Set loglevel to the maximum.\n");
        hsettings.loglevel = 2;
        return 1;
    }
    if(strcmp(input,"loglevel_normal") == 0)
    {
        toLog(0,"Set loglevel to normal.\n");
        hsettings.loglevel = 1;
        return 1;
    }
    if(strcmp(input,"loglevel_quiet") == 0)
    {
        toLog(0,"Set loglevel to the minimum.\n");
        hsettings.loglevel = 0;
        return 1;
    }

    if(strcmp(input,"reinit_enable") == 0)
    {
        toLog(0,"Reinit enabled.\n");
        hsettings.reinit_allowed = 1;
        return 1;
    }
    if(strcmp(input,"reinit_disable") == 0)
    {
        toLog(0,"Reinit disabled.\n");
        hsettings.reinit_allowed = 0;
        return 1;
    }

    if(strcmp(input,"status") == 0)
    {
        char rtstr[64];
        diffsec_to_str(time(NULL)-stats.startDaemon,rtstr,64);
        toLog(0,"------- status of hasses -------\n");
        toLog(0,"Version: %s\n",VERSION);
        toLog(0,"Compiled: %s\n",__DATE__);
        toLog(0,"Pid: %d\n",getpid());
        toLog(0,"Uid: %d (%s)\n",getuid(),(hsettings.paramuid == -1 ? "not set" : hsettings.paramuser));
        toLog(0,"Running time: %s\n",rtstr);
        toLog(0,"Mode: %s\n",(hsettings.use_ssl?"SSL encrypted (https)":"unencrypted (http)"));
        if(hsettings.use_ssl)
        {
            toLog(0,"SSL Cert key file: %s\n",hsettings.certfile);
            toLog(0,"SSL Prvt key file: %s\n",hsettings.pkeyfile);
        }
        toLog(0,"Loglevel: %d\n",hsettings.loglevel);
        toLog(0,"Reinit allowed: %s\n",hsettings.reinit_allowed ? "yes":"no");
        toLog(0,"CORS-base url: %s\n",hsettings.corsbase);
        toLog(0,"Count of connected clients: %d\n",client_count());
        toLog(0,"Current registred SSE clients: %d\n",client_count_commstate());
        toLog(0,"Maximum number of connections: %u\n",stats.maxclients);
        toLog(0,"Total client handshaked: %u\n",stats.allclient);
        toLog(0,"Total reinit connection: %u\n",stats.allreinit);
        toLog(0,"Total message processed: %u\n",stats.allmessage);
        toLog(0,"Total message sent: %u\n",stats.allsmessage);
        toLog(0,"------------- end --------------\n");
        return 1;
    }
    return 0;
}

void checkTimeouts(void)
{
    time_t t = time(NULL);
    if(last_cli_ttl_check + CLIENT_CHK_TIME_INTERVAL >= t)
        return;
    toLog(2,"- Client timeout check...\n");
    last_cli_ttl_check = t;
    client_start();
    while(client_next() != NULL)
        while(client_current() != NULL &&
                ((client_current()->status == STATUS_COMM && 
                    client_current()->created + CLIENT_HANDSHAKED_TIMEOUT < t) 
                  || 
                 (client_current()->status != STATUS_COMM && 
                    client_current()->created + CLIENT_NOTHSHAKED_TIMEOUT < t)))
        {
            toLog(1,"--- Client %s reach timeout, closing...\n",client_current()->info);
            close_client(client_current()->descr);
        }
}

int get_reinit_allowed(void)
{
    return hsettings.reinit_allowed;
}

void toLog(int level, const char * format, ...)
{
    if(level <= hsettings.loglevel)
    {
        if(hsettings.nodaemon)
        {
            va_list args;
            va_start(args,format);
            vfprintf(stdout,format,args);
            va_end(args);
            fflush(stdout);
        }
        else
        {
            FILE *logf;

            logf = fopen(hsettings.logfile,"a");
            if(logf == NULL)
            {
                fprintf(stderr,"Error opening log file: %s\n",hsettings.logfile);
                exit(1);
            }

            time_t rawtime;
            time(&rawtime);
            if(log_oldrawtime != rawtime)
            {
                struct tm * timeinfo;
                timeinfo = localtime (&rawtime);
                strftime(log_timebuf,80,"%F %T hasses: ",timeinfo);
                log_oldrawtime = rawtime;
            }

            va_list args;
            va_start(args,format);
            fputs(log_timebuf,logf);
            vfprintf(logf,format,args);
            va_end(args);
            fclose(logf);
        }
    }
}

void diffsec_to_str(int diff_sec,char *buffer,int max)
{
    int x,d,h,m,s;
    s = diff_sec%86400;
    d = (diff_sec-s)/86400;
    x = s;
    s = x%3600;
    h = (x-s)/3600;
    x = s;
    s = x%60;
    m = (x-s)/60;
    if(d > 0)
        snprintf(buffer,max,"%dday %02d:%02d:%02d",d,h,m,s);
    else
        snprintf(buffer,max,"%02d:%02d:%02d",h,m,s);
}
//end.
