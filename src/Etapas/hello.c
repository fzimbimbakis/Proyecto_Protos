#include "../../include/hello.h"

#define MSG_NOSIGNAL      0x2000  /* don't raise SIGPIPE */

void hello_parser_init(struct hello_parser *p)
{
    p->state = hello_version;
    p->remaining = 0;
}

enum hello_state hello_parser_feed(struct hello_parser *p, uint8_t b)
{
    switch (p->state)
    {
        case hello_version:
            if (b == 0x05)
            {
                p->state = hello_nmethods;
            }
            else
            {
                p->state = hello_error_unsupported_version;
            }
            break;
        case hello_nmethods:
            p->remaining = b;
            p->state = hello_methods;
            if (p->remaining <= 0)
            {
                p->state = hello_done;
            }
            break;
        case hello_methods:
            if (p->on_authentication_method != NULL)
            {
                p->on_authentication_method(p->data, b);
            }
            p->remaining--;
            if (p->remaining <= 0)
            {
                p->state = hello_done;
            }
            break;
        case hello_done:

            break;
        case hello_error_unsupported_version:

            break;
        default:
            abort();
            break;
    }
    return p->state;
}

bool hello_is_done(const enum hello_state state, bool *error)
{
    bool ret = false;
    switch (state)
    {
        case hello_error_unsupported_version:
            if (error != 0)
            {
                *error = true;
            }
            ret = true;
            break;
        case hello_done:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
    return ret;
}

enum hello_state hello_consume(buffer *b, struct hello_parser *p, bool *error)
{
    enum hello_state st = p->state;
    bool finished = false;
    while (buffer_can_read(b) && !finished)
    {
        uint8_t byte = buffer_read(b);
        st = hello_parser_feed(p, byte);
        if (hello_is_done(st, error))
        {
            finished = true;
        }
    }
    return st;
}

int hello_marshal(buffer *b, const uint8_t method)
{
    size_t n;
    uint8_t *buf = buffer_write_ptr(b, &n);
    if (n < 2)
    {
        return -1;
    }
    buf[0] = 0x05;
    buf[1] = method;
    buffer_write_adv(b, 2);
    return 2;
}

/** callback del parser utilizado en `read_hello' */
static void on_hello_method(struct hello_parser *p, const uint8_t method) {
    uint8_t *selected  = p->data;

    if(METHOD_NO_AUTHENTICATION_REQUIRED == method) {
        *selected = method;
    }
}

/** inicializa las variables de los estados HELLO_â€¦ */
static void hello_read_init(const unsigned state, struct selector_key *key) {
    char * etiqueta = "HELLO READ INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);
    d->parser.data                           = &d->method;
    d->parser.on_authentication_method = on_hello_method, hello_parser_init(
            &d->parser);
    debug(etiqueta, 0, "Finished stage", key->fd);
}

/** lee todos los bytes del mensaje de tipo 'hello' y inicia su proceso */
static unsigned hello_read(struct selector_key *key) {
    char * etiqueta = "HELLO READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    unsigned  ret      = HELLO_READ;
    bool  error    = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    debug(etiqueta, 0, "Reading from client", key->fd);
    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);
    if(n > 0) {
        buffer_write_adv(d->rb, n);
        debug(etiqueta, n, "Finished reading", key->fd);
        debug(etiqueta, n, "Starting hello consume", key->fd);
        const enum hello_state st = hello_consume(d->rb, &d->parser, &error);
        if(hello_is_done(st, 0)) {
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                ret = hello_process(d);
            } else {
                ret = ERROR;
            }
        }
    } else {
        debug(etiqueta, n, "Error, nothing to read", key->fd);
        ret = ERROR;
    }
    debug(etiqueta, error, "Finished stage", key->fd);
    return error ? ERROR : ret;
}

/** procesamiento del mensaje `hello' */
static unsigned hello_process(const struct hello_st* d) {
    unsigned ret = HELLO_WRITE;

    uint8_t m = d->method;
    const uint8_t r = (m == METHOD_NO_ACCEPTABLE_METHODS) ? 0xFF : 0x00;
    if (-1 == hello_marshal(d->wb, r)) {
        ret  = ERROR;
    }
    if (METHOD_NO_ACCEPTABLE_METHODS == m) {
        ret  = ERROR;
    }
    return ret;
}

void hello_read_close(const unsigned state, struct selector_key *key)
{
    char * etiqueta = "HELLO READ CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    hello_parser_close(&d->parser);
    debug(etiqueta, 0, "Finished stage", key->fd);
}

unsigned hello_write(struct selector_key *key)
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