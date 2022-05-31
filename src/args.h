#ifndef PROYECTO_PROTOS_ARGS_H
#define PROYECTO_PROTOS_ARGS_H

#define MAX_USERS 10
#include <stdbool.h>
#include <netinet/in.h>
struct users {
    char *name;
    char *pass;
};

typedef struct socks5args_struct {
    char *              socks_addr;
//    char *              socks_addr_6;
    unsigned short      socks_port;
//    int                 socks_family;
    struct sockaddr_in socks_addr_info;
//    struct sockaddr_in6 socks_addr_info6;

    char *              mng_addr;
//    char *              mng_addr_6;
    unsigned short      mng_port;
//    int                 mng_family;
    struct sockaddr_in mng_addr_info;
//    struct sockaddr_in6 mng_addr_info6;

    bool disectors_enabled;

//    uint16_t socks5_buffer_size;
//    uint16_t sctp_buffer_size;
//
//    uint8_t timeout;

    struct users    users[MAX_USERS];
    int             n_users;
} socks5args_struct;
typedef socks5args_struct * socks5args;

void parse_args(const int argc, const char **argv, socks5args args);

#endif //PROYECTO_PROTOS_ARGS_H
