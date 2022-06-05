#ifndef PROYECTO_PROTOS_REQUEST_H
#define PROYECTO_PROTOS_REQUEST_H


#include <netinet/in.h>
#include <arpa/inet.h>
#include "buffer.h"

#define MAX_FQDN_SIZE 0xFF

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

union socks_addr
{
    char fqdn[MAX_FQDN_SIZE];
    struct sockaddr_in ipv4;
    struct sockaddr_in6 ipv6;
};

struct request
{
    enum socks_req_cmd cmd;
    enum socks_addr_type dest_addr_type;
    union socks_addr dest_addr;
    in_port_t dest_port;
};

typedef struct request_parser
{
    struct request *request;

    enum request_state state;
    //bytes que faltan leer
    uint8_t n;
    //bytes leidos
    uint8_t i;
} request_parser;

enum socks_reply_status
{
    status_succeeded = 0x00,
    status_general_socks_server_failure = 0x01,
    status_connection_not_allowed_by_ruleset = 0x02,
    status_network_unreachable = 0x03,
    status_host_unreachable = 0x04,
    status_connection_refused = 0x05,
    status_ttl_expired = 0x06,
    status_command_not_supported = 0x07,
    status_address_type_not_supported = 0x08,
};


/** inicializa el parser **/
void request_parser_init(request_parser *p);

/** entrega un byte al parser. Retorna true si se llego al final **/
enum request_state request_parser_feed(request_parser *p, uint8_t b);

/** consume los bytes del mensaje del cliente y se los entrega al parser
 * hasta que se termine de parsear
**/
enum request_state request_consume(buffer *b, request_parser *p, bool *error);

bool request_is_done(const enum request_state state, bool *error);

/** ensambla la respuesta del request dentro del buffer con el metodo
 * seleccionado.
**/
int request_marshal(buffer *b, const enum socks_reply_status status, const enum socks_atyp atyp, const union socks_addr addr, const in_port_t dest_port);

enum socks_reply_status errno_to_socks(int e);

#include <netdb.h>
#include <arpa/inet.h>

enum socks_reply_status cmd_resolve(struct request *request, struct sockaddr **originaddr, socklen_t *originlen, int *domain);
#endif //PROYECTO_PROTOS_REQUEST_H
