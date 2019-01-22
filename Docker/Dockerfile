#
# Dockerfile of HaSSEs
# Automatic builded by hub.docker.com from
#  https://github.com/hyper-prog/hasses
#

FROM alpine
LABEL maintainer="hyper80@gmail.com" Description="Hasses asynchronous SSE server"

RUN apk update \
    && apk add openssl
COPY . /hasses/
RUN apk add libc-dev gcc make openssl-dev \
    && cd /hasses \
    && make && make install \
    && cd / && rm -rf /hasses \
    && apk del libc-dev gcc make openssl-dev \
    && mkdir /hassescommon

EXPOSE 8080
EXPOSE 8085

VOLUME ["/hassescommon"]

CMD ["-p=8080","-cp=8085","-murl=/sse","-nodaemon"]
# Srart hasses daemon when container runs
ENTRYPOINT ["/usr/local/bin/hasses"]
