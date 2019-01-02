# Asynchronous SSE Server
# Author: Peter Deak (hyper80@gmail.com)
# License: GPL

CFLAGS= -Wall -O3
L_CL_FLAGS= -Wall -O3
L_SW_FLAGS= -Wall -O3 -lssl -lcrypto
COMPILER=gcc

all: hasses

hasses: hasses.o cdata.o chat.o cio.o
	$(COMPILER) $(+) -o $(@) $(L_SW_FLAGS)

hasses.o: hasses.c cdata.h chat.h hasses.h cio.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

cdata.o: cdata.c cdata.h 
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

chat.o: chat.c chat.h cdata.h hasses.h cio.h
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

cio.o: cio.c cio.h 
	$(COMPILER) -c $(<) -o $(@) $(CFLAGS)

clean:
	rm *.o;rm ./hasses

install:
	install -s -m 0755 hasses /usr/local/bin/hasses
	install -m 0755 build-hasses-parameters /usr/local/bin/build-hasses-parameters

uninstall:
	rm /usr/local/bin/hasses
	rm /usr/local/bin/build-hasses-parameters
