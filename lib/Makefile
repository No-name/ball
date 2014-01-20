SRC=ball.c

TARGET=ball.o

$(TARGET):$(SRC)
	gcc -c $< $(CFLAG) -o $@ -I../include

.PHONY:clean
clean:
	rm $(TARGET)
