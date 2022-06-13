#ifndef PROYECTO_PROTOS_SOCKS5NIO_H
#define PROYECTO_PROTOS_SOCKS5NIO_H


#include "states.h"
#include <sys/socket.h>
#include "stm.h"
#include "selector.h"

/**handler del socket pasivo que atiende conexiones socksv5 */

void
socksv5_passive_accept(struct selector_key *key);

/** libreria de pools internos*/
void
socksv5_pool_destroy(void);

/* declaraciÃ³n forward de los handlers de selecciÃ³n de una conexiÃ³n
 * establecida entre un cliente y el proxy.
 */

void socksv5_read   (struct selector_key *key);
void socksv5_write  (struct selector_key *key);
void socksv5_block  (struct selector_key *key);
void socksv5_close  (struct selector_key *key);



/** maquina de estados general */
enum socks_v5state {
    /**
     * recibe el mensaje `hello` del cliente, y lo procesa
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - HELLO_READ  mientras el mensaje no estÃ© completo
     *   - HELLO_WRITE cuando estÃ¡ completo
     *   - ERROR       ante cualquier error (IO/parseo)
     */
    HELLO_READ,

    /**
     * envÃ­a la respuesta del `hello' al cliente.
     *
     * Intereses:
     *     - OP_WRITE sobre client_fd
     *
     * Transiciones:
     *   - HELLO_WRITE  mientras queden bytes por enviar
     *   - REQUEST_READ cuando se enviaron todos los bytes
     *   - ERROR        ante cualquier error (IO/parseo)
     */
    HELLO_WRITE,

    //AUTH,
    USERPASS_READ,
    USERPASS_WRITE,

    REQUEST_READ,
    REQUEST_RESOLV,
    REQUEST_CONNECTING,
    REQUEST_WRITE,
    COPY,



    // estados terminales
    DONE,
    ERROR,
};






/**
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una Ãºnica
 * alocaciÃ³n cuando recibimos la conexiÃ³n.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */
typedef struct socks5 {
    /** informacion del cliente */
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    int client_fd;

    /** resolucion de la direccion de origen del server */
    struct addrinfo *origin_resolution;

    /** intento actual de la direccion del origin server */
    struct addrinfo *origin_resolution_current;

    /**informacion del origin server */
    struct sockaddr_storage origin_addr;
    socklen_t origin_addr_len;
    int origin_domain;
    int origin_fd;

    /** maquinas de estados */
    struct state_machine          stm;

    /** estados para el client_fd */
    union {
        struct hello_st           hello;
        struct userpass_st        userpass;
        struct request_st         request;
        struct copy_st               copy;
    } client;
    /** estados para el origin_fd */
    union {
        struct connecting         conn;
        struct copy_st               copy;
    } orig;

    //TODO(facu): ver tamaño de buffer mas conveniente
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    buffer read_buffer, write_buffer;

    /** cantidad de referencias a este objeto, si es una se debe destruir */
    unsigned references;

    /** siguiente en el pool */
    struct socks5 *next;

    /** Authentication result **/
    uint8_t authentication;

}socks5;


/**
 * Par username password
 */
struct users {
    char *name;
    char *pass;
};
#define MAX_USERS 10

/** obtiene el struct (socks5 *) desde la llave de selecciÃ³n  */
#define ATTACHMENT(key) ( (struct socks5 *)(key)->data)
#endif //PROYECTO_PROTOS_SOCKS5NIO_H
