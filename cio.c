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

#include <openssl/ssl.h>
#include <openssl/bio.h>

#include "hasses.h"
#include "cdata.h"
#include "cio.h"

#define CIO(a) ((struct CioData *)(a->cio))

struct CioData
{
    SSL *ssl;
    BIO *in_bio;
    BIO *out_bio;
};

int no_ssl = 0;
SSL_CTX *ssl_ctx;

int (*cio_high_read_callback)(struct CliConn *,char *) = NULL;
int (*cio_low_write_callback)(struct CliConn *,char *,int) = NULL;

const char *cio_ssl_error_text(int e);

int cio_init(int use_ssl,const char *certfile,const char *pkeyfile)
{
    if(cio_high_read_callback == NULL || cio_low_write_callback == NULL)
    {
        toLog(0,"SSL Error, you must set cio_high_read_callback and cio_low_write_callback functions!\n");
        return 1;
    }

    no_ssl = !use_ssl;

    if(no_ssl)
        return 0;

    toLog(1,"Initialize SSL.\n");

    SSL_load_error_strings(); // readable error messages
    SSL_library_init();       // initialize library
    ssl_ctx = SSL_CTX_new(TLS_server_method()); // create context

    if (ssl_ctx) {
        SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
    }

    if(!SSL_CTX_use_certificate_file(ssl_ctx,certfile,SSL_FILETYPE_PEM))
    {
        toLog(0,"SSL Error, failed to add Certificate key file: \"%s\"\n",certfile);
        return 1;
    }
    else
        toLog(2,"SSL: Certification key added.\n");

    if(!SSL_CTX_use_PrivateKey_file(ssl_ctx,pkeyfile,SSL_FILETYPE_PEM))
    {
        toLog(0,"SSL Error, failed to add Private key file: \"%s\"\n",pkeyfile);
        return 1;
    }
    else
        toLog(2,"SSL: Private key added.\n");

    if(!SSL_CTX_check_private_key(ssl_ctx))
    {
        toLog(0,"SSL Error, Key check failed! (SSL_CTX_check_private_key)\n");
        return 1;
    }
    else
        toLog(2,"SSL: Key check succes.\n");

    return 0;
}

int cio_init_ssl(struct CliConn *client)
{
    client->cio = (struct CioData *)malloc(sizeof(struct CioData));
    CIO(client)->ssl = SSL_new(ssl_ctx);
    if(!CIO(client)->ssl)
    {
        toLog(0,"SSL Error: Cannot create new SSL! <%d>\n",client->descr);
        return 1;
    }

    CIO(client)->in_bio = BIO_new(BIO_s_mem());
    if(CIO(client)->in_bio == NULL)
    {
        toLog(0,"SSL Error: Cannot allocate read BIO.\n");
        return 1;
    }
    //BIO_set_mem_eof_return(CIO(client)->in_bio, -1);

    CIO(client)->out_bio = BIO_new(BIO_s_mem());
    if(CIO(client)->out_bio == NULL)
    {
        toLog(0,"SSL Error: cannot allocate write BIO.\n");
        return 1;
    }
    //BIO_set_mem_eof_return(CIO(client)->out_bio, -1);

    SSL_set_bio(CIO(client)->ssl, CIO(client)->in_bio, CIO(client)->out_bio);
    SSL_set_accept_state(CIO(client)->ssl);
    client->status = STATUS_SSLACPT;
    return 0;
}

void write_outbio_pending_to_low(struct CliConn *client)
{
    int pending = BIO_ctrl_pending(CIO(client)->out_bio);
    if(pending > 0)
    {
        int read;
        char out_buffer[MAX_READ_SIZE];
        read = BIO_read(CIO(client)->out_bio,out_buffer,sizeof(out_buffer));
        (*cio_low_write_callback)(client,out_buffer,read);
    }
}

int cio_high_write(struct CliConn *client,char *buffer)
{
    if(no_ssl)
        return (*cio_low_write_callback)(client,buffer,strlen(buffer));

    if(client->cio == NULL)
        if(cio_init_ssl(client))
            return 1;

    switch(client->status)
    {
        case STATUS_UNKNOWN:
        case STATUS_NEW:
        case STATUS_SSLACPT:
        case STATUS_END:
            return 1;
        default:
            break;
    }

    SSL_write(CIO(client)->ssl,buffer,strlen(buffer));
    write_outbio_pending_to_low(client);
    return 0;
}

int cio_low_read(struct CliConn *client,char *buffer,int length)
{
    if(no_ssl)
    {
        buffer[length] = '\0';
        return (*cio_high_read_callback)(client,buffer);
    }

    if(client->cio == NULL)
        if(cio_init_ssl(client))
            return 1;

    char rio_buffer[MAX_READ_SIZE];

    int written = 0;
    int read = 0;

    //Waiting data to be read by ssl
    if(length > 0)
        written = BIO_write(CIO(client)->in_bio,buffer,length);

    if(written > 0 || 1)
    {
        if(!SSL_is_init_finished(CIO(client)->ssl))
        {
            int rv,rv2;
            rv = SSL_accept(CIO(client)->ssl);
            if(rv==1)
            {
                toLog(2,"SSL_accept() succesful, ssl connection established.\n");
                client->status = STATUS_NEW;
                write_outbio_pending_to_low(client);
            }
            if(rv==0)
            {
                rv2 = SSL_get_error(CIO(client)->ssl,rv);
                toLog(1,"SSL_accept ERROR, Connection shut down. (0):\nSSL_get_error says %d, %s\n",
                            rv2,cio_ssl_error_text(rv2));
            }
            if(rv<0)
            {
                rv2 = SSL_get_error(CIO(client)->ssl,rv);
                if(rv2 == SSL_ERROR_WANT_READ || rv2 == SSL_ERROR_WANT_WRITE)
                    write_outbio_pending_to_low(client);
                else
                    toLog(1,"SSL_accept returned <0 (%d):\nSSL_get_error says %d, %s\n",
                           rv,rv2,cio_ssl_error_text(rv2));
            }
        }
    }

    if(SSL_is_init_finished(CIO(client)->ssl))
    {
        int rv2;
        read = SSL_read(CIO(client)->ssl, rio_buffer, sizeof(rio_buffer));
        if(read > 0)
        {
            rio_buffer[read]='\0';
            if(strlen(rio_buffer) > 0)
                (*cio_high_read_callback)(client,rio_buffer);
        }
        if(read == 0)
        {
            rv2 = SSL_get_error(CIO(client)->ssl,read);
            toLog(1,"SSL_read returns 0, Error: %d, %s\n",rv2,cio_ssl_error_text(rv2));
        }
        if(read < 0)
        {
            rv2 = SSL_get_error(CIO(client)->ssl,read);
            if(rv2 == SSL_ERROR_WANT_READ || rv2 == SSL_ERROR_WANT_WRITE)
                write_outbio_pending_to_low(client);
            else
                toLog(1,"SSL_read returned <0 (%d):\nSSL_get_error says %d, %s\n",
                        read,rv2,cio_ssl_error_text(rv2));
        }
    }

    return 0;
}

int cio_client_close(struct CliConn *client)
{
    if(client == NULL)
        return 0;
    if(client->cio != NULL)
    {
        SSL_shutdown(CIO(client)->ssl);
        SSL_free(CIO(client)->ssl);
        toLog(2,"SSL: Shutdown/Free connection.\n");
        client->cio = NULL;
    }
    return 0;
}

int cio_info_text(struct CliConn *client,char *buffer,int maxlen)
{
    if(client->cio == NULL)
    {
        strcpy(buffer,"");
        return 0;
    }
    snprintf(buffer,maxlen,"SSL Status:%s Ver:%s Chiper:%s",
                    SSL_state_string(CIO(client)->ssl),
                    SSL_get_version(CIO(client)->ssl),
                    SSL_get_cipher_name(CIO(client)->ssl)
            );
    return 1;
}

const char *cio_ssl_error_text(int e)
{
    switch(e)
    {
        case SSL_ERROR_NONE:
            return "SSL_ERROR_NONE";
        case SSL_ERROR_ZERO_RETURN:
            return "SSL_ERROR_ZERO_RETURN";
        case SSL_ERROR_WANT_READ:
            return "SSL_ERROR_WANT_READ";
        case SSL_ERROR_WANT_WRITE:
            return "SSL_ERROR_WANT_WRITE";
        case SSL_ERROR_WANT_CONNECT:
            return "SSL_ERROR_WANT_CONNECT";
        case SSL_ERROR_WANT_ACCEPT:
            return "SSL_ERROR_WANT_ACCEPT";
        case SSL_ERROR_WANT_X509_LOOKUP:
            return "SSL_ERROR_WANT_X509_LOOKUP";
        case SSL_ERROR_SYSCALL:
            return "SSL_ERROR_SYSCALL";
        case SSL_ERROR_SSL:
            return "SSL_ERROR_SSL";
        default:
            return "Unknown";
    }
}

void cio_high_read_SET(int (*c)(struct CliConn *,char *))
{
    cio_high_read_callback = c;
}

void cio_low_write_SET(int (*c)(struct CliConn *,char *,int))
{
    cio_low_write_callback = c;
}

//end.
