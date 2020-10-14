![Hasses logo](https://raw.githubusercontent.com/hyper-prog/hasses/master/images/hasseslogo.png)

Hyper's async SSE (Server Sent Event) server
============================================

Hasses is a small notification server achieve Server Sent Event (SSE) listener
in a separate daemon.
It's written in C and it has asynchronous design to handle more clients in same thread.
It uses linux epoll() function.

 - What is SSE: http://en.wikipedia.org/wiki/Server-sent_events
 - What is epoll (Asyncronous programming): http://en.wikipedia.org/wiki/Epoll

Most SSE example uses server side codes holds a connection to each client.
It is usually means a process/thread for each client connection,
which is not optimal when the number of clients are rising.
This daemon act as a minimal web server which suitable to handle SSE connections only,
and can handle all incoming connections in one thread.
The main web server environment (which serve the web pages) can send the notifications
to the clients through a separate network connection or a FIFO file.
The clients can connect to the hasses server with a special token(s),
which are usable to address messages to a subset of clients or even one client.
The daemon has a rich logging and client tracking possibility.

Author
------

- Written by Peter Deak (C) hyper80@gmail.com, License GPLv2
- Webpage: http://hyperprog.com/hasses

Compile / Install
-----------------
Require openssl development tools
(debian: aptitude install libssl-dev)

    $make
    #make install

Docker images
-------------
Available an Alpine linux container with the configured hasses daemon:
 Docker hub:

- https://hub.docker.com/r/hyperprog/hassesdaemon

 Downloadable (pullable) image name:
 
    hyperprog/hassesdaemon

The image expose the 8080 port to SSE connections 
and the 8085 for control channel communication messages.
The sse requests matches to the /sse locations by default.

 Architecture
 ------------
 
 ![Hasses Arch](https://raw.githubusercontent.com/hyper-prog/hasses/master/images/architecture.png)

#### Config for apache2 proxy

    #aptitude install libapache2-mod-proxy-html
    #aptitude install libxml2-dev xml2 libxml2

    #a2enmod ssl
    #a2enmod proxy
    #a2enmod proxy_balancer
    #a2enmod proxy_http

    To the enabled site:
    
    ProxyPreserveHost On
	  ProxyPass /sseeventprovider http://0.0.0.0:8080/sseeventprovider
	  ProxyPassReverse /sseeventprovider http://0.0.0.0:8080/sseeventprovider


#### Config for nginx proxy

To the enabled site outside the "server" section:

    upstream ssebackend {
        server 127.0.0.1:8080;
        keepalive 3200;
    }

To the enabled site inside the "server" section:        

    location /sseeventprovider {
        proxy_buffering off;
        proxy_cache off;
        keepalive_timeout 0;
        proxy_set_header Connection "Keep-Alive";
        proxy_http_version 1.1;
        chunked_transfer_encoding on;
        proxy_pass http://ssebackend;
    }
    

 Debugging / Troubleshooting
 ---------------------------

If you would like get informations about the hasses internal state, get statistics or client information you can send commands
to the hasses communication channel to modify the internal state, or get information to the log output.

These commands are:

    "status" - Print status/statistics to the log
    "clientlist" - List clients to the log
    "loglevel_quiet" - Set loglevel to minimal
    "loglevel_normal" - Set loglevel to normal
    "loglevel_debug" - Set loglevel to maximum
    "reinit_enable" - Enable re-initialize opened connections
    "reinit_disable" - Enable re-initialize opened connections
    "<token>=<message>" - Send message to the subscribers of <token>
    "<token>=<message>;<token2>=<message2>" - Send more messages
    "<token>-<rId>=<message>" - Send message to the subscribers of <token> except <rId>
    "*=<message>;" - Send message to all clients

To send the "status" command to the /var/run/myprog/hassesfifo file:

    echo "status" | /var/run/myprog/hassesfifo

To send the "status" command to the 8085 tcp port communication channel (on 192.168.1.10 ip)

    echo "status" | netcat 192.168.1.10 8085

The "status" command will generate similar message to the log:

    ------- status of hasses -------
    Version: 1.22
    Compiled: May  7 2020
    Pid: 3217
    Uid: 0 (not set)
    Running time: 00:00:26
    Mode: unencrypted (http)
    Loglevel: 2
    Reinit allowed: no
    CORS-base url: *
    Count of connected clients: 0
    Current registred SSE clients: 0
    Maximum number of connections: 0
    Total client handshaked: 0
    Total reinit connection: 0
    Total message processed: 0
    Total message sent: 0
    ------------- end --------------

The "clientlist" command will generate output like this:

    ------- requested client list -------
    List of clients:
    #1 - <6> info: 192.168.1.151:50190 S:SSE Err: 0 Messages: 3 Reinit: 0 SSL:No
      Connection time: 00:03:00  Handshaked time: 00:03:00 UniqId:
      Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:76.0) Gecko/20100101 Firefox/76.0
      Subscribes: room624
    #2 - <7> info: 192.168.1.151:50200 S:SSE Err: 0 Messages: 0 Reinit: 0 SSL:No
      Connection time: 00:00:09  Handshaked time: 00:00:09 UniqId:
      Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/81.0.4044.129 Safari/537.36
      Subscribes: room686
    ---------------- end ----------------



