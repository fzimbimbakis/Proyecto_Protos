#include <sys/errno.h>
#include "../../include/request.h"

#define IPV4_LEN 4
#define IPV6_LEN 16

void request_parser_init(struct request_parser *p)
{
    p->state = request_version;
    p->read = 0;
    p->remaining=0;
}

void request_parser_close(struct request_parser* parser){
    //TODO: chequear que se libere bien todo
    //free(parser->request);
}


void request_close(const unsigned state, struct selector_key *key){
    char * etiqueta = "REQUEST CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct request_st *d = &ATTACHMENT(key)->client.request;
    request_parser_close(d->parser);
    free(d->parser);
    debug(etiqueta, 0, "Finished stage", key->fd);
}


void
request_init(const unsigned state, struct selector_key *key)
{
    char * etiqueta = "REQUEST INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct request_st *d = &ATTACHMENT(key)->client.request;
//    // Adding the read buffer
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb= &(ATTACHMENT(key)->write_buffer);
    d->parser = malloc(sizeof(*(d->parser)));
    d->parser->request =d->request;
    request_parser_init(d->parser);
    d->status= malloc(sizeof(enum socks_reply_status));
    *(d->status)=status_succeeded;
    d->client_fd= &ATTACHMENT(key)->client_fd;
    d->origin_fd=&ATTACHMENT(key)->origin_fd;

    d->origin_addr=&ATTACHMENT(key)->origin_addr;
    d->origin_addr_len= &ATTACHMENT(key)->origin_addr_len;
    d->origin_domain=&ATTACHMENT(key)->origin_domain;
}


// lee todos los bytes del mensaje de tipo 'request' e inicia su proceso
unsigned request_read(struct selector_key *key)
{

    char * etiqueta = "REQUEST READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct request_st *d = &ATTACHMENT(key)->client.request;

    buffer *b = d->rb;
    unsigned ret = REQUEST_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(b, &count); //trae el puntero de escritura
    debug(etiqueta, 0, "Middle stage", key->fd);
    n = recv(key->fd, ptr, count, 0);
    if (n > 0)
    {
        buffer_write_adv(b, n);
        int st = request_consume(b, d->parser, &error);
        if (request_is_done(st, &error))
        {
            ret = request_process(key, d);
        }
    }
    else
    {
        debug(etiqueta, 0, "Error recv", key->fd);
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
    return st;
}

static void remaining_set(request_parser *p, const int n)
{
    p->remaining = n;
    p->read = 0;
}

static int remaining_is_done(request_parser *p)
{
    return p->read >= p->remaining;
}


static enum request_state version(uint8_t b)
{
    enum request_state next;
    if (b == 0x05)
    {
        next = request_cmd;
    }
    else
    {
        next = request_error_unsupported_version;
    }
    return next;
}

static enum request_state cmd(request_parser *p, uint8_t b)
{
    enum request_state next;
    if (b == socks_req_cmd_connect)
    {
        p->request->cmd = b;
        next = request_rsv;
    }
    else
    {
        next = request_error_unsupported_cmd;
    }
    return next;
}

static enum request_state atyp(request_parser *p, uint8_t b)
{
    enum request_state next;
    p->request->dest_addr_type = b;
    switch (p->request->dest_addr_type)
    {
        case socks_req_addrtype_ipv4:
            remaining_set(p, IPV4_LEN);
            memset(&(p->request->dest_addr.ipv4), 0, sizeof(p->request->dest_addr.ipv4));
            p->request->dest_addr.ipv4.sin_family = AF_INET;
            next = request_dest_addr;
            break;
        case socks_req_addrtype_domain:
            next = request_dest_addr_fqdn;
            break;
        case socks_req_addrtype_ipv6:
            remaining_set(p, IPV6_LEN);
            memset(&(p->request->dest_addr.ipv6), 0, sizeof(p->request->dest_addr.ipv6));
            p->request->dest_addr.ipv6.sin6_family = AF_INET6;
            next = request_dest_addr;
            break;
        default:
            next = request_error_unsupported_type;
            break;
    }
    return next;
}

static enum request_state dest_addr_fqdn(request_parser *p, uint8_t b)
{
    remaining_set(p, b); //The first
    //octet of the address field contains the number of octets of name that
    //follow.
    p->request->dest_addr.fqdn[p->remaining - 1] = 0;
    return request_dest_addr;
}

static enum request_state dest_addr(request_parser *p, uint8_t b)
{
    enum request_state next;
    switch (p->request->dest_addr_type)
    {
        case socks_req_addrtype_ipv4:
            ((uint8_t *)&(p->request->dest_addr.ipv4.sin_addr))[p->read++] = b;
            break;
        case socks_req_addrtype_domain:
            p->request->dest_addr.fqdn[p->read++] = b;
            break;
        case socks_req_addrtype_ipv6:
            ((uint8_t *)&(p->request->dest_addr.ipv6.sin6_addr))[p->read++] = b;
            break;
    }
    if (remaining_is_done(p))
    {
        remaining_set(p, 2); //el puerto se manda en 2 bytes
        p->request->dest_port = 0;
        next = request_dest_port;
    }
    else
    {
        next = request_dest_addr;
    }
    return next;
}

static enum request_state dest_port(request_parser *p, uint8_t b)
{
    enum request_state next = request_dest_port;
    *(((uint8_t *)&(p->request->dest_port)) + p->read) = b;
    p->read++;
    if (remaining_is_done(p))
    {
        next = request_done;
    }
    return next;
}

enum request_state request_parser_feed(request_parser *p, uint8_t b){
    char * etiqueta = "REQUEST PARSER FEED";
    debug(etiqueta, 0, "Starting stage", 0);
    enum request_state following_state=request_error;
    switch (p->state) {
        case request_version:
            debug(etiqueta, 0, "Version", 0);
            following_state= version(b);
            break;
        case request_cmd:
            debug(etiqueta, 0, "Cmd", 0);
            following_state= cmd(p,b);
            break;
        case request_rsv:
            debug(etiqueta, 0, "Rsv", 0);
            following_state= request_atyp; //ignoro el campo rsv ya que siempre es '00'
            break;
        case request_atyp:
            debug(etiqueta, 0, "Atyp", 0);
            following_state= atyp(p,b);
            break;
        case request_dest_addr:
            debug(etiqueta, 0, "Dest addr", 0);
            following_state= dest_addr(p,b);
            break;
        case request_dest_addr_fqdn:
            debug(etiqueta, 0, "Dest addr fqdn", 0);
            following_state= dest_addr_fqdn(p,b);
            break;
        case request_dest_port:
            debug(etiqueta, 0, "Dest port", 0);
            following_state= dest_port(p,b);
            break;
        case request_done:
            debug(etiqueta, 0, "Request done", 0);
            following_state = request_done;
            break;
        case request_error_unsupported_version:
            following_state = request_error_unsupported_version;
            break;
        case request_error_unsupported_cmd:
            following_state = request_error_unsupported_cmd;
            break;
        case request_error_unsupported_type:
            following_state = request_error_unsupported_type;
            break;
        case request_error:
            following_state=request_error;
            break;
    }

    p->state = following_state;
    return p->state;
}

bool request_is_done(const enum request_state state, bool *error){
    if(state == request_done && !*error)
        return true;
    return false;
}
//TODO: no se llega a ver completo en la clase
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
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);
    if (n == -1)
    {
        ret = ERROR;
    }
    else
    {
        buffer_read_adv(b, n);
        if (!buffer_can_read(b))
        {
            if (d->status ==status_succeeded)
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


unsigned request_process(struct selector_key *key, struct request_st *d)
{
    char* etiqueta="REQUEST PROCESS";
    debug(etiqueta, 0, "Starting stage", key->fd);
    unsigned ret;
    pthread_t tid;

    switch (d->request->cmd)
    {
        case socks_req_cmd_connect: {
            // esto mejoraría enormemente de haber usado sockaddr_storage en el request
            switch (d->request->dest_addr_type) {
                case socks_req_addrtype_ipv4: {
                    debug(etiqueta, 0, "IPV4", key->fd);
                    ATTACHMENT(key)->origin_domain = AF_INET;
                    d->request->dest_addr.ipv4.sin_port = d->request->dest_port;
                    ATTACHMENT(key)->origin_addr_len = sizeof(d->request->dest_addr.ipv4);
                    memcpy(&ATTACHMENT(key)->origin_addr, &d->request->dest_addr,
                           sizeof(d->request->dest_addr.ipv4));

                    //ret = request_connect(key, d);
                    ret=REQUEST_CONNECTING;
                    break;
                }
                case socks_req_addrtype_ipv6: {
                    debug(etiqueta, 0, "IPV6", key->fd);
                    ATTACHMENT(key)->origin_domain = AF_INET6;
                    d->request->dest_addr.ipv6.sin6_port = d->request->dest_port;
                    ATTACHMENT(key)->origin_addr_len = sizeof(d->request->dest_addr.ipv6);
                    memcpy(&ATTACHMENT(key)->origin_addr, &d->request->dest_addr,
                    sizeof(d->request->dest_addr.ipv6));

                    //ret = request_connect(key, d);
                    ret=REQUEST_CONNECTING;
                    break;
                }
                case socks_req_addrtype_domain: {
                    debug(etiqueta, 0, "FQDN", key->fd);
                    struct selector_key *k= malloc(sizeof (*key));
                    if(k==NULL){
                        debug(etiqueta, 0, "Malloc error", key->fd);
                        ret=REQUEST_WRITE;
                        *(d->status)= status_general_socks_server_failure;
                        selector_set_interest_key(key, OP_WRITE);
                    }else{
                        memcpy(k,key, sizeof(*k));
                        if(-1 == pthread_create(&tid, 0, request_resolv_blocking, k)){
                            debug(etiqueta, 0, "Error creating thread", key->fd);
                            ret=REQUEST_WRITE;
                            *(d->status)=status_general_socks_server_failure;
                            selector_set_interest_key(key, OP_WRITE);
                        }else{
                            debug(etiqueta, 0, "Entering request resolv", key->fd);
                            ret=REQUEST_RESOLV;
                            selector_set_interest_key(key, OP_NOOP);
                        }
                    }

                    break;
                }
                default: {
                    // TODO Check implementation
                    ret = REQUEST_WRITE;
                    *(d->status) = status_address_type_not_supported;
                    selector_set_interest_key(key, OP_WRITE);
                }
            }
            break;
        }

        case socks_req_cmd_bind:
        case socks_req_cmd_associate:
        default:
            *(d->status)= status_command_not_supported;
            ret = REQUEST_WRITE;
            selector_set_interest_key(key, OP_WRITE);
            break;
    }

    return ret;
}

void *
request_resolv_blocking(void *data){
    struct selector_key *key =(struct selector_key *)data;
    struct socks5 *s= ATTACHMENT(key);

    pthread_detach(pthread_self());
    s->origin_resolution=0;
    struct addrinfo hints = {
            .ai_family=AF_UNSPEC,
            .ai_socktype=SOCK_STREAM,
            .ai_flags= AI_PASSIVE,
            .ai_protocol=0,
            .ai_canonname=NULL,
            .ai_addr=NULL,
            .ai_next=NULL,
    };

    char buff[7];
    snprintf(buff,sizeof(buff), "%d", ntohs(s->client.request.request->dest_port));

    getaddrinfo(s->client.request.request->dest_addr.fqdn, buff, &hints,
                &s->origin_resolution);

    selector_notify_block(key->s, key->fd);

    free(data);

    return 0;
}

// procesa el resultado de la resolución de nombres
//TODO(facu)
unsigned request_resolv_done(struct selector_key *key)
{
    char * etiqueta = "REQUEST RESOLV DONE";
    debug(etiqueta, 0, "Stating stage", key->fd);
    /*struct request_st *d = &ATTACHMENT(key)->client.request;
    struct socks5 *s = ATTACHMENT(key);

    if (d->o.cant_addr == 0)
    {
        if(d->addr_resolv.ip_type + 1 < IP_CANT_TYPES) {
            d->addr_resolv.ip_type++;
            return request_process(key, d);
        }
        else if(d->addr_resolv.status != status_succeeded) {
            d->status = d->addr_resolv.status;
        }
        else if(d->status != status_ttl_expired) {
            d->status = status_general_socks_server_failure;
        }

        s->socks_info.status = d->status;
        goto fail;
    }
    else
    {
        struct sockaddr_storage addr_st = d->addr_resolv.origin_addr_res[d->addr_resolv.cant_addr - 1];
        if(addr_st.ss_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)&addr_st;
            s->origin_domain = addr->sin_family;
            addr->sin_port = d->request.dest_port;
        }
        else if(addr_st.ss_family == AF_INET6) {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)&addr_st;
            s->origin_domain = addr->sin6_family;
            addr->sin6_port = d->request.dest_port;
        }
        else {
            d->status = status_address_type_not_supported;
            goto fail;
        }

        s->origin_addr_len = sizeof(struct sockaddr_storage);
        memcpy(&s->origin_addr, &addr_st, s->origin_addr_len);
        d->addr_resolv.cant_addr--;
    }

    return request_connect(key, d);

    fail:
        if (-1 != request_marshal(s->client.request.wb, d->status, d->request.dest_addr_type, d->request.dest_addr, d->request.dest_port))
        {
            return REQUEST_WRITE;
        }
        else {
            abort();
        }*/
//    selector_set_interest_key(key, OP_WRITE);

    debug(etiqueta, 0, "Finishing stage", key->fd);
    return REQUEST_CONNECTING;
}
