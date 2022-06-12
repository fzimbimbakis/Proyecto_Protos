#ifndef PROYECTO_PROTOS_REQUEST_H
#define PROYECTO_PROTOS_REQUEST_H


#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "buffer.h"
#include "selector.h"
//#include "states.h"
#include "socks5nio.h"
#include "stm.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include "buffer.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#define MAX_FQDN_SIZE 0xFF
#define MSG_NOSIGNAL      0x2000  /* don't raise SIGPIPE */

enum request_state
{
    request_version,
    request_cmd,
    request_rsv,
    request_atyp,
    request_dest_addr,
    request_dest_addr_fqdn,
    request_dest_port,

    // done section
    request_done,

    //error section
    request_error,
    request_error_unsupported_cmd,
    request_error_unsupported_type,
    request_error_unsupported_version,
};

enum socks_cmd
{
    socks_req_cmd_connect = 0x01,
    socks_req_cmd_bind = 0x02,
    socks_req_cmd_associate= 0x03,
};

enum socks_atyp
{
    socks_req_addrtype_ipv4 = 0x01,
    socks_req_addrtype_domain = 0x03,
    socks_req_addrtype_ipv6 = 0x04,
};


struct sockaddr_fqdn{
    char host[MAX_FQDN_SIZE];
    ssize_t size;
};

union socks_addr
{
    struct sockaddr_fqdn fqdn;
    struct sockaddr_in ipv4;
    struct sockaddr_in6 ipv6;
};

struct request
{
    enum socks_cmd cmd;
    enum socks_atyp dest_addr_type;
    union socks_addr dest_addr;
    in_port_t dest_port;
};

typedef struct request_parser
{
    struct request *request;

    enum request_state state;
    //bytes que faltan leer del campo que hay que leer dependiendo del estado
    uint8_t remaining;
    //bytes leidos
    uint8_t read;
} request_parser;




/** inicializa el parser **/
void request_parser_init(request_parser *p);

/** cierra el parser **/
void request_parser_close(struct request_parser* parser);

/** inicializa el request_st **/
void request_init(const unsigned state, struct selector_key *key);

void request_close(const unsigned state, struct selector_key *key);

/** lee el request enviado por el cliente
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


/** manda la respuesta al respectivo request
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
 * TODO: pasarlas a connect.h
 * @param key
 * @param d
 * @return
 */
//unsigned request_connect(struct selector_key *key, struct request_st *d);
//unsigned request_connecting(struct selector_key *key);

/** function to use on a thread to resolv dns without blocking**/
void * request_resolv_blocking(void *data);

unsigned request_resolv_done(struct selector_key *key);

bool request_is_done(const enum request_state state, bool *error);

/** ensambla la respuesta del request dentro del buffer con el metodo
 * seleccionado.
**/
//int request_marshal(buffer *b, const enum socks_reply_status status, const enum socks_atyp atyp, const union socks_addr addr, const in_port_t dest_port);

//enum socks_reply_status errno_to_socks(int e);


#endif //PROYECTO_PROTOS_REQUEST_H
