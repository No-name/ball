CFLAG = -g -Wall
.PHONY:all

all:client server

client:client.c
	gcc -o $@ $(CFLAG) $<

server:server.c
	gcc -o $@ $(CFLAG) $<

.PHONY:clean
clean:
	rm client server
