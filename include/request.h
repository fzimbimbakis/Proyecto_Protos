#ifndef PROYECTO_PROTOS_REQUEST_H
#define PROYECTO_PROTOS_REQUEST_H

#define MAX_FQDN_SIZE 0xFF
#define MSG_NOSIGNAL      0x2000  /* don't raise SIGPIPE */

#include "buffer.h"
#include "selector.h"
#include "request_parser.h"
#include "states.h"
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * Posibles comados socks
 */
typedef enum socks_cmd
{
    socks_req_cmd_connect = 0x01,
    socks_req_cmd_bind = 0x02,
    socks_req_cmd_associate= 0x03,
}socks_cmd;

/**
 * Posibles tipos de address en la request SOCKS
 */
typedef enum socks_atyp
{
    socks_req_addrtype_ipv4 = 0x01,
    socks_req_addrtype_domain = 0x03,
    socks_req_addrtype_ipv6 = 0x04,
}socks_atyp;

/**
 * Estructura para que el parser guarde la address en caso de FQDN
 */
struct sockaddr_fqdn{
    char host[MAX_FQDN_SIZE];
    ssize_t size;
};

/**
 * Estructura con información del request
 */
struct request
{
    enum socks_cmd cmd;
    enum socks_atyp dest_addr_type;
    union {
        struct sockaddr_storage dest_addr;
        struct sockaddr_fqdn fqdn;
    };
    in_port_t dest_port;
};


/**
 * Inicializa el request_st
 * @param state
 * @param key
 */
void request_init(const unsigned state, struct selector_key *key);

/**
 * Cierra recursos
 * @param state
 * @param key
 */
void request_close(const unsigned state, struct selector_key *key);

/**
 * request_read
 * @param key
 * @return
 *
 * Lee el request enviado por el cliente
 *
 *  The SOCKS request is formed as follows:

        +----+-----+-------+------+----------+----------+
        |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
        +----+-----+-------+------+----------+----------+
        | 1  |  1  | X'00' |  1   | Variable |    2     |
        +----+-----+-------+------+----------+----------+

     Where:

          o  VER    protocol version: X'05'
          o  CMD
             o  CONNECT X'01'
             o  BIND X'02'
             o  UDP ASSOCIATE X'03'
          o  RSV    RESERVED
          o  ATYP   address type of following address
             o  IP V4 address: X'01'
             o  DOMAINNAME: X'03'
             o  IP V6 address: X'04'
          o  DST.ADDR       desired destination address
          o  DST.PORT desired destination port in network octet
             order
 */
unsigned request_read(struct selector_key *key);


/**
 * request_write
 * @param key
 * @return
 *
 * * manda la respuesta al respectivo request
 *
 *    +----+-----+-------+------+----------+----------+
        |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
        +----+-----+-------+------+----------+----------+
        | 1  |  1  | X'00' |  1   | Variable |    2     |
        +----+-----+-------+------+----------+----------+

     Where:

          o  VER    protocol version: X'05'
          o  REP    Reply field:
             o  X'00' succeeded
             o  X'01' general SOCKS server failure
             o  X'02' connection not allowed by ruleset
             o  X'03' Network unreachable
             o  X'04' Host unreachable
             o  X'05' Connection refused
             o  X'06' TTL expired
             o  X'07' Command not supported
             o  X'08' Address type not supported
             o  X'09' to X'FF' unassigned
          o  RSV    RESERVED
          o  ATYP   address type of following address
             o  IP V4 address: X'01'
             o  DOMAINNAME: X'03'
             o  IP V6 address: X'04'
          o  BND.ADDR       server bound address
          o  BND.PORT       server bound port in network octet order
 */
unsigned request_write(struct selector_key *key);



/** procesa el request ya parsead0 **/
unsigned request_process(struct selector_key *key, struct request_st *d);

/** entrega un byte al parser **/
enum request_state request_parser_feed(request_parser *p, uint8_t b);

/** consume los bytes del mensaje del cliente y se los entrega al parser
 * hasta que se termine de parsear
**/
enum request_state request_consume(buffer *b, request_parser *p, bool *error);

/**
 * Checks if the parser has reached the final state
 * @param state
 * @param error
 * @return
 */
bool request_is_done(const enum request_state state, bool *error);

/**
 * Ensambla la respuesta dentro del buffer b con el status recibido
 * @param status
 * @param b
 * @return
 */
int request_marshall(int status, buffer * b);



#endif //PROYECTO_PROTOS_REQUEST_H
