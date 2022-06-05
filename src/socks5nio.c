/**
 * socks5nio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes)
 */
#include<stdio.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <assert.h>  // assert
#include <errno.h>
#include <time.h>
#include <unistd.h>  // close
#include <pthread.h>
#include <arpa/inet.h>

#include "hello.h"

#include "../include/request.h"
#include "../include/buffer.h"
#include "../include/socks5nio.h"
#include "../include/stm.h"
#include"netutils.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))


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


    REQUEST_READ,
    REQUEST_RESOLV,
    REQUEST_CONNECTING,
    REQUEST_WRITE,
    COPY,



    // estados terminales
    DONE,
    ERROR,
};

////////////////////////////////////////////////////////////////////
// DefiniciÃ³n de variables para cada estado

/** Used by the HELLO_READ and HELLO_WRITE states */
typedef struct hello_st
{
    /** Buffers used for IO */
    buffer *rb, *wb;
    /** Pointer to hello parser */
    struct hello_parser parser;
    /** Selected auth method */
    uint8_t method;
} hello_st;

/** Used by the USERPASS_READ and USERPASS_WRITE states */
//typedef struct userpass_st
//{
    /** Buffers used for IO */
    //buffer *rb, *wb;
    /** Pointer to hello parser */
    //struct up_req_parser parser;
    /** Selected user */
    //uint8_t * user;
    /** Selected password */
    //uint8_t * password;
//} userpass_st;

/** Used by the REQUEST_READ, REQUEST_WRITE and REQUEST_RESOLV state */
typedef struct request_st
{
    buffer *rb, *wb;


    struct request request;
    struct request_parser parser;


    enum socks5_response_status status;

    struct sockaddr_storage *origin_addr;
    socklen_t *origin_addr_len;
    int *origin_domain;

    const int *client_fd;
    int *origin_fd;
} request_st;

/** Used by REQUEST_CONNECTING */
typedef struct connecting{
    buffer *wb
    const int *client_fd;
    int *origin_fd;
    enum socks_reply_status *status;
};

/** Used by the COPY state */
typedef struct copy
{
    /** File descriptor */
    int fd;
    /** Reading buffer */
    buffer * rb;
    /** Writing buffer */
    buffer * wb;
    /** Interest of the copy */
    fd_interest interest;
    /** Pointer to the structure of the opposing copy state*/
    struct copy_st * other_copy;

} copy_st;



/*
 * Si bien cada estado tiene su propio struct que le da un alcance
 * acotado, disponemos de la siguiente estructura para hacer una Ãºnica
 * alocaciÃ³n cuando recibimos la conexiÃ³n.
 *
 * Se utiliza un contador de referencias (references) para saber cuando debemos
 * liberarlo finalmente, y un pool para reusar alocaciones previas.
 */
struct socks5 {
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
        struct request_st         request;
        struct copy               copy;
    } client;
    /** estados para el origin_fd */
    union {
        struct connecting         conn;
        struct copy               copy;
    } orig;

    //TODO(facu): ver tamaño de buffer mas conveniente
    uint8_t raw_buff_a[2048], raw_buff_b[2048];
    buffer read_buffer, write_buffer;

    /** cantidad de referencias a este objeto, si es una se debe destruir */
    unsigned references;+

    /** siguiente en el pool */
    struct struct socks5 *next;

};

/**
 * Pool de struct socks5, para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */

static const unsigned max_pool=50;
static unsigned  pool_size=0;
static struct socks5 *pool=0;

static const struct  state_definition* socks5_describe_states();

/** crea un nuevo struct socks */
static struct socks5* socks5_new(int client_fd){
    struct socks5 *ret;

    if(pool == NULL){
        ret = malloc(sizeof(*ret));
    }else{
        ret=pool;
        pool=pool->next;
        ret->next=0;
    }

    if(ret == NULL){
        goto finally;
    }

    memset(ret, 0x00, sizeof(*ret));

    ret->origin_fd =-1;
    ret->client_fd= client_fd;
    ret->client_addr_len= sizeof(ret->client_addr);

    ret->stm.initial =HELLO_READ;
    ret->stm.max_state= ERROR;
    ret->stm.states= socks5_describe_states();
    stm_init(&ret->stm);

    buffer_init(&ret->read_buffer, N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    ret->references= 1;

    finally:
        return ret;
}



/** realmente destruye */
static void
socks5_destroy_(struct socks5* s) {
    if(s->origin_resolution != NULL) {
        freeaddrinfo(s->origin_resolution);
        s->origin_resolution = 0;
    }
    free(s);
}

/**
 * destruye un  `struct socks5', tiene en cuenta las referencias
 * y el pool de objetos.
 */
static void
socks5_destroy(struct socks5 *s) {
    if(s == NULL) {
        // nada para hacer
    } else if(s->references == 1) {
        if(s != NULL) {
            if(pool_size < max_pool) {
                s->next = pool;
                pool    = s;
                pool_size++;
            } else {
                socks5_destroy_(s);
            }
        }
    } else {
        s->references -= 1;
    }
}

void
socksv5_pool_destroy(void) {
    struct socks5 *next, *s;
    for(s = pool; s != NULL ; s = next) {
        next = s->next;
        free(s);
    }
}

/** obtiene el struct (socks5 *) desde la llave de selecciÃ³n  */
#define ATTACHMENT(key) ( (struct socks5 *)(key)->data)

/* declaraciÃ³n forward de los handlers de selecciÃ³n de una conexiÃ³n
 * establecida entre un cliente y el proxy.
 */
static void socksv5_read   (struct selector_key *key);
static void socksv5_write  (struct selector_key *key);
static void socksv5_block  (struct selector_key *key);
static void socksv5_close  (struct selector_key *key);
static const struct fd_handler socks5_handler = {
        .handle_read   = socksv5_read,
        .handle_write  = socksv5_write,
        .handle_close  = socksv5_close,
        .handle_block  = socksv5_block,
};

/** Intenta aceptar la nueva conexiÃ³n entrante*/
void
socksv5_passive_accept(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    struct socks5                *state           = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr,
                              &client_addr_len);
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    state = socks5_new(client);
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberÃ³ alguna conexiÃ³n.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;

    if(SELECTOR_SUCCESS != selector_register(key->s, client, &socks5_handler,
                                             OP_READ, state)) {
        goto fail;
    }
    return ;
    fail:
    if(client != -1) {
        close(client);
    }
    socks5_destroy(state);
}

////////////////////////////////////////////////////////////////////////////////
// HELLO
////////////////////////////////////////////////////////////////////////////////

/** callback del parser utilizado en `read_hello' */
static void
on_hello_method(struct hello_parser *p, const uint8_t method) {
    uint8_t *selected  = p->data;

    if(SOCKS_HELLO_NOAUTHENTICATION_REQUIRED == method) {
        *selected = method;
    }
}

/** inicializa las variables de los estados HELLO_â€¦ */
static void
hello_read_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);
    d->parser.data                     = &d->method;
    d->parser.on_authentication_method = on_hello_method, hello_parser_init(
            &d->parser);
}

static unsigned
hello_process(const struct hello_st* d);

/** lee todos los bytes del mensaje de tipo `hello' y inicia su proceso */
static unsigned
hello_read(struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    unsigned  ret      = HELLO_READ;
    bool  error    = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);
    if(n > 0) {
        buffer_write_adv(d->rb, n);
        const enum hello_state st = hello_consume(d->rb, &d->parser, &error);
        if(hello_is_done(st, 0)) {
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                ret = hello_process(d);
            } else {
                ret = ERROR;
            }
        }
    } else {
        ret = ERROR;
    }

    return error ? ERROR : ret;
}

/** procesamiento del mensaje `hello' */
static unsigned
hello_process(const struct hello_st* d) {
    unsigned ret = HELLO_WRITE;

    uint8_t m = d->method;
    const uint8_t r = (m == SOCKS_HELLO_NO_ACCEPTABLE_METHODS) ? 0xFF : 0x00;
    if (-1 == hello_marshall(d->wb, r)) {
        ret  = ERROR;
    }
    if (SOCKS_HELLO_NO_ACCEPTABLE_METHODS == m) {
        ret  = ERROR;
    }
    return ret;
}

void
hello_read_close(const unsigned state, struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    hello_parser_close(&d->parser);
}

unsigned
hello_write(struct selector_key *key)
{

    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    unsigned ret = HELLO_WRITE;
    uint8_t *ptr;
    size_t count;
    ssize_t n;


    ptr = buffer_read_ptr(d->wb, &count);
    n= send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n==-1){
        ret=ERROR;
    }else{
        buffer_read_adv(d->wb,n);
        if(!buffer_can_read(d->wb)){
            if(SELECTOR_SUCCESS== selector_set_interest_key(key, OP_READ)){
                ret= REQUEST_READ;
            }else{
                ret=ERROR;
            }
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////
// REQUEST
////////////////////////////////////////////////////////////////////////////

static void
request_init(const unsigned state, struct selector_key *key)
{
    struct request_st *d = &ATTACHMENT(key)->client.request;
    // Adding the read buffer
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb= &(ATTACHMENT(key)->write_buffer);
    d->parser.request =&d->request;
    d->status=status_general_SOCKS_server_failure;
    request_parser_init(&d->parser);
    d->client_fd= &ATTACHMENT(key)->client_fd;
    d->origin_fd=&ATTACHMENT(key)->origin_fd;

    d->origin_addr=&ATTACHMENT(key)->origin_addr;
    d->origin_addr_len= &ATTACHMENT(key)->origin_addr_len;
    d->origin_domain=&ATTACHMENT(key)->origin_domain;
}

static unsigned
request_process(struct selector_key *key, struct request_st *d);

// lee todos los bytes del mensaje de tipo 'request' e inicia su proceso
static unsigned request_read(struct selector_key *key)
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
        int st = request_consume(b, &d->parser, &error);
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

//TODO: no se llega a ver completo en la clase
static unsigned request_write(struct selector_key *key)
{
    struct request_st *d = &ATTACHMENT(key)->client.request;

    buffer *b = d->wb;
    unsigned ret = REQUEST_WRITE;
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


static unsigned request_connect(struct selector_key *key, struct request_st *d);

//TODO: no se llega a ver completo en la clase
static unsigned request_process(struct selector_key *key, struct request_st *d)
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
                    memcpy(&ATTACHMENT(key)->socks_info.dest_addr, &d->request.dest_addr,
                           sizeof(d->request.dest_addr.ipv4));
                    ATTACHMENT(key)->socks_info.dest_port = d->request.dest_port;

                    ret = request_connect(key, d);
                    break;
                }
                case socks_req_addrtype_ipv6: {
                    ATTACHMENT(key)->origin_domain = AF_INET6;
                    d->request.dest_addr.ipv6.sin6_port = d->request.dest_port;
                    ATTACHMENT(key)->origin_addr_len = sizeof(d->request.dest_addr.ipv6);
                    memcpy(&ATTACHMENT(key)->origin_addr, &d->request.dest_addr,
                           sizeof(d->request.dest_addr.ipv6));

                    memcpy(&ATTACHMENT(key)->socks_info.dest_addr, &d->request.dest_addr,
                           sizeof(d->request.dest_addr.ipv6));
                    ATTACHMENT(key)->socks_info.dest_port = d->request.dest_port;

                    ret = request_connect(key, d);
                    break;
                }
                case socks_req_addrtype_domain: {
                    struct selector_key *k= malloc(sizeof (*key));
                    if(k==NULL){
                        ret=REQUEST_WRITE;
                        d->status= status_general_SOCKS_server_failure;
                        selector_set_interest_key(key, OP_WRITE);
                    }else{
                        memcpy(k,key, sizeof(*k));
                        if(-1 == pthread_create(&tid, 0, request_resolv_blocking, k)){
                            ret=REQUEST_WRITE;
                            d->status=status_general_SOCKS_server_failure;
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
            // Unsupported

        default:
            d->status = status_command_not_supported;
            ret = REQUEST_WRITE;
            break;
    }

    return ret;
}

static void *
request_resolv_blocking(void *data){
    struct selector_key *key =(struct selector_key *)data;
    struct socks5 *s= ATTACHMENT(key);

    pthread_detach(pthread_self());
    s->origin_resolution=0;
    struct addrinfo hints = {
            .ai_family=AF_UNSPEC,
            .ai_socktype=SOCK_STREAM,
            .ai_flags= AI_PASSIVE,
            .ai_protocol=0;
    };
    
    //TODO: no se llega a ver todo en la clase
}

// procesa el resultado de la resolución de nombres
static unsigned request_resolv_done(struct selector_key *key)
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
}


static unsigned 
request_connect(struct selector_key *key, struct request_st *d){
    bool error= false;
    
    enum socks5_response_status=d->status;
    int *fd=d->origin_fd;
    
    *fd= socket(ATTACHMENT(key)->origin_domain.SOCK_STREAM, 0);
    
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
            
            st= selector_register(key->s, *fd, &socks5_handler,
                                  OP_WRITE, key->data);
            if(SELECTOR_SUCCESS != st){
                error=true;
                goto finally;
            }
            ATTACHMENT(key)->references += 1;
        }else{
            status =errno_to_sock(errno);
            error=true;
            goto finally;

            //TODO: seguir porque no se ve como sigue en la clase
        }
    }

    finally:
    return error ? ERROR : ret;
}

static unsigned
request_connecting(struct selector_key *key){
    int error;
    socklen_t len= sizeof(error);
    struct connecting *d= &ATTACHMENT(key)->orig.conn;

    if(getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0){
        *d->status= status_general_SOCKS_server_failure;
    }else{
        if(error== 0){
            *d->status=status_succeeded;
            *d->origin_fd=key->fd;
        }else{
            *d->status=errno_to_socks(error);
        }
    }

    if(-1 == request_marshall (d->wb, *d->status)){
        *d->status=status_general_SOCKS_server_failure;
        abort();
    }

    selector_status s=0;
    s|= selector_set_interest(key->s, *d->client_fd, OP_WRITE);
    s|= selector_set_interest_key(key, OP_NOOP);

    return SELECTOR_SUCCESS== s ? REQUEST_WRITE:ERROR;

}

//////////////////////////////////////////////////////////////////////////////
// COPY
/////////////////////////////////////////////////////////////////////////////

static void copy_init(struct selector_key *key)
{
    struct copy *d = &ATTACHMENT(key)->client.copy;

    d->fd = &ATTACHMENT(key)->client_fd;
    d->rb = &ATTACHMENT(key)->read_buffer;
    d->wb = &ATTACHMENT(key)->write_buffer;
    d->duplex = OP_READ | OP_WRITE;
    d->other = &ATTACHMENT(key)->orig.copy;

    d = &ATTACHMENT(key)->orig.copy;
    d->fd = &ATTACHMENT(key)->origin_fd;
    d->rb = &ATTACHMENT(key)->write_buffer;
    d->wb = &ATTACHMENT(key)->read_buffer;
    d->duplex = OP_READ | OP_WRITE;
    d->other = &ATTACHMENT(key)->client.copy;
}

static fd_interest copy_compute_interests(fd_selector s, struct copy *d)
{
    fd_interest ret = OP_NOOP;

    if(*d->fd != -1)
    {
        if (((d->duplex & OP_READ) && buffer_can_write(d->rb)) )
        {
            ret |= OP_READ;
        }
        if ((d->duplex & OP_WRITE) && buffer_can_read(d->wb) )
        {
            ret |= OP_WRITE;
        }
        if (SELECTOR_SUCCESS != selector_set_interest(s, *d->fd, ret))
        {
            abort();
        }
    }

    return ret;
}
// lee bytes de un socket y los encola para ser escritos en otro socket
static unsigned copy_r(struct selector_key *key)
{
    struct copy *d = copy_ptr(key);

    assert(*d->fd == key->fd);
    size_t size;
    ssize_t n;
    buffer *b = d->rb;
    unsigned ret = COPY;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);
    if (n <= 0)
    {
        // Si error o EOF cierro el canal de lectura y el canal de escritura del origin
        shutdown(*d->fd, SHUT_RD);
        d->duplex &= ~OP_READ;
        if (*d->other->fd != -1)
        {
            shutdown(*d->other->fd, SHUT_WR);
            d->other->duplex &= ~OP_WRITE;
        }
    }
    else
    {
        buffer_write_adv(b, n);

    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other);
    if (d->duplex == OP_NOOP)
    {
        ret = DONE;
    }

    return ret;
}

// escribe bytes encolados
static unsigned copy_w(struct selector_key *key)
{
    struct copy *d = copy_ptr(key);

    assert(*d->fd == key->fd);
    size_t size;
    ssize_t n;
    buffer *b = d->wb;
    unsigned ret = COPY;

    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);
    if (n == -1)
    {
        shutdown(*d->fd, SHUT_WR);
        d->duplex &= ~OP_WRITE;
        if (*d->other->fd != -1)
        {
            shutdown(*d->other->fd, SHUT_RD);
            d->other->duplex &= ~OP_READ;
        }
    }
    else
    {
        buffer_read_adv(b, n);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other);
    if (d->duplex == OP_NOOP)
    {
        ret = DONE;
    }

    return ret;
}

/** definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
        {
                .state = HELLO_READ,
                .on_arrival = hello_read_init,
                .on_departure = hello_read_close,
                .on_read_ready = hello_read,
        },
        {       .state = HELLO_WRITE,
                .on_arrival = hello_write_init,
                .on_departure = hello_write_close,
                .on_write_ready = hello_write

        },
        /**{
                .state = USERPASS_READ,
                .on_arrival = userpass_read_init,
                .on_departure = userpass_read_close,
                .on_read_ready = userpass_read,
        },
        {
                .state = USERPASS_WRITE,
                .on_arrival = userpass_write_init,
                .on_departure = userpass_write_close,
                .on_write_ready = userpass_write,
        },*/
        {
                .state = REQUEST_READ,
                .on_arrival = request_init,
                .on_departure = request_close,
                .on_read_ready = request_read,
        },
        {
                .state = REQUEST_RESOLV,
                .on_arrival = resolve_init,
                .on_departure = resolve_close,
                .on_read_ready = resolve_read,
                .on_write_ready = resolve_write,
        },
        {
                .state = REQUEST_CONNECTING,
                .on_arrival = connecting_init,
                .on_read_ready = connecting_read,
                .on_write_ready = connecting_write,
                .on_departure = connecting_close,
        },
        {   .state = COPY,
                .on_arrival = copy_init,
                .on_read_ready = copy_read,
                .on_write_ready = copy_write,
                .on_departure = copy_close
        },
        {
                .state = DONE,
                // For now, no need to define any handlers, all in sockv5_done
        },
        {
                .state = ERROR,
                // No now, no need to define any handlers, all in sockv5_done
        }};







///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexiÃ³n pasiva.
// son los que emiten los eventos a la maquina de estados.
        static void
        socksv5_done(struct selector_key* key);

        static void
        socksv5_read(struct selector_key *key) {
            struct state_machine *stm   = &ATTACHMENT(key)->stm;
            const enum socks_v5state st = stm_handler_read(stm, key);

            if(ERROR == st || DONE == st) {
                socksv5_done(key);
            }
        }

        static void
        socksv5_write(struct selector_key *key) {
            struct state_machine *stm   = &ATTACHMENT(key)->stm;
            const enum socks_v5state st = stm_handler_write(stm, key);

            if(ERROR == st || DONE == st) {
                socksv5_done(key);
            }
        }

        static void
        socksv5_block(struct selector_key *key) {
            struct state_machine *stm   = &ATTACHMENT(key)->stm;
            const enum socks_v5state st = stm_handler_block(stm, key);

            if(ERROR == st || DONE == st) {
                socksv5_done(key);
            }
        }

        static void
        socksv5_close(struct selector_key *key) {
            socks5_destroy(ATTACHMENT(key));
        }

        static void
        socksv5_done(struct selector_key* key) {
            const int fds[] = {
                    ATTACHMENT(key)->client_fd,
                    ATTACHMENT(key)->origin_fd,
            };
            for(unsigned i = 0; i < N(fds); i++) {
                if(fds[i] != -1) {
                    if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                        abort();
                    }
                    close(fds[i]);
                }
            }
        }