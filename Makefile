CC = gcc

LIBS = -lresolv -lsocket -lm -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

FLAGS =  -g -O2
CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib


all: server client

server: server.o get_hw_addrs.o msg.o
	${CC} ${FLAGS} -o server server.o get_hw_addrs.o msg.o ${LIBS}

client: client.o get_hw_addrs.o msg.o
	${CC} ${FLAGS} -o client client.o get_hw_addrs.o msg.o ${LIBS}

msg.o: msg.c
	${CC} ${CFLAGS} -c msg.c

client.o: client.c a3.h
	${CC} ${CFLAGS} -c client.c a2.h 

server.o: server.c a3.h
	${CC} ${CFLAGS} -c server.c a2.h

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${CFLAGS} -c get_hw_addrs.c

clean:
	rm get_hw_addrs.o server server.o client client.o msg.o


