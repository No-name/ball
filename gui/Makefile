SRC=gui_client.c main_panel.c chart_panel.c
OBJ=$(SRC:.c=.o)

gui_client:$(OBJ) ../ball.o
	gcc -o $@ $^ `pkg-config --cflags --libs gtk+-3.0`

.c.o:
	gcc -c $< -o $@ -I.. `pkg-config --cflags --libs gtk+-3.0`

.PHONY:clean
clean:
	rm -rf main_panel *.o
