GCC_FLAGS= -Wall -g -Wextra -I./include

all: server  client

client: src/client.c src/tcpClientUtil.c src/logger.c
	mkdir -p bin
	gcc $(GCC_FLAGS)  src/client.c src/tcpClientUtil.c src/util.c src/logger.c -o bin/client

server: src/server.c
	mkdir -p bin
	gcc $(GCC_FLAGS) src/server.c -o bin/server

clean:
	rm -f bin/*

.PHONY: all clean client server