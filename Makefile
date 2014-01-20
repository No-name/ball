.PHONY:all server client lib

all:server client

server:lib
	cd server && make

client:lib
	cd gui && make

lib:
	cd lib && make

.PHONY:clean
clean:
	cd server && make clean
	cd gui && make clean
	cd lib && make clean
