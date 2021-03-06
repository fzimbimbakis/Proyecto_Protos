CFLAGS= -g -I./include --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L

CLIENT_C_FILES = src/client/client.o src/client/clientUtils.o src/client/clientArgs.o src/debug.o src/address_utils.o

LDFLAGS = -lpthread -pthread
SERVER_C_FILES =  src/netutils.o src/selector.o src/stm.o src/myParser.o src/password_dissector/dissec_parser.o src/Etapas/authentication.o src/socks5nio.o src/mng_nio.o src/Etapas/mng_request.o src/Etapas/connecting.o src/Etapas/hello.o src/Etapas/request_parser.o src/Etapas/resolv.o src/Etapas/request.o src/Etapas/copy.o src/debug.o src/address_utils.o src/socket_utils.o src/buffer.o src/args.o src/main.o

dissec_parser.o: dissec_parser.h
mng_nio.o: mng_nio.h
mng_request.o: mng_request.h
authentication.o:	authentication.h
myParser.o:			myParser.h
request_parser.o:	request_parser.h
resolv.o:			resolv.h
client.o:			client.h
clientArgs.o:		clientArgs.h
tcpClientUtil.o:	tcpClientUtil.h
clientUtils.o:		clientUtils.h
util.o:				util.h
logger.o:			logger.h
selector.o:			selector.h
stm.o:				stm.h
socks5nio.o:		socks5nio.h
connecting.o:		connecting.h
hello.o:			hello.h
request.o:			request.h
copy.o:				copy.h
debug.o:			debug.h
address_utils.o:	address_utils.h
socket_utils.o:		socket_utils.h
buffer.o:			buffer.h
args.o:				args.h
main.o:				main.h
netutils.o:			netutils.h


all: server  client

client: $(CLIENT_C_FILES)
	mkdir -p bin
	$(CC) $(CFLAGS)  $(CLIENT_C_FILES) -o bin/client

server: $(SERVER_C_FILES)
	mkdir -p bin
	$(CC) $(CFLAGS) $(LDFLAGS) $(SERVER_C_FILES) -o bin/server

clean:
	rm -fr ./bin/*
	rm -f ./src/Etapas/*.o
	rm -f ./src/*.o
	rm -f ./src/password_dissector/*.o
	rm -f ./src/client/*.o


.PHONY: all clean