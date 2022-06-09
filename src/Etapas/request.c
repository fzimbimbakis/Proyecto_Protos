#include <sys/errno.h>
#include "../../include/request.h"


void request_parser_init(struct request_parser *p)
{
    p->state = request_version;
    p->remaining = 0;
    p->read = 0;
}

void request_parser_close(struct request_parser* parser){
    //TODO: chequear que se libere bien todo
    free(parser->request);
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
    struct request_st *d = &ATTACHMENT(key)->client.request;
//    // Adding the read buffer
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb= &(ATTACHMENT(key)->write_buffer);
    d->parser = malloc(sizeof(*(d->parser)));
    d->parser->request =&d->request;
    request_parser_init(d->parser);
    d->status=status_general_socks_server_failure;
    d->client_fd= &ATTACHMENT(key)->client_fd;
    d->origin_fd=&ATTACHMENT(key)->origin_fd;

    d->origin_addr=&ATTACHMENT(key)->origin_addr;
    d->origin_addr_len= &ATTACHMENT(key)->origin_addr_len;
    d->origin_domain=&ATTACHMENT(key)->origin_domain;
}


// lee todos los bytes del mensaje de tipo 'request' e inicia su proceso
unsigned request_read(struct selector_key *key)
{
    struct request_st *d = &ATTACHMENT(key)->client.request;

    buffer *b = d->rb;
    unsigned ret = REQUEST_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(b, &count);
    n = recv(key->fd, ptr, count, 0);
    if (n > 0)
    {
        buffer_write_adv(b, n);
        int st = request_consume(b, d->parser, &error);
        if (request_is_done(st, 0))
        {
            ret = request_process(key, d);
        }
    }
    else
    {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}


enum request_state request_consume(buffer *b, struct request_parser *p, bool *error)
{
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

//TODO: no se llega a ver completo en la clase
unsigned request_write(struct selector_key *key)
{
    struct request_st *d = &ATTACHMENT(key)->client.request;

    buffer *b = d->wb;
    unsigned ret = ERROR;
    uint8_t *ptr;
    size_t count;
    ssize_t n;
    ptr = buffer_read_ptr(b, &count);
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
    return ret;
}


unsigned request_process(struct selector_key *key, struct request_st *d)
{
    unsigned ret;
    pthread_t tid;

    switch (d->request.cmd)
    {
        case socks_req_cmd_connect: {
            // esto mejoraría enormemente de haber usado sockaddr_storage en el request
            switch (d->request.dest_addr_type) {
                case socks_req_addrtype_ipv4: {
                    ATTACHMENT(key)->origin_domain = AF_INET;
                    d->request.dest_addr.ipv4.sin_port = d->request.dest_port;
                    ATTACHMENT(key)->origin_addr_len = sizeof(d->request.dest_addr.ipv4);
                    memcpy(&ATTACHMENT(key)->origin_addr, &d->request.dest_addr,
                           sizeof(d->request.dest_addr.ipv4));

                    ret = request_connect(key, d);
                    break;
                }
                case socks_req_addrtype_ipv6: {
                    ATTACHMENT(key)->origin_domain = AF_INET6;
                    d->request.dest_addr.ipv6.sin6_port = d->request.dest_port;
                    ATTACHMENT(key)->origin_addr_len = sizeof(d->request.dest_addr.ipv6);
                    memcpy(&ATTACHMENT(key)->origin_addr, &d->request.dest_addr,
                    sizeof(d->request.dest_addr.ipv6));

                    ret = request_connect(key, d);
                    break;
                }
                case socks_req_addrtype_domain: {
                    struct selector_key *k= malloc(sizeof (*key));
                    if(k==NULL){
                        ret=REQUEST_WRITE;
                       d->status= status_general_socks_server_failure;
                        selector_set_interest_key(key, OP_WRITE);
                    }else{
                        memcpy(k,key, sizeof(*k));
                        if(-1 == pthread_create(&tid, 0, request_resolv_blocking, k)){
                            ret=REQUEST_WRITE;
                            d->status=status_general_socks_server_failure;
                            selector_set_interest_key(key, OP_WRITE);
                        }else{
                            ret=REQUEST_RESOLV;
                            selector_set_interest_key(key, OP_NOOP);
                        }
                    }

                    break;
                }
                default: {
                    ret = REQUEST_WRITE;
                    d->status = status_address_type_not_supported;
                    selector_set_interest_key(key, OP_WRITE);
                }
            }
            break;
        }

        case socks_req_cmd_bind:
        case socks_req_cmd_associate:
        default:
            d->status = status_command_not_supported;
            ret = REQUEST_WRITE;
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
    snprintf(buff,sizeof(buff), "%d", ntohs(s->client.request.request.dest_port));

    getaddrinfo(s->client.request.request.dest_addr.fqdn, buff, &hints,
                &s->origin_resolution);

    selector_notify_block(key->s, key->fd);

    free(data);

    return 0;
}

enum socks_reply_status errno_to_socks(int e)
{
    enum socks_reply_status ret = status_general_socks_server_failure;

    switch (e)
    {
        case 0:
            ret = status_succeeded;
            break;
        case ECONNREFUSED:
            ret = status_connection_refused;
            break;
        case EHOSTUNREACH:
            ret = status_host_unreachable;
            break;
        case ENETUNREACH:
            ret = status_network_unreachable;
            break;
        case ETIMEDOUT:
            ret = status_ttl_expired;
            break;
    }

    return ret;
}

// procesa el resultado de la resolución de nombres
/*static unsigned request_resolv_done(struct selector_key *key)
{
    struct request_st *d = &ATTACHMENT(key)->client.request;
    struct socks5 *s = ATTACHMENT(key);

    if (d->addr_resolv.cant_addr == 0)
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
        }
    return 0;
}*/


unsigned
request_connect(struct selector_key *key, struct request_st *d){
    bool error= false;

    enum socks_reply_status status= d->status;
    int *fd=d->origin_fd;

    *fd= socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);

    if(*fd==-1){
        error=true;
        goto finally;
    }

    if(selector_fd_set_nio(*fd) == -1){
        goto finally;
    }

    if(-1 == connect(*fd, (const struct sockaddr*)&ATTACHMENT(key)->origin_addr,
                     ATTACHMENT(key)->origin_addr_len)){
        if(errno == EINPROGRESS){
            selector_status st= selector_set_interest_key(key, OP_NOOP);
            if(SELECTOR_SUCCESS != st){
                error=true;
                goto finally;
            }

            const struct fd_handler socksv5 = {
                    .handle_read       = socksv5_passive_accept,
                    .handle_write      = NULL,
                    .handle_close      = NULL, // nada que liberar
            };

            st= selector_register(key->s, *fd, &socksv5,
                                  OP_WRITE, key->data);
            if(SELECTOR_SUCCESS != st){
                error=true;
                goto finally;
            }
            ATTACHMENT(key)->references += 1;
        }else{
            status = errno_to_socks(errno);
            error=true;
            goto finally;

            //TODO: seguir porque no se ve como sigue en la clase
        }
    }

    finally:
    return error ? ERROR : 0;
    //return 0;
}

int
request_marshall(buffer* b,
                 const enum socks_reply_status status){
    size_t count;
    uint8_t *buff= buffer_write_ptr(b, &count);

    if(count < 10)
        return -1;

    buff[0]=0x05;
    buff[1]=status;
    buff[2]=0x00;
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

unsigned request_connecting(struct selector_key *key){
    int error;
    socklen_t len= sizeof(error);
    struct connecting *d= &ATTACHMENT(key)->orig.conn;

    if(getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0){
        *d->status= status_general_socks_server_failure;
    }else{
        if(error== 0){
            *d->status=status_succeeded;
            *d->origin_fd=key->fd;
        }else{
            *d->status=errno_to_socks(error);
        }
    }

    if(-1 == request_marshall (d->wb, *d->status)){
        *d->status=status_general_socks_server_failure;
        abort();
    }

    selector_status s=0;
    s|= selector_set_interest(key->s, *d->client_fd, OP_WRITE);
    s|= selector_set_interest_key(key, OP_NOOP);

    return SELECTOR_SUCCESS== s ? REQUEST_WRITE:ERROR;
    //return ERROR;
}
