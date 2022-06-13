#include "../../include/request_parser.h"
#include "debug.h"
#include "request.h"
#include <string.h>
#include <netinet/in.h>

void set_port(request_parser *parser);

enum request_state dest_port(request_parser *p, uint8_t b);

static enum request_state dest_addr(request_parser *p, uint8_t b);

static enum request_state dest_address_fqdn(request_parser *p, uint8_t b);

static void ipv4_address_init(struct sockaddr_in *addr);

static void ipv6_address_init(struct sockaddr_in6 *addr);

static int remaining_is_done(request_parser *p);

static void remaining_set(request_parser *p, int n);

static enum request_state address_type(request_parser *p, uint8_t b);

static enum request_state cmd(request_parser *p, uint8_t b);

static enum request_state version(uint8_t b);

void request_parser_init(struct request_parser *parser) {
    parser->state = request_version;
    parser->read = 0;
    parser->remaining = 0;
    parser->request = malloc(sizeof(parser->request));
}

void request_parser_close(struct request_parser *parser) {
//    free(parser->request);
//    parser->request = NULL;
}

enum request_state request_parser_feed(struct request_parser *parser, uint8_t b) {
    char *etiqueta = "REQUEST PARSER FEED";
    debug(etiqueta, 0, "Starting stage", 0);

    enum request_state following_state = request_error;
    switch (parser->state) {

        case request_version:
            debug(etiqueta, b, "Reading version", 0);
            following_state = version(b);
            break;

        case request_cmd:
            debug(etiqueta, b, "Reading command", 0);
            following_state = cmd(parser, b);
            break;

        case request_rsv:
            debug(etiqueta, b, "Reserved space", 0);
            following_state = request_atyp; //ignoro el campo rsv ya que siempre es '00'
            break;

        case request_atyp:
            debug(etiqueta, b, "Reading address type", 0);
            following_state = address_type(parser, b);
            break;

        case request_dest_addr:
            debug(etiqueta, b, "Reading address", 0);
            following_state = dest_addr(parser, b);
            break;

        case request_dest_addr_fqdn:
            debug(etiqueta, b, "Reading FQDN size", 0);
            following_state = dest_address_fqdn(parser, b);
            break;

        case request_dest_port:
            debug(etiqueta, b, "Reading port", 0);
            following_state = dest_port(parser, b);
            break;

        case request_done:
            debug(etiqueta, b, "Done parsing", 0);
            following_state = request_done;
            break;

        case request_error_unsupported_version:
            debug(etiqueta, b, "request_error_unsupported_version", 0);
            following_state = request_error_unsupported_version;
            break;

        case request_error_unsupported_cmd:
            debug(etiqueta, b, "request_error_unsupported_cmd", 0);
            following_state = request_error_unsupported_cmd;
            break;

        case request_error_unsupported_type:
            debug(etiqueta, b, "request_error_unsupported_type", 0);
            following_state = request_error_unsupported_type;
            break;

        case request_error:
            debug(etiqueta, b, "request_error", 0);
            following_state = request_error;
            break;
    }

    parser->state = following_state;
    return parser->state;
}

static enum request_state version(uint8_t b) {
    enum request_state next;
    if (b == 0x05)
        next = request_cmd;
    else
        next = request_error_unsupported_version;

    return next;
}

static enum request_state cmd(request_parser *p, uint8_t b) {
    enum request_state next;
    if (b == socks_req_cmd_connect) {
        p->request->cmd = b;
        next = request_rsv;
    } else
        next = request_error_unsupported_cmd;

    return next;
}

static enum request_state address_type(request_parser *p, uint8_t b) {
    enum request_state next;
    p->request->dest_addr_type = b;
    switch (p->request->dest_addr_type) {
        case socks_req_addrtype_ipv4:
            remaining_set(p, IPV4_LEN);
            struct sockaddr_in *address = (struct sockaddr_in *) &(p->request->dest_addr);
            ipv4_address_init(address);
            next = request_dest_addr;
            break;
        case socks_req_addrtype_domain:
            next = request_dest_addr_fqdn;
            break;
        case socks_req_addrtype_ipv6:
            remaining_set(p, IPV6_LEN);
            struct sockaddr_in6 *address6 = (struct sockaddr_in6 *) &(p->request->dest_addr);
            ipv6_address_init(address6);
            next = request_dest_addr;
            break;
        default:
            next = request_error_unsupported_type;
            break;
    }
    return next;
}

static void remaining_set(request_parser *p, const int n) {
    p->remaining = n;
    p->read = 0;
}

static int remaining_is_done(request_parser *p) {
    return p->read >= p->remaining;
}

static void ipv6_address_init(struct sockaddr_in6 *addr) {
    memset(addr, 0, sizeof(*addr));
    addr->sin6_family = AF_INET6;
}

static void ipv4_address_init(struct sockaddr_in *addr) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
}

static enum request_state dest_address_fqdn(request_parser *p, uint8_t b) {
    if (b > MAX_FQDN_SIZE)
        return request_error;

    remaining_set(p, b);
    p->request->fqdn.host[p->remaining - 1] = 0;
    p->request->fqdn.size = b;

    return request_dest_addr;
}

static enum request_state dest_addr(request_parser *p, uint8_t b) {
//    enum request_state next;
    switch (p->request->dest_addr_type) {
        case socks_req_addrtype_ipv4: {
            struct sockaddr_in *address = (struct sockaddr_in *) &(p->request->dest_addr);
            ((uint8_t *) (&address->sin_addr))[p->read++] = b;
            break;
        }
        case socks_req_addrtype_domain: {
            p->request->fqdn.host[p->read++] = b;
            break;
        }
        case socks_req_addrtype_ipv6: {
            struct sockaddr_in6 *address6 = (struct sockaddr_in6 *) &(p->request->dest_addr);
            ((uint8_t *) (&address6->sin6_addr))[p->read++] = b;
            break;
        }
    }

    //// Done?
    if (remaining_is_done(p)) {
        remaining_set(p, 2); //el puerto se manda en 2 bytes
        p->request->dest_port = 0;
        return request_dest_port;

    } else
        return request_dest_addr;
}

enum request_state dest_port(request_parser *p, uint8_t b) {
    //// Read one uint8_t to a uint16_t
    *(((uint8_t *) &(p->request->dest_port)) + p->read) = b;
    p->read++;

    if (remaining_is_done(p)) {
        set_port(p);
        return request_done;
    } else
        return request_dest_port;
}

void set_port(request_parser *parser) {
    switch (parser->request->dest_addr_type) {
        case socks_req_addrtype_ipv4: {
            struct sockaddr_in *address = (struct sockaddr_in *) &(parser->request->dest_addr);
            address->sin_port = parser->request->dest_port;
            break;
        }

        case socks_req_addrtype_domain:
            break;

        case socks_req_addrtype_ipv6: {
            struct sockaddr_in6 *address6 = (struct sockaddr_in6 *) &(parser->request->dest_addr);
            address6->sin6_port = parser->request->dest_port;
            break;
        }

    }
}
