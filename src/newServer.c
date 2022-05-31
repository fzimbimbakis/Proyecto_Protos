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
#include <netdb.h>
//#include "socks5.h"
#include "args.h"
#include "selector.h"
//#include "socks5nio.h"

#define MAX_CONNECTIONS_QUEUE 20

#define IP_FOR_REQUESTS "127.0.0.1"
#define PORT_FOR_REQUESTS "3000"

/**
 * Archivo construido tomando main.c de Juan Codagnone de ejemplo
 *
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

static bool done = false;

struct fdPair{
    int fdClient;
    int fdOrigin;
}fdPair;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

void receive(struct selector_key *key){
    printf("Receive %d\n", key->fd);
    char buffer[1025];  //data buffer of 1K
    int valread;

    //Check if it was for closing , and also read the incoming message
    if ((valread = read( key->fd , buffer, 1024)) == 0)
    {
        //Somebody disconnected
        printf("Host disconnected %d\n" , key->fd);

        //Close the socket
        selector_unregister_fd(key->s, key->fd);

    }
    else
    {
        //set the string terminating NULL byte on the end of the data read
        buffer[valread] = '\0';

        // Print data (initial approach)
        printf( "%s %d: %s", "Received", key->fd,  buffer);
        if(((struct fdPair *) (key->data))->fdOrigin != key->fd) {
            printf("Send to %d: %s", ((struct fdPair *) (key->data))->fdOrigin, buffer);
            send(((struct fdPair *) (key->data))->fdOrigin, buffer, strlen(buffer), 0);
        }else{
            printf("Send to %d: %s", ((struct fdPair *) (key->data))->fdClient, buffer);
            send(((struct fdPair *) (key->data))->fdClient , buffer , strlen(buffer) , 0 );
        }
    }
}
struct addrinfo *res;
void socksv5_passive_accept(struct selector_key *key){
    printf("Socks passive accept %d \n", key->fd);

    // TODO Free pair
    struct fdPair *pair = malloc(sizeof(fdPair));

    struct sockaddr_storage client_address;
    int addrlen = sizeof(client_address), new_socket;
    if ((new_socket = accept(key->fd, (struct sockaddr *)&client_address, (socklen_t*)&addrlen))<0)
    {
        perror("accept error");
        exit(EXIT_FAILURE);
    }

    const struct fd_handler socksv5 = {
            .handle_read       = receive,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar TODO cerrar fds
    };
    selector_register(key->s, new_socket, &socksv5, OP_READ, pair);
    pair->fdClient = new_socket;

    int D_new_socket = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    printf( "Connecting...\n");
    if(connect(D_new_socket,res->ai_addr,res->ai_addrlen) != 0){
        printf( "Connect to fixed destination failed.");
        exit(EXIT_FAILURE);
    }
    const struct fd_handler D_socksv5 = {
            .handle_read       = receive,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };
    printf( "Connected to fixed destination!\n");
    selector_register(key->s, D_new_socket, &D_socksv5, OP_READ, pair);
    pair->fdOrigin = D_new_socket;


}
int
main(const int argc, const char **argv) {

    // Create socket to destination
    struct addrinfo hints;
    //get host info, make socket and connect it
    memset(&hints, 0,sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(IP_FOR_REQUESTS, PORT_FOR_REQUESTS, &hints, &res) != 0){
        printf( "getadrrinfo failed");
        exit(EXIT_FAILURE);
    }
    printf("Fixed address obtained\n");

    /*  New empty args struct           */
    socks5args args = malloc(sizeof(socks5args_struct));
    memset(args, 0, sizeof(*args));

    /*  Get configurations and users    */
    parse_args(argc, argv, args);

    // no tenemos nada que leer de stdin
    close(0);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    /* Get addr info                    */
    memset(&(args->socks_addr_info), 0, sizeof((args->socks_addr_info)));
    (args->socks_addr_info).sin_family      = AF_INET;
    (args->socks_addr_info).sin_addr.s_addr = htonl(INADDR_ANY);
    (args->socks_addr_info).sin_port        = htons(args->socks_port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", args->socks_port);

    /* No importa reportar nada si falla, man 7 ip. */
    // TODO(bruno) No se que es el 1.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &(args->socks_addr_info), sizeof(args->socks_addr_info)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, MAX_CONNECTIONS_QUEUE) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }

    /*  Registrar sigterm es Util para terminar el programa normalmente.
        Esto ayuda mucho en herramientas como valgrind.                         */
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    /* Check socket flags?          */
    if(selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }

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

    selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }

    const struct fd_handler socksv5 = {
            .handle_read       = socksv5_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };

    ss = selector_register(selector, server, &socksv5,
                           OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

    for(;!done;) {
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
    // TODO(bruno) Check free of all resources
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

//    socksv5_pool_destroy();

    if(server >= 0) {
        close(server);
    }

    return ret;


}

