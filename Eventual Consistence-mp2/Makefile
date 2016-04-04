#auther Xiaobin Zheng, xzheng19, 668374748
EXECS=client server
CFLAGS=-g -Wall -Wextra -Wno-unused-parameter -pthread -std=c99
# CC=clang
CC=gcc

all: $(EXECS)

client: client.o
	$(CC) $(CFLAGS) client.o -o client

server: server.o
	$(CC) $(CFLAGS) server.o -o server

client.o: client.c
	$(CC) $(CFLAGS) -c client.c -o client.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c -o server.o

%.o: %.c
	$(CC) -c $<

clean:
	rm -f *.o *.txt $(EXECS) *~