#include <stdio.h>     /* for printf */
#include <stdlib.h>
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include "../include/debug.h"
#include "../include/address_utils.h"
#include "../include/args.h"

#define DEFAULT_BUFFER_SIZE 2048

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

static long
buffer_size(const char *s) {
    char *end     = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)) {
        fprintf(stderr, "Error setting buffer size\n");
        return -1;
    }
    return sl;
}

static int
user(char *s, struct users *user) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        return -1;
    } else {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }
    return 0;
}

static void
version(void) {
    fprintf(stderr, "socks5v version 0.0\n"
                    "ITBA Protocolos de ComunicaciÃ³n 2020/1 -- Grupo X\n"
                    "AQUI VA LA LICENCIA\n");
}

static int
usage(const char *progname) {
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -l <SOCKS addr>  Dirección donde servirÃ¡ el proxy SOCKS.\n"
            "   -d Activa el debugging por STDOUT.\n"
            "   -D Activa el debugging por el archivo debugging.txt.\n"
            "   -b <buffer size> Setea el tamaño del buffer del proxy SOCKS\n"
            "   -N Deshabilita los passwords disectors.\n"
            "   -L <conf  addr>  Dirección donde servirÃ¡ el servicio de management.\n"
            "   -p <SOCKS port>  Puerto entrante conexiones SOCKS.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el proxy. Hasta 10.\n"
            "   -v               Imprime información sobre la versión y termina.\n"
            "\n",
            progname);
    return -1;
}

int parse_args(const int argc, char *const * argv, struct socks5args *args) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    //// Default values
    args->socks_addr = "127.0.0.1";
    args->socks_addr_6 = "::1";
    args->socks_port = 1080;
    args->socks_family = AF_UNSPEC;
    memset(&args->socks_addr_info, 0, sizeof(args->socks_addr_info));
    memset(&args->socks_addr_info6, 0, sizeof(args->socks_addr_info6));

    args->buffer_size = DEFAULT_BUFFER_SIZE;
    args->mng_buffer_size = DEFAULT_BUFFER_SIZE;

    args->mng_addr = "127.0.0.1";
    args->mng_addr_6 = "::";
    args->mng_port = 8888;
    args->mng_family = AF_UNSPEC;
    memset(&args->mng_addr_info, 0, sizeof(args->mng_addr_info));
    memset(&args->mng_addr_info6, 0, sizeof(args->mng_addr_info6));

    args->disectors_enabled = true;

    args->debug = 0;

    int c;
    int aux;
    long long_aux;
    while (true) {
        int option_index = 0;

        c = getopt_long(argc, argv, "dDhl:L:Np:P:u:v", NULL, &option_index);
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                args->debug = STDOUT_DEBUG;
                break;
            case 'D':
                args->debug = FILE_DEBUG;
                break;
            case 'b':
                // TODO(bruno) Esto no esta implementado.
                long_aux = buffer_size(optarg);
                if(aux == -1)
                    return -1;
                args->buffer_size = (unsigned short)long_aux;
                break;
            case 'h':
                return usage(argv[0]);
            case 'l':
                args->socks_family = address_processing(optarg, &args->socks_addr_info, &args->socks_addr_info6, args->socks_port);
                if(args->socks_family == -1){
                    printf("Unable to resolve address type. Please, enter a valid address.\n");
                    return -1;
                }
                if(args->socks_family == AF_INET) {
                    args->socks_addr = optarg;
                }
                if(args->socks_family == AF_INET6) {
                    args->socks_addr_6 = optarg;
                }
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
            case 'N':
                args->disectors_enabled = false;
                break;
            case 'p':
                aux = port(optarg);
                if(aux == -1)
                    return -1;
                args->socks_port = (unsigned short)aux;
                args->socks_addr_info.sin_port = htons(aux);
                args->socks_addr_info6.sin6_port = htons(aux);
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
                if(nusers >= MAX_USERS) {
                    fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
                    return -1;
                } else {
                    aux = user(optarg, users + nusers);
                    if(aux == -1)
                        return -1;
                    nusers++;
                }
                break;
            case 'v':
                version();
                return -1;
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
    if(args->socks_family == AF_UNSPEC){
        aux = address_processing(args->socks_addr, &args->socks_addr_info, &args->socks_addr_info6, args->socks_port);
        if(aux == -1){
            printf("Error processing default IPv4 address for proxy SOCKS.\n");
            return -1;
        }
        aux = address_processing(args->socks_addr_6, &args->socks_addr_info, &args->socks_addr_info6, args->socks_port);
        if(aux == -1){
            printf("Error processing default IPv6 address for proxy SOCKS.\n");
            return -1;
        }
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

