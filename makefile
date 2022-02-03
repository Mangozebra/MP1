# makefile

all: server client

client: crc.c interface.h command.h
	g++ -g -w -std=c++11 -o client crc.c

server: crsd.c interface.h command.h
	g++ -g -w -std=c++11 -o server crsd.c

clean:
	rm -rf *.o *.csv fifo* server client data*_*
