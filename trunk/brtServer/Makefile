SHELL = /bin/sh
SYSTEM = $(shell uname)
C++ = g++
CC = g++
DFLAGS = 
OFLAGS = -O3 -g
LFLAGS = -L. -L/usr/lib/ -L/usr/local/lib/mysql -L/usr/local/include/ -L/usr/local/lib -lmysqlclient_r -lrt -lboost_filesystem
CFLAGS =

CFLAGS += $(OFLAGS) $(DFLAGS) -I. -I/usr/local/include -Izlib

OBJS = updater.o commandpacket.o server.o pubprotocol.o socket.o md5.o sha1.o util.o user.o main.o
LIBS = zlib/libz.a
COBJS = anet.o
PROGS = ./server

all: $(OBJS) $(COBJS) $(PROGS)

./server: $(OBJS) $(COBJS)
	$(C++) -o ./server $(COBJS) $(OBJS) $(LFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(COBJS) $(PROGS)

$(OBJS): %.o: %.cpp
	$(C++) -o $@ $(CFLAGS) -c $<

$(COBJS): %.o: %.c
	$(CC) -o $@ $(CFLAGS) -c $<

./server: $(COBJS) $(OBJS)

all: $(PROGS)

anet.o: anet.c fmacros.h anet.h 
updater.o: includes.h updater.h
socket.o: includes.h util.h socket.h
util.o: includes.h util.h
users.o: includes.h user.h
commandpacket.o: includes.h commandpacket.h
server.o: includes.h server.h pubprotocol.h user.h server.h md5.h sha1.h anet.h redisclient.h
pubprotocol.o: server.h util.h pubprotocol.h
md5.o: md5.h
sha1.o: sha1.h
main.o: includes.h redisclient.h util.h socket.h user.h md5.h updater.h