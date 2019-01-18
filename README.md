![Hasses logo](https://raw.githubusercontent.com/hyper-prog/hasses/master/images/hasseslogo.png)

Hyper's async SSE (Server Sent Event) server
============================================

Hasses is a small notification server achieve Server Sent Event (SSE) listener
in a separate daemon. 
It's written in C and it has asynchronous design to handle more clients in same thread. 
It uses linux epoll() function. 

 - What is SSE: http://en.wikipedia.org/wiki/Server-sent_events
 - What is epoll (Asyncronous programming): http://en.wikipedia.org/wiki/Epoll

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
    
 Acrhitecture
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
    