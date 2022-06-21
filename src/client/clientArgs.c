#include "../../include/clientArgs.h"
#include <stdio.h>     /* for printf */
#include <stdlib.h>
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include "debug.h"
#include "../../include/address_utils.h"
#include <stdbool.h>

static int
port(const char *s) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
        fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
        return -1;
    }
    return (unsigned short)sl;
}

static int
user(char *s, struct user* user) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        return -1;
    } else {
        *p = 0;
        p++;
        user->username = s;
        user->password = p;
        user->credentials=true;
    }
    return 0;
}

int parse_args(const int argc, char *const * argv, struct m16args *args) {

    //// Default values

    args->mng_addr = "127.0.0.1";
    args->mng_addr_6 = "::1";
    args->mng_port = 8080;
    args->mng_family = AF_UNSPEC;
    memset(&args->mng_addr_info, 0, sizeof(args->mng_addr_info));
    memset(&args->mng_addr_info6, 0, sizeof(args->mng_addr_info6));

    int c;
    int aux;
    while (true) {
        int option_index = 0;

        c = getopt_long(argc, argv, "dD:L:P:u:", NULL, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                args->debug = STDOUT_DEBUG;
                break;
            case 'D':
                args->debug = FILE_DEBUG;
                break;
            case 'L':
                args->mng_family = address_processing(optarg, &args->mng_addr_info, &args->mng_addr_info6, args->mng_port);
                if(args->mng_family == -1){
                    printf("Unable to resolve address type. Please, enter a valid address.\n");
                    return -1;
                }
                if(args->mng_family == AF_INET) {
                    args->mng_addr = optarg;
                }
                if(args->mng_family == AF_INET6) {
                    args->mng_addr_6 = optarg;
                }
                break;
            case 'P':
                aux = port(optarg);
                if(aux == -1)
                    return -1;
                args->mng_port = (unsigned short)aux;
                args->mng_addr_info.sin_port = htons(aux);
                args->mng_addr_info6.sin6_port = htons(aux);
                break;
            case 'u':
                aux = user(optarg, args->user);
                if(aux == -1)
                    return -1;
                break;
            default:
                fprintf(stderr, "unknown argument %d.\n", c);
                return -1;
        }

    }
    if (optind < argc) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        return -1;
    }
    if(args->mng_family == AF_UNSPEC){
        aux = address_processing(args->mng_addr, &args->mng_addr_info, &args->mng_addr_info6, args->mng_port);
        if(aux == -1){
            printf("Error processing default IPv4 address for mng.\n");
            return -1;
        }
        aux = address_processing(args->mng_addr_6, &args->mng_addr_info, &args->mng_addr_info6, args->mng_port);
        if(aux == -1){
            printf("Error processing default IPv6 address for mng.\n");
            return -1;
        }
    }
    return 0;
}

