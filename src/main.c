/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en este hilo.
 *
 * Se descargarÃ¡ en otro hilos las operaciones bloqueantes (resoluciÃ³n de
 * DNS utilizando getaddrinfo), pero toda esa complejidad estÃ¡ oculta en
 * el selector.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "../include/socket_utils.h"
#include "../include/address_utils.h"
#include "../include/mng_nio.h"
//#include "socks5.h"
#include "../include/selector.h"
#include "../include/socks5nio.h"
#include "../include/args.h"
#include "../include/mng_nio.h"
#include <stdio.h>
#include "debug.h"
#define CREDENTIALS_PATH "../credentials.txt"
#define SELECTOR_INITIAL_ELEMENTS 1024
static bool done = false;
#define DEFAULT_AUTH_METHOD 0x02
uint8_t auth_method = DEFAULT_AUTH_METHOD;
#define DEFAULT_PASSWORD_DISSECTORS_MODE 0
uint8_t password_dissectors = DEFAULT_PASSWORD_DISSECTORS_MODE;

//// METRICS        /////////////////////////////////////////////

//// Historic connections
size_t metrics_historic_connections = 0;

//// Concurrent connections
size_t metrics_concurrent_connections = 0;

//// Max concurrent connections
size_t metrics_max_concurrent_connections = 0;

//// Historic byte transfer
size_t metrics_historic_byte_transfer = 0;

//// Historic auth attempts
size_t metrics_historic_auth_attempts = 0;

//// Historic connections attempts
size_t metrics_historic_connections_attempts = 0;

//// Average bytes per read
size_t total_reads = 0;
size_t metrics_average_bytes_per_read = 0;

//// Average bytes per write
size_t total_writes = 0;
size_t metrics_average_bytes_per_write = 0;

/////////////////////////////////////////////////////////////////

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

struct users users[MAX_USERS];
struct users admins[MAX_USERS];
int nadmins = 0;
int nusers = 0;

int
main(const int argc, const char **argv) {

    printf("Inicializando server...\n");
    /*  New empty args struct           */
    struct socks5args * args = malloc(sizeof(struct socks5args));

    if(args == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(args, 0, sizeof(*args));


    /*  Get configurations and users    */
    // TODO Ver el casteo este
    int parse_args_result = parse_args(argc, (char *const *)argv, args);

    if(parse_args_result == -1){
        free(args);
        exit(1);
    }

    FILE* filePointer;
    int bufferLength = 255;
    size_t aux;
    char line[bufferLength];
    char *pass, *user, *newUsername, *newPassword;
    debug("MAIN", 0, "Opening credentials", 0);
    filePointer = fopen((const char *)args->credentials, "r");
    if( filePointer == NULL ) {
        fprintf(stderr, "Couldn't open credentials file (%s) (Use -f [file]): %s\n", args->credentials, strerror(errno));
        exit(1);
    }

    while(fgets(line, bufferLength, filePointer)) {
        pass = strchr(line, ' ');
        *pass = 0;
        pass++;
        user = line;
        aux = strlen(pass);
        if(pass[aux-1] == '\n')
            pass[--aux] = 0;
        newUsername = malloc(strlen(user) + 1);
        newPassword = malloc( aux + 1);
        strcpy(newUsername, user);
        strcpy(newPassword, pass);
        admins[nadmins].name = newUsername;
        admins[nadmins++].pass = newPassword;
    }

    fclose(filePointer);


    /* Debugging */
    char * etiqueta = "MAIN";
    int debug_option = args->debug;
    debug_init(debug_option);

    // no tenemos nada que leer de stdin
    close(0);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    //// Creación de sockets master /////////////////////////////////////////////////////////////////
    debug(etiqueta, 0, "Starting master sockets creation", 0);
    int socket6 = -1;
    int socket = -1;
    if(args->socks_family == AF_UNSPEC){
        debug(etiqueta, AF_UNSPEC, "Proxy SOCKS address unspecified -> IPv4 and IPv6 socket", 0);

        //// Creo sockets para IPv4 y IPv6
        socket = create_socket(AF_INET, &(args->socks_addr_info), NULL);
        if(socket == -1){
            err_msg = "Error creating IPv4 socket";
            goto finally;
        }

        socket6 = create_socket(AF_INET6, NULL, &args->socks_addr_info6);
        if(socket6 == -1){
            err_msg = "Error creating IPv6 socket";
            goto finally;
        }

        debug(etiqueta, 0, args->socks_addr, args->socks_port);
        debug(etiqueta, 0, args->socks_addr_6, args->socks_port);
    }
    if(args->socks_family == AF_INET){

        debug(etiqueta, AF_UNSPEC, "IPv4 Proxy SOCKS address -> Creating socket", 0);

        //// Creo sockets para IPv4
        socket = create_socket(AF_INET, &args->socks_addr_info, NULL);
        if(socket == -1){
            err_msg = "Error creating IPv4 socket";
            goto finally;
        }

        debug(etiqueta, 0, args->socks_addr, args->socks_port);
    }
    if(args->socks_family == AF_INET6){

        debug(etiqueta, AF_UNSPEC, "IPv6 Proxy SOCKS address -> Creating socket", 0);

        //// Creo sockets para IPv6
        socket6 = create_socket(AF_INET6, NULL, &args->socks_addr_info6);
        if(socket6 == -1){
            err_msg = "Error creating IPv6 socket";
            goto finally;
        }

        debug(etiqueta, 0, args->socks_addr_6, args->socks_port);
    }
    debug(etiqueta, 0, "Master sockets creation finished.", 0);

    ////////////////////////////////////////////////////////////////////////////////////////////////////

    //// Creación de sockets master de management /////////////////////////////////////////////////////////////////
    debug(etiqueta, 0, "Starting management master sockets creation", 0);
    int mng_socket6 = -1;
    int mng_socket = -1;
    if(args->mng_family == AF_UNSPEC){
        debug(etiqueta, AF_UNSPEC, "Management address unspecified -> IPv4 and IPv6 socket", 0);

        //// Creo sockets para IPv4 y IPv6
        mng_socket = create_socket(AF_INET, &(args->mng_addr_info), NULL);
        if(mng_socket == -1){
            err_msg = "Error creating IPv4 socket";
            goto finally;
        }

        mng_socket6 = create_socket(AF_INET6, NULL, &args->mng_addr_info6);
        if(mng_socket6 == -1){
            err_msg = "Error creating IPv6 socket";
            goto finally;
        }

        debug(etiqueta, 0, args->mng_addr, args->mng_port);
        debug(etiqueta, 0, args->mng_addr_6, args->mng_port);
    }
    if(args->mng_family == AF_INET){

        debug(etiqueta, AF_UNSPEC, "IPv4 management address -> Creating socket", 0);

        //// Creo sockets para IPv4
        mng_socket = create_socket(AF_INET, &args->mng_addr_info, NULL);
        if(mng_socket == -1){
            err_msg = "Error creating IPv4 socket";
            goto finally;
        }

        debug(etiqueta, 0, args->mng_addr, args->mng_port);
    }
    if(args->mng_family == AF_INET6){

        debug(etiqueta, AF_UNSPEC, "IPv6 management address -> Creating socket", 0);

        //// Creo sockets para IPv6
        mng_socket6 = create_socket(AF_INET6, NULL, &args->mng_addr_info6);
        if(mng_socket6 == -1){
            err_msg = "Error creating IPv6 socket";
            goto finally;
        }

        debug(etiqueta, 0, args->mng_addr_6, args->mng_port);
    }
    debug(etiqueta, 0, "Master management sockets creation finished.", 0);

    ////////////////////////////////////////////////////////////////////////////////////////////////////


    // registrar sigterm es Util para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);


    debug(etiqueta, 0, "Starting selector creation", 0);
    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            },
    };
    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(SELECTOR_INITIAL_ELEMENTS);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }
    debug(etiqueta, 0, "Selector created", 0);


    //// Registro los master sockets con interes en leer
    debug(etiqueta, 0, "Registering master sockets", 0);
    const struct fd_handler socksv5 = {
            .handle_read       = socksv5_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };
    if(args->socks_family == AF_UNSPEC){
        ss |= selector_register(selector, socket6, &socksv5,OP_READ, NULL);
        debug(etiqueta, ss, "Registered IPv6 master socket on selector", socket6);
        ss |= selector_register(selector, socket, &socksv5,OP_READ, NULL);
        debug(etiqueta, ss, "Registered IPv4 master socket on selector", socket);
    }
    if(args->socks_family == AF_INET){
        ss |= selector_register(selector, socket, &socksv5,OP_READ, NULL);
        debug(etiqueta, ss, "Registering IPv4 master socket on selector", 0);
    }
    if(args->socks_family == AF_INET6){
        ss |= selector_register(selector, socket6, &socksv5,OP_READ, NULL);
        debug(etiqueta, ss, "Registering IPv6 master socket on selector", 0);
    }
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }
    debug(etiqueta, 0, "Done registering master sockets", 0);

    //// Registro los management master sockets con interes en leer
    debug(etiqueta, 0, "Registering master sockets", 0);
    const struct fd_handler mng = {
            .handle_read       = mng_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };
    if(args->mng_family == AF_UNSPEC){
        ss |= selector_register(selector, mng_socket6, &mng,OP_READ, NULL);
        debug(etiqueta, ss, "Registered IPv6 master socket on selector", socket6);
        ss |= selector_register(selector, mng_socket, &mng,OP_READ, NULL);
        debug(etiqueta, ss, "Registered IPv4 master socket on selector", socket);
    }
    if(args->mng_family == AF_INET){
        ss |= selector_register(selector, mng_socket, &mng,OP_READ, NULL);
        debug(etiqueta, ss, "Registering IPv4 master socket on selector", 0);
    }
    if(args->mng_family == AF_INET6){
        ss |= selector_register(selector, mng_socket6, &mng,OP_READ, NULL);
        debug(etiqueta, ss, "Registering IPv6 master socket on selector", 0);
    }
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering mng fd";
        goto finally;
    }
    debug(etiqueta, 0, "Done registering mng master sockets", 0);


    debug(etiqueta, 0, "Starting selector iteration", 0);
    for(;!done;) {
        debug(etiqueta, 0, "New selector cycle", 0);
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }
    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
    finally:
    debug(etiqueta, 0, "Finally", 0);
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                ss == SELECTOR_IO
                ? strerror(errno)
                : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();
    mng_pool_destroy();
    socksv5_pool_destroy();

    /* Debugging */
    if(debug_option == FILE_DEBUG){
        debug_file_close();
    }

    free(args);

    if(socket >= 0) {
        close(socket);
    }
    if(socket6 >= 0) {
        close(socket6);
    }

    for (int i = 0; i < nadmins; ++i) {
        free(admins[i].name);
        free(admins[i].pass);
    }

    return ret;
}
