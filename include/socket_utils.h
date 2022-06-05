#ifndef PROYECTO_PROTOS_SOCKET_CREATION_H
#define PROYECTO_PROTOS_SOCKET_CREATION_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "../include/debug.h"
#include "../include/selector.h"
/**
 * Creates socket, binds it, listens on it and sets it as NON-BLOCKING
 * @param type
 *              Type of address.
 * @param addr
 *              struct sockaddr_in used if type == AF_INET
 * @param addr6
 *              struct sockaddr_in used if type == AF_INET6
 * @return
 *          socket_fd from socket() call on success
 *          -1 on error
 */
int create_socket(int type, struct sockaddr_in * addr, struct sockaddr_in6 * addr6);
#endif //PROYECTO_PROTOS_SOCKET_CREATION_H
