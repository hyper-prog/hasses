Hyper's async SSE (Server Sent Event) server

 What is SSE: http://en.wikipedia.org/wiki/Server-sent_events
 What is epoll (Asyncronous programming): http://en.wikipedia.org/wiki/Epoll

Author
-----------------------------------------
Written by Peter Deak (hyper80@gmail.com)
Webpage: http://hyperprog.com
Copyright (C) 2015
License: GPLv2

Compile
-----------------------------------------
Require openssl development tools
(debian: aptitude install libssl-dev)

 $make
 #make install


Config for apache2 proxy (may be instable)
-----------------------------------------
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


Config for nginx proxy
-----------------------------------------
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
