#ifndef PROYECTO_PROTOS_ADDRESS_UTILS_H
#define PROYECTO_PROTOS_ADDRESS_UTILS_H
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "socks5nio.h"
#include <netdb.h>

/**
 * Converts a presentation format address to network format with inet_pton.
 * Sets it in params addresses structs.
 * @param address
 * @param addr
 * @param addr6
 * @return
 *          AF_INET if address is IPv4
 *          AF_INET6 if address is IPv6
 *          -1 if it does not match an IP address
**/
int address_processing(char * address, struct sockaddr_in *addr, struct sockaddr_in6 *addr6, uint16_t port);

/**
 * Sets the origin information to the current addrinfo
 * @param data
 * @param current
 * @return
 */
void set_addr(struct selector_key * key, struct addrinfo * current);
#endif //PROYECTO_PROTOS_ADDRESS_UTILS_H
