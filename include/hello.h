#ifndef PROYECTO_PROTOS_HELLO_H
#define PROYECTO_PROTOS_HELLO_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "buffer.h"
#include "selector.h"
#include "states.h"
#include "socks5nio.h"
#include "stm.h"
/**
//    The client connects to the server, and sends a version
//    identifier/method selection message:
//
//                    +----+----------+----------+
//                    |VER | NMETHODS | METHODS  |
//                    +----+----------+----------+
//                    | 1  |    1     | 1 to 255 |
//                    +----+----------+----------+
//
//    The VER field is set to X'05' for this version of the protocol.  The
//    NMETHODS field contains the number of method identifier octets that
//    appear in the METHODS field.
//
//    The server selects from one of the methods given in METHODS, and
//    sends a METHOD selection message:
//
//                          +----+--------+
//                          |VER | METHOD |
//                          +----+--------+
//                          | 1  |   1    |
//                          +----+--------+
//
//    If the selected METHOD is X'FF', none of the methods listed by the
//    client are acceptable, and the client MUST close the connection.
//
//    The values currently defined for METHOD are:
//
//           o  X'00' NO AUTHENTICATION REQUIRED
//           o  X'01' GSSAPI
//           o  X'02' USERNAME/PASSWORD
//           o  X'03' to X'7F' IANA ASSIGNED
//           o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
//           o  X'FF' NO ACCEPTABLE METHODS
//
//    The client and server then enter a method-specific sub-negotiation.
**/

static const uint8_t METHOD_NO_AUTHENTICATION_REQUIRED = 0x00;
static const uint8_t METHOD_NO_ACCEPTABLE_METHODS = 0xFF;
static const uint8_t METHOD_USERNAME_PASSWORD = 0x02;

/**
 * Hello state sub-states
 */
enum hello_state
{
    hello_version,
    hello_nmethods,
    hello_methods,
    hello_done,
    hello_error_unsupported_version,
};
/**
 * Hello parser struct
 */
typedef struct hello_parser{
    /** invocado cada vez que se presenta un nuevo método **/
    void (*on_authentication_method)(void *data, const uint8_t method);

    /** permite al usuario del parser almacenar sus datos **/
    void *data;

    /********* zona privada *********/
    enum hello_state state;
    /* cantidad de metodos que faltan por leer */
    uint8_t remaining;
}hello_parser;

/**
 * Inicializa el parser
 * @param p
 */
void hello_parser_init(struct hello_parser *p);

/**
 * Entrega un byte al parser. Retorna true si se llego al final
 * @param p
 * @param b
 * @return
 */
enum hello_state hello_parser_feed(struct hello_parser *p, uint8_t b);

/**
 * Consume los bytes del mensaje del cliente y se los entrega al parser
 * hasta que se termine de parsear
 * @param b
 * @param p
 * @param error
 * @return
 */
enum hello_state hello_consume(buffer *b, struct hello_parser *p, bool *error);

/**
 * Ensambla la respuesta del hello dentro del buffer con el metodo seleccionado.
 * @param b
 * @param method
 * @return
 */
int hello_marshal(buffer *b, const uint8_t method);

/**
 *
 * @param state
 * @param error
 * @return
 */
bool hello_is_done(const enum hello_state state, bool *error);

/**
 *
 * @param p
 */
void hello_parser_close(struct hello_parser *p);

/**
 * Callback del parser utilizado en 'read_hello'
 * @param p
 * @param method
 */
static void on_hello_method(void *p, uint8_t method);

/**
 * Inicializa las variables de los estados hello_st
 * @param state
 * @param key
 */
void hello_read_init(unsigned state, struct selector_key *key);

/**
 * Lee todos los bytes del mensaje de tipo 'hello' y inicia su proceso
 * @param key
 * @return
 */
unsigned hello_read(struct selector_key *key);

/**
 * Procesamiento del mensaje 'hello'
 * @param d
 * @return
 */
static unsigned hello_process(const struct hello_st* d);

/**
 * Close resources
 * @param state
 * @param key
 */
void hello_read_close(unsigned state, struct selector_key *key);

/**
 * Writes bytes on buffer to client
 * @param key
 * @return
 */
unsigned hello_write(struct selector_key *key);

#endif //PROYECTO_PROTOS_HELLO_H
