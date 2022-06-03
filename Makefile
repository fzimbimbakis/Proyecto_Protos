GCC_FLAGS= -Wall -g -Wextra -I./include
CLIENT_C_FILES = src/client.c src/tcpClientUtil.c src/util.c src/logger.c
SERVER_C_FILES = src/selector.c src/buffer.c src/args.c src/superServer.c
all: server  client

client: src/client.c src/tcpClientUtil.c src/logger.c
	mkdir -p bin
	gcc $(GCC_FLAGS)  $(CLIENT_C_FILES) -o bin/client

server: src/server.c
	mkdir -p bin
	gcc $(GCC_FLAGS) $(SERVER_C_FILES) -o bin/server

clean:
	rm -f bin/*

.PHONY: all clean client server