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
    unsigned short      socks_port;
    struct sockaddr_in socks_addr_info;

    struct users    users[MAX_USERS];
    int             n_users;
} socks5args_struct;
typedef socks5args_struct * socks5args;

void parse_args(const int argc, const char **argv, socks5args args);

#endif //PROYECTO_PROTOS_ARGS_H
