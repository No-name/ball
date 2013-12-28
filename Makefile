SRC=client.c server.c ball.c
OBJ=$(SRC:%.c=%.o)

CFLAG = -g -Wall
.PHONY:all

all:client server gui_client

gui_client : gui_client.c ball.o
	gcc -o $@ $(CFLAG) $^ `pkg-config --cflags --libs gtk+-3.0`

client:client.o ball.o
	gcc -o $@ $(CFLAG) $^

server:server.o ball.o
	gcc -o $@ $(CFLAG) $^

.c.o:
	gcc -c $< $(CFLAG) -o $@ 

.PHONY:clean
clean:
	rm client server $(OBJ)
