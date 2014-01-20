CFLAG= -g
SRC=gui_client.c main_panel.c chart_panel.c login_panel.c
OBJ=$(SRC:.c=.o)

TARGET=ball_client

$(TARGET):$(OBJ) ../lib/ball.o
	gcc -o $@ $^ `pkg-config --cflags --libs gtk+-3.0`

.c.o:
	gcc -c $(CFLAG) $< -o $@ -I../include `pkg-config --cflags --libs gtk+-3.0`

.PHONY:clean
clean:
	rm -rf $(TARGET) *.o
