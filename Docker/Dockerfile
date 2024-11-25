#
# Dockerfile of HaSSEs
# Automatic builded by hub.docker.com from
#  https://github.com/hyper-prog/hasses
#

FROM alpine AS hassesbuildstage
RUN apk update && apk add openssl
COPY . /hasses/
RUN apk add libc-dev gcc make openssl-dev \
    && cd /hasses \
    && make && make install


FROM alpine AS hassesdaemon
LABEL maintainer="hyper80@gmail.com" Description="Hasses asynchronous SSE server"
RUN apk update && apk add openssl
COPY --from=hassesbuildstage /usr/local/bin/hasses /usr/local/bin/

EXPOSE 8080
EXPOSE 8085

CMD ["-p=8080","-cp=8085","-murl=/sse","-nodaemon","-cors-base=*","-q"]
# Srart hasses daemon when container runs
ENTRYPOINT ["/usr/local/bin/hasses"]
