#include <stdlib.h>
#include "../../include/copy.h"
#ifndef MSG_NOSIGNAL
//// For mac compilation only
#define MSG_NOSIGNAL 0x2000  /* don't raise SIGPIPE */
#endif
fd_interest copy_compute_interests(fd_selector s, struct copy_st *d)
{
    char * etiqueta = "COPY COMPUTE INTERESTS";
    debug(etiqueta, 0, "Setting interest", d->fd);
    fd_interest ret = OP_NOOP;

    if(d->fd != -1)
    {
        if (((d->interest & OP_READ) && buffer_can_write(d->rb)) )
        {
            debug(etiqueta, 0, "Add read interest", d->fd);
            ret |= OP_READ;
        }
        if ((d->interest & OP_WRITE) && buffer_can_read(d->wb) )
        {
            debug(etiqueta, 0, "Add write interest", d->fd);
            ret |= OP_WRITE;
        }
        if (SELECTOR_SUCCESS != selector_set_interest(s, d->fd, ret))
        {
            debug(etiqueta, 0, "Error setting interests", d->fd);
            // TODO(bruno) Manage abort?
            abort();
        }
    }

    return ret;
}
void copy_init(const unsigned int state, struct selector_key *key)
{
    char * etiqueta = "COPY INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);

    struct copy_st *d = &ATTACHMENT(key)->client.copy;
    d->fd = ATTACHMENT(key)->client_fd;
    d->rb = &ATTACHMENT(key)->read_buffer;
    d->wb = &ATTACHMENT(key)->write_buffer;
    d->interest = OP_READ | OP_WRITE;
    d->other_copy = &ATTACHMENT(key)->orig.copy;

    d = &ATTACHMENT(key)->orig.copy;
    d->fd = ATTACHMENT(key)->origin_fd;
    d->rb = &ATTACHMENT(key)->write_buffer;
    d->wb = &ATTACHMENT(key)->read_buffer;
    d->interest = OP_READ | OP_WRITE;
    d->other_copy = &ATTACHMENT(key)->client.copy;

    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other_copy);
}


void * copy_ptr(struct selector_key *key){
    char * etiqueta = "COPY PTR";
    int current_fd = key->fd;
    struct socks5 * data = key->data;
    if(current_fd ==  data->origin_fd){
        debug(etiqueta, 0, "Working on origin", key->fd);
        return &data->orig.copy;
    }
    if(current_fd ==  data->client_fd){
        debug(etiqueta, 0, "Working on client", key->fd);
        return &data->client.copy;
    }
    return NULL;
}
// lee bytes de un socket y los encola para ser escritos en otro socket
extern size_t metrics_historic_byte_transfer;
extern size_t metrics_average_bytes_per_read;
extern size_t total_reads;
extern size_t metrics_concurrent_connections;
unsigned copy_read(struct selector_key *key)
{
    char * etiqueta = "COPY READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct copy_st *d = copy_ptr(key);
    if(d == NULL){
        debug(etiqueta, 0, "Failed getting copy ptr", key->fd);
        exit(EXIT_FAILURE);
    }
    assert(d->fd == key->fd);
    size_t size;
    ssize_t n;
    buffer *b = d->rb;
    unsigned ret = COPY;

    debug(etiqueta, 0, "Starting read", key->fd);
    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);

    debug(etiqueta, n, "Finished recv", key->fd);
    if (n <= 0)
    {
        // Si error o EOF cierro el canal de lectura y el canal de escritura del origin
        debug(etiqueta, n, "Error or EOF: Shutdown read channel and oposite write channel", key->fd);
        shutdown(d->fd, SHUT_RD);
        d->interest &= ~OP_READ;
        if (d->other_copy->fd != -1)
        {
            shutdown(d->other_copy->fd, SHUT_WR);
            d->other_copy->interest &= ~OP_WRITE;
        }
    }
    else
    {
        //// Add bytes read
        if (total_reads == 0) {
            metrics_average_bytes_per_read = n;
            total_reads++;
        } else {
            metrics_average_bytes_per_read = ((metrics_average_bytes_per_read * total_reads) + n) / (total_reads + 1);
            total_reads++;
        }
        metrics_historic_byte_transfer += n;

        buffer_write_adv(b, n);

        debug(etiqueta, n, "Buffer write adv", key->fd);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other_copy);
    if (d->interest == OP_NOOP)
    {
        debug(etiqueta, n, "Socket has no more interests -> DONE state", key->fd);
        metrics_concurrent_connections -= 1;
        ret = DONE;
    }

    return ret;
}

// escribe bytes encolados
extern size_t metrics_average_bytes_per_write;
extern size_t total_writes;
unsigned copy_write(struct selector_key *key)
{
    char * etiqueta = "COPY WRITE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct copy_st *d = copy_ptr(key);

    assert(d->fd == key->fd);
    size_t size;
    ssize_t n;
    buffer *b = d->wb;
    unsigned ret = COPY;

    debug(etiqueta, 0, "Starting write", key->fd);
    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);

    if (n == -1)
    {
        shutdown(d->fd, SHUT_WR);
        d->interest &= ~OP_WRITE;
        if (d->other_copy->fd != -1)
        {
            shutdown(d->other_copy->fd, SHUT_RD);
            d->other_copy->interest &= ~OP_READ;
        }
    }
    else
    {
        //// Add written bytes to metrics
        metrics_historic_byte_transfer += n;
        if (total_writes == 0) {
            metrics_average_bytes_per_write = n;
            total_writes++;
        } else {
            metrics_average_bytes_per_write = ((metrics_average_bytes_per_write * total_writes) + n) / (total_writes + 1);
            total_writes++;
        }
        buffer_read_adv(b, n);
        debug(etiqueta, n, "Buffer read adv", key->fd);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other_copy);
    if (d->interest == OP_NOOP)
    {
        debug(etiqueta, n, "Socket has no more interests -> DONE state", key->fd);
        metrics_concurrent_connections -= 1;
        ret = DONE;
    }

    return ret;
}
