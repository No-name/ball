SRC=server.c db_proc.c
OBJ=$(SRC:%.c=%.o)

CFLAG = -g -Wall

TARGET=ball_server

$(TARGET):$(OBJ)
	gcc -o $@ $(CFLAG) $^ ../lib/ball.o -lpq

.c.o:
	gcc -c $< $(CFLAG) -o $@ -I../include

.PHONY:clean
clean:
	rm *.o $(TARGET)
