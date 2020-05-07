/* Asynchronous SSE Server
 * Author: Peter Deak (hyper80@gmail.com)
 * License: GPL
 */
#ifndef HASSES_CIO_H
#define HASSES_CIO_H

/* Schematic view of CIO communications/functions

                                                      cio_high_read_SET(callback)
  ______________                           ___________            |              _____________
 /              \                         /           \           |             /             \
 | Socket/file  | --- cio_low_read() ---> |           | --- cio_high_read() --> |             |
 | memory       |                         | CIO codes |                         | Application |
 |    devices   | <-- cio_low_write() --  |           | <-- cio_high_write() -- |             |
 \______________/          |              \___________/                         \_____________/
                           |
               cio_low_write_SET(callback)

  cio_high_* functions's buffer are C strings
  cio_low_* function's buffer are bytes with specified length
*/

/** Init cio subsystem, it must called once at program before any cio* func. */
int cio_init(int use_ssl,const char *certfile,const char *pkeyfile);

/** Close/free the specified client */
int cio_client_close(struct CliConn *client);

int cio_high_write(struct CliConn *client,char *buffer);
void cio_high_read_SET(int (*c)(struct CliConn *,char *));

int cio_low_read(struct CliConn *client,char *buffer,int length);
void cio_low_write_SET(int (*c)(struct CliConn *,char *,int));

int cio_info_text(struct CliConn *client,char *buffer,int maxlen);

#endif
//end.
