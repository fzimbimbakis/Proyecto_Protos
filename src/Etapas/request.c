#include <sys/errno.h>
#include "../../include/request.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "buffer.h"
#include "selector.h"
#include "states.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include "request_parser.h"
#include "resolv.h"
#define IPV4_LEN 4
#define IPV6_LEN 16
#ifndef MSG_NOSIGNAL
//// For mac compilation only
#define MSG_NOSIGNAL 0x2000  /* don't raise SIGPIPE */
#endif



enum socks_v5state error_handler(enum socks_reply_status status, struct selector_key *key ){
    struct socks5 * data= ATTACHMENT(key);
    request_marshall(status, &data->write_buffer);
    selector_set_interest_key(key, OP_WRITE);
    return REQUEST_WRITE;
}

enum socks_v5state error_handler_to_client(enum socks_reply_status status, struct selector_key *key ){
    struct socks5 * data= ATTACHMENT(key);
    request_marshall(status, &data->write_buffer);
    selector_set_interest(key->s, data->client_fd, OP_WRITE);
    if(data->origin_fd!=-1)
        selector_set_interest(key->s, data->origin_fd, OP_NOOP);

    return REQUEST_WRITE;
}

//// INIT
void
request_init(const unsigned state, struct selector_key *key)
{
    char * etiqueta = "REQUEST INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct request_st *d = &ATTACHMENT(key)->client.request;

    //// Buffers
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb= &(ATTACHMENT(key)->write_buffer);

    //// Status
    d->status=status_succeeded;

    //// Parser
    d->parser = malloc(sizeof(*(d->parser)));
    if(d->parser == NULL){
        d->status=status_general_socks_server_failure;
        error_handler(d->status, key);
    }
    if (request_parser_init(d->parser) < 0){
        d->status=status_general_socks_server_failure;
        error_handler(d->status, key);
    }
    d->parser->request = malloc(sizeof(*d->parser->request));
    if(d->parser->request == NULL){
        d->status=status_general_socks_server_failure;
        error_handler(d->status, key);
    }

    //// FDs
    d->client_fd= &ATTACHMENT(key)->client_fd;
    d->origin_fd=&ATTACHMENT(key)->origin_fd;

    //// Origin addr
    d->origin_addr=&ATTACHMENT(key)->origin_addr;
    d->origin_addr_len= &ATTACHMENT(key)->origin_addr_len;
    d->origin_domain=&ATTACHMENT(key)->origin_domain;

    debug(etiqueta, 0, "Finished stage", key->fd);
}

//// CLOSE
void request_close(const unsigned state, struct selector_key *key){
    char * etiqueta = "REQUEST CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    if(ATTACHMENT(key)->client.request.parser->request->dest_addr_type != socks_req_addrtype_domain){
        struct request_st *d = &ATTACHMENT(key)->client.request;
        request_parser_close(d->parser);
        free(d->parser);
        d->parser=NULL;
    }
    debug(etiqueta, 0, "Finished stage", key->fd);
}

//// READ
unsigned request_read(struct selector_key *key)
{


    char * etiqueta = "REQUEST READ";
    debug(etiqueta, 0, "Starting stage", key->fd);

    struct request_st *d = &ATTACHMENT(key)->client.request;

    if(d->status != status_succeeded){ //por si fallo en el init
        return REQUEST_WRITE;
    }

    buffer *b = d->rb;
    unsigned ret = REQUEST_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(b, &count);

    debug(etiqueta, 0, "Leo del cliente", key->fd);

    n = recv(key->fd, ptr, count, 0);
    if (n > 0)
    {
        buffer_write_adv(b, n);
        int st = request_consume(b, d->parser, &error);
        if (request_is_done(st, &error))
        {
            debug(etiqueta, 0, "Finished reading -> Start processing", key->fd);
            ret = request_process(key, d);
        }
    }
    else
    {
        debug(etiqueta, 0, "Read interest with nothing to read", key->fd);
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

enum request_state request_consume(buffer *b, struct request_parser *p, bool *error)
{
    char * etiqueta = "REQUEST CONSUME";
    debug(etiqueta, 0, "Starting stage", 0);

    enum request_state st = p->state;

    bool finished = false;
    while (buffer_can_read(b) && !finished)
    {
        uint8_t byte = buffer_read(b);
        st = request_parser_feed(p, byte);
        if (request_is_done(st, error))
        {
            finished = true;
        }
    }
    debug(etiqueta, 0, "Finished stage", 0);
    return st;
}

bool request_is_done(const enum request_state state, bool *error){
    if(state == request_done && !*error)
        return true;
    return false;
}

//// PROCESS
unsigned request_process(struct selector_key *key, struct request_st *d)
{
    char* etiqueta="REQUEST PROCESS";
    debug(etiqueta, 0, "Starting stage", key->fd);

    unsigned ret;
    pthread_t tid;
    struct socks5 * data = ATTACHMENT(key);

    if(d->parser->request->cmd != socks_req_cmd_connect){
        debug(etiqueta, 0, "COMMAND NOT SUPPORTED", key->fd);
        d->status=status_command_not_supported;
        //data->orig.conn.status = status_general_socks_server_failure;
        return error_handler(d->status, key);
    }

//    int family;
//    uint16_t port;
//    struct sockaddr_storage * addr;

// esto mejoraría enormemente de haber usado sockaddr_storage en el request
    switch (d->parser->request->dest_addr_type) {

        //// IPv4
        case socks_req_addrtype_ipv4: {
            d->addr_family = socks_req_addrtype_ipv4;
            debug(etiqueta, 0, "IPV4", key->fd);
            struct sockaddr_in * addr4 = (struct sockaddr_in *) &(data->client.request.parser->request->dest_addr);
            data->origin_domain = AF_INET;
            data->origin_addr_len = sizeof(struct sockaddr);
            memcpy((struct sockaddr_in *) &(data->origin_addr), addr4, sizeof(*addr4));

            debug(etiqueta, 0, "Going to REQUEST_CONNECTING", key->fd);
            ret=REQUEST_CONNECTING;
            break;
        }

        //// IPv6
        case socks_req_addrtype_ipv6: {
            d->addr_family = socks_req_addrtype_ipv6;
            debug(etiqueta, 0, "IPV6", key->fd);
            struct sockaddr_in6 * addr6 = (struct sockaddr_in6 *) &(data->client.request.parser->request->dest_addr);
            data->origin_domain = AF_INET6;
            data->origin_addr_len = sizeof(struct sockaddr_in6);
            memcpy((struct sockaddr_in6 *) &(data->origin_addr), addr6, sizeof(*addr6));

            debug(etiqueta, 0, "Going to REQUEST_CONNECTING", key->fd);
            ret=REQUEST_CONNECTING;
            break;
            }

        //// FQDN
        case socks_req_addrtype_domain: {
            d->addr_family = socks_req_addrtype_domain;
            debug(etiqueta, 0, "FQDN", key->fd);
            struct selector_key *k= malloc(sizeof (*key));

                if(k==NULL){
                    debug(etiqueta, 0, "Malloc error -> REQUEST_WRITE to reply error to client", key->fd);
                    //data->orig.conn.status = status_general_socks_server_failure;
                    d->status=status_general_socks_server_failure;
                    return error_handler(d->status, key);
                }

                debug(etiqueta, 0, "Creating thread for FQDN resolve", key->fd);
                memcpy(k, key, sizeof(*k));
                if(-1 == pthread_create(&tid, 0, request_resolv_blocking, k)){
                    debug(etiqueta, 0, "Error creating thread for FQDN resolve -> REQUEST_WRITE to reply error to client", key->fd);
                    free(k);
                    d->status=status_general_socks_server_failure;
                    //data->orig.conn.status = status_general_socks_server_failure;
                    return error_handler(d->status, key);
                }

                debug(etiqueta, 0, "Passing to REQUEST_RESOLV (OP_NOOP interest) state to wait FQDN resolve", key->fd);
                ret=REQUEST_RESOLV;
                selector_set_interest_key(key, OP_NOOP);

                break;
            }
            default: {
                debug(etiqueta, 0, "Address type not supported -> REQUEST_WRITE to reply error to client", key->fd);
                d->status = status_address_type_not_supported;
                return error_handler(d->status, key);
            }

    }

    return ret;
}

//// WRITE
unsigned request_write(struct selector_key *key)
{
    char * etiqueta = "REQUEST WRITE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct request_st *d = &ATTACHMENT(key)->client.request;

    buffer *b = d->wb;
    unsigned ret = REQUEST_WRITE;
    uint8_t *ptr;
    size_t count;
    ssize_t n;
    ptr = buffer_read_ptr(b, &count);
    debug(etiqueta, count, "Writing to client", key->fd);
    //signal(SIGPIPE, SIG_IGN);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);
    if (n == -1){
        debug(etiqueta, n, strerror(errno), 0);
        ret = ERROR;
    }else{
        debug(etiqueta, n, "Wrote to client", key->fd);
        buffer_read_adv(b, n);
        if (!buffer_can_read(b))
        {
            if (ATTACHMENT(key)->orig.conn.status ==status_succeeded)
            {
                ret = COPY;
                selector_set_interest(key->s, *d->client_fd, OP_READ);
                selector_set_interest(key->s, *d->origin_fd, OP_READ);
            }
            else
            {
                ret = DONE;
                selector_set_interest(key->s, *d->client_fd, OP_NOOP);
                if(-1== *d->origin_fd){
                    selector_set_interest(key->s, *d->origin_fd, OP_NOOP);
                }
            }
        }
    }
    debug(etiqueta, ret, "Finished stage", key->fd);
    return ret;
}

int request_marshall(int status, buffer * b){
    size_t count;
    uint8_t *buff= buffer_write_ptr(b, &count);

    if(count < 10)
        return -1;

    buff[0]=0x05;
    buff[1]= status;
    buff[2]=0x00;//rsv
    buff[3]=socks_req_addrtype_ipv4;
    buff[4]=0x00;
    buff[5]=0x00;
    buff[6]=0x00;
    buff[7]=0x00;
    buff[8]=0x00;
    buff[9]=0x00;
    buffer_write_adv(b, 10);

    return 10;
}
