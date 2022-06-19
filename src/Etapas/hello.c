#include "../../include/hello.h"

#ifndef MSG_NOSIGNAL
//// For mac compilation only
#define MSG_NOSIGNAL 0x2000  /* don't raise SIGPIPE */
#endif

void hello_parser_init(struct hello_parser *p)
{
    p->state = hello_version;
    p->remaining = 0;
}

enum hello_state hello_parser_feed(struct hello_parser *p, uint8_t b)
{
    static char * etiqueta = "HELLO PARSER FEED";
    switch (p->state)
    {
        case hello_version:
            if (b == 0x05)
            {
                debug(etiqueta, b, "Hello version supported", 0);
                p->state = hello_nmethods;
            }
            else
            {
                debug(etiqueta, b, "Hello version not supported", 0);
                p->state = hello_error_unsupported_version;
            }
            break;
        case hello_nmethods:
            p->remaining = b;
            p->state = hello_methods;
            debug(etiqueta, b, "Number of methods received", 0);
            if (p->remaining <= 0)
            {
                debug(etiqueta, b, "Number of methods received is 0", 0);
                p->state = hello_done;
            }
            break;
        case hello_methods:
            debug(etiqueta, b, "Analyzing method", p->remaining);
            if (p->on_authentication_method != NULL)
            {
                p->on_authentication_method(p->data, b);
            }
            p->remaining--;
            if (p->remaining <= 0)
            {
                debug(etiqueta, b, "No more methods remaining", p->remaining);
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
// TODO Acá se setea el método que quiero?
static void on_hello_method(void  *p, const uint8_t method) {
    char * etiqueta = "ON HELLO METHOD";
    uint8_t *selected  = p;
    debug(etiqueta, method, "Posible method from client list of methods", 0);
//    if(METHOD_NO_AUTHENTICATION_REQUIRED == method) {
//        debug(etiqueta, method, "New method selected, METHOD_NO_AUTHENTICATION_REQUIRED", 0);
//        *selected = method;
//    }
    if(0x02 == method) {
        debug(etiqueta, method, "New method selected, USERPASS", 0);
        *selected = method;
    }
}

/** inicializa las variables de los estados HELLO_st */
void hello_read_init(const unsigned state, struct selector_key *key) {
    char * etiqueta = "HELLO READ INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);
    d->method = 0xFF;
    d->parser = malloc(sizeof(*(d->parser)));
    d->parser->data = &(d->method);
    d->parser->on_authentication_method = on_hello_method;
    hello_parser_init(d->parser);
    debug(etiqueta, 0, "Finished stage", key->fd);
}

/** inicializa las variables de los estados HELLO_st */
void hello_write_init(const unsigned state, struct selector_key *key) {
    char * etiqueta = "HELLO WRITE INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);
    debug(etiqueta, 0, "Finished stage", key->fd);
}


/** lee todos los bytes del mensaje de tipo 'hello' y inicia su proceso */
unsigned hello_read(struct selector_key *key) {
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
        const enum hello_state st = hello_consume(d->rb, d->parser, &error);
        if(hello_is_done(st, 0)) {
            debug(etiqueta, error, "Finished hello consume", key->fd);
            debug(etiqueta, 0, "Setting selector interest to write", key->fd);
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
    char * etiqueta = "HELLO PROCESS";
    debug(etiqueta, 0, "Starting input from client processing", 0);
    unsigned ret = HELLO_WRITE;

    uint8_t m = d->method;
//    const uint8_t r = (m == METHOD_NO_ACCEPTABLE_METHODS) ? 0xFF : 0x00;
    debug(etiqueta, m, "Method selected", 0);
    if (-1 == hello_marshal(d->wb, m)) {
        ret  = ERROR;
    }
    debug(etiqueta, ret, "Finished input from client processing", 0);
    return ret;
}

void hello_read_close(const unsigned state, struct selector_key *key)
{
    char * etiqueta = "HELLO READ CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    hello_parser_close(d->parser);
    free(d->parser);
    debug(etiqueta, 0, "Finished stage", key->fd);
}

void hello_write_close(const unsigned state, struct selector_key *key)
{
    char * etiqueta = "HELLO WRITE CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    //// Nothing to close or free
    debug(etiqueta, 0, "Finished stage", key->fd);
}

unsigned hello_write(struct selector_key *key)
{
    char * etiqueta = "HELLO WRITE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    unsigned ret = HELLO_WRITE;
    uint8_t *ptr;
    size_t count;
    ssize_t n;


    debug(etiqueta, 0, "Writing to client", key->fd);
    ptr = buffer_read_ptr(d->wb, &count);
    n= send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n==-1){
        debug(etiqueta, 0, "Error on send", key->fd);
        debug(etiqueta, 0, "Finished stage", key->fd);
        return ERROR;
    }

    buffer_read_adv(d->wb,n);
    if(!buffer_can_read(d->wb)){
        if(d->method == METHOD_NO_ACCEPTABLE_METHODS){
            debug(etiqueta, 0, "No acceptable methods -> CLOSING CONNECTION", key->fd);
            debug(etiqueta, 0, "Finished stage", key->fd);
            return DONE;
        }
        if(SELECTOR_SUCCESS== selector_set_interest_key(key, OP_READ)){
            debug(etiqueta, 0, "Succeed, setting interest to read", key->fd);
            ret= USERPASS_READ;
        }else{
            debug(etiqueta, 0, "Error on selector set interest", key->fd);
            ret=ERROR;
        }
    }

    debug(etiqueta, 0, "Finished stage", key->fd);
    return ret;
}

void hello_parser_close(struct hello_parser *p){
    /** empty **/
}
