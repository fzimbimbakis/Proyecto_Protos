#include <stdlib.h>
#include "../../include/mng_request.h"
#include "../../include/states.h"
#include "../../include/socks5nio.h"
#include "../../include/mng_nio.h"

struct parser *mng_request_index_add_user_parser_init(struct selector_key *key);

struct parser *mng_request_index_delete_user_parser_init(struct selector_key *key);

struct parser *mng_request_index_yes_no_value(struct selector_key *key);

enum mng_state process_mng_index(struct selector_key *key, buffer *wb, enum mng_request_indexes index);

void error_mng_marshal(buffer *wb, enum mng_reply_status status);

void process_mng_params_request(struct selector_key *key, buffer *wb, enum mng_request_indexes index);

void list_users_mng_request_marshall(buffer *wb, struct selector_key *key);
//// REQUEST READ INDEX

int checkIndex(uint8_t const *ptr, uint8_t size, uint8_t *error) {
    switch (*ptr) {
        case mng_request_index_supported_indexes:
        case mng_request_index_list_users:
        case mng_request_index_historic_connections:
        case mng_request_index_concurrent_connections:
        case mng_request_index_max_concurrent_connections:
        case mng_request_index_historic_bytes_transferred:
        case mng_request_index_historic_auth_attempts:
        case mng_request_index_historic_connections_attempts:
        case mng_request_index_average_bytes_per_read:
        case mng_request_index_average_bytes_per_write:
        case mng_request_index_add_user:
        case mng_request_index_delete_user:
        case mng_request_index_disable_auth:
        case mng_request_index_disable_password_disectors:
        case mng_request_index_shutdown_server:
            return true;
        default: {
            *error = mng_status_index_not_supported;
            return false;
        }
    }
}

void mng_request_index_init(const unsigned state, struct selector_key *key) {
    char *etiqueta = "MNG REQUEST INDEX INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 1;

    d->parser = malloc(sizeof(*d->parser));

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);

    //// Read index
    d->parser->states[0] = malloc(sizeof(parser_substate));
    d->parser->states[0]->state = long_read;
    d->parser->states[0]->remaining = d->parser->states[0]->size = 1;
    d->parser->states[0]->result = malloc(sizeof(uint8_t) + 1);
    d->parser->states[0]->check_function = checkIndex;

    parser_init(d->parser);

    debug(etiqueta, 0, "Finished stage", key->fd);
}

unsigned mng_request_index_read(struct selector_key *key) {

    char *etiqueta = "MNG REQUEST INDEX READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    unsigned ret = MNG_REQUEST_READ_INDEX;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    debug(etiqueta, 0, "Reading from client", key->fd);
    ptr = buffer_write_ptr(d->rb, &count);
    if (count <= 0)
        return ERROR;
    n = recv(key->fd, ptr, 1, 0);       //// Leo solo uno para ver el index
    if (n > 0) {

        buffer_write_adv(d->rb, n);
        debug(etiqueta, n, "Finished reading", key->fd);

        debug(etiqueta, 0, "Starting mng index consume", key->fd);
        const enum parser_state st = consume(d->rb, d->parser, &error);

        if (is_done(st, 0)) {
            debug(etiqueta, error, "Finished mng index consume", key->fd);
            debug(etiqueta, 0, "Setting selector interest to write", key->fd);
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                if (error) {
                    debug(etiqueta, mng_status_index_not_supported,
                          "MNG index no supported -> Change to write to notify client", 0);
                    selector_set_interest_key(key, OP_WRITE);
                    d->status = mng_status_index_not_supported;
                    ret = MNG_REQUEST_WRITE;
                } else {
                    d->index = *d->parser->states[0]->result;
                    ret = process_mng_index(key, d->wb, d->index);
                }
            } else {
                ret = ERROR;
            }
        }
    } else {
        debug(etiqueta, n, "Error, nothing to read", key->fd);
        ret = ERROR;
    }
    debug(etiqueta, error, "Finished stage", key->fd);
    return ret;

}

void mng_request_index_close(const unsigned state, struct selector_key *key) {
    char *etiqueta = "MNG READ INDEX CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct parser *p = ATTACHMENT(key)->client.mng_request.parser;
    for (int i = 0; i < p->size; ++i) {
        if (p->states[i]->state == long_read) {
            free(p->states[i]->result);
        }
        free(p->states[i]);
    }
    free(p->states);
    free(p);
    debug(etiqueta, 0, "Finished stage", key->fd);
}

//// REQUEST READ

void mng_request_init(const unsigned state, struct selector_key *key) {
    char *etiqueta = "MNG REQUEST INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    switch (ATTACHMENT(key)->client.mng_request.index) {
        case mng_request_index_add_user: {
            mng_request_index_add_user_parser_init(key);
            break;
        }
        case mng_request_index_delete_user: {
            mng_request_index_delete_user_parser_init(key);
            break;
        }
        case mng_request_index_disable_auth:
        case mng_request_index_disable_password_disectors:
            mng_request_index_yes_no_value(key);
            break;
        default: {
            debug(etiqueta, 0, "MNG REQUEST INIT WITH INVALID INDEX. Bug!", 0);
            abort();
        }
    }

    debug(etiqueta, 0, "Finished stage", key->fd);
}

unsigned mng_request_read(struct selector_key *key) {
    char *etiqueta = "MNG REQUEST READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    unsigned ret = MNG_REQUEST_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    debug(etiqueta, 0, "Reading from client", key->fd);
    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, n, 0);       //// Leo solo uno para ver el index
    if (n > 0) {

        buffer_write_adv(d->rb, n);
        debug(etiqueta, n, "Finished reading", key->fd);

        debug(etiqueta, 0, "Starting mng consume", key->fd);
        const enum parser_state st = consume(d->rb, d->parser, &error);

        if (is_done(st, 0)) {
            debug(etiqueta, error, "Finished mng consume", key->fd);
            debug(etiqueta, 0, "Setting selector interest to write", key->fd);
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                debug(etiqueta, 0, "Starting mng request data processing", 0);
                process_mng_params_request(key, d->wb, d->index);
                ret = MNG_REQUEST_WRITE;
            } else {
                ret = ERROR;
            }
        }
    } else {
        debug(etiqueta, n, "Error, nothing to read", key->fd);
        ret = ERROR;
    }
    debug(etiqueta, error, "Finished stage", key->fd);
    return ret;
}

void mng_request_close(const unsigned state, struct selector_key *key) {
    char *etiqueta = "MNG READ INDEX CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct parser *p = ATTACHMENT(key)->client.mng_request.parser;
    for (int i = 0; i < p->size; ++i) {
        if (p->states[i]->state == long_read) {
            free(p->states[i]->result);
        }
        free(p->states[i]);
    }
    free(p->states);
    free(p);
    debug(etiqueta, 0, "Finished stage", key->fd);
}

//// REQUEST WRITE

unsigned mng_request_write(struct selector_key *key) {
    // TODO
}


//// INDEX PROCESS
void uint_mng_request_marshall(buffer *wb, uint32_t value);

void supported_indexes_mng_request_marshall(buffer *wb);

enum mng_state process_mng_index(struct selector_key *key, buffer *wb, enum mng_request_indexes index) {
    switch (index) {
        case mng_request_index_supported_indexes: {
            supported_indexes_mng_request_marshall(wb);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_list_users: {
            list_users_mng_request_marshall(wb, key);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_connections: {
            uint_mng_request_marshall(wb, 1);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_concurrent_connections: {
            uint_mng_request_marshall(wb, 2);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_max_concurrent_connections: {
            uint_mng_request_marshall(wb, 3);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_bytes_transferred: {
            uint_mng_request_marshall(wb, 4);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_auth_attempts: {
            uint_mng_request_marshall(wb, 5);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_connections_attempts: {
            uint_mng_request_marshall(wb, 6);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_average_bytes_per_read: {
            uint_mng_request_marshall(wb, 7);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_average_bytes_per_write: {
            uint_mng_request_marshall(wb, 8);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_add_user:
        case mng_request_index_delete_user:
        case mng_request_index_disable_auth:
        case mng_request_index_disable_password_disectors:
            return MNG_REQUEST_READ;

        case mng_request_index_shutdown_server:
            abort();
        default: {
            return MNG_ERROR;
        }
    }
}

//// PROCESS PARAMETERS

void process_mng_params_request(struct selector_key *key, buffer *wb, enum mng_request_indexes index) {
    // TODO
}

//// MNG REQUEST MARSHALS

void uint_mng_request_marshall(buffer *wb, uint32_t value) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(wb, &n);
    if (n < 5)
        return;
    buf[0] = 0x00;
    uint8_t *aux = (uint8_t *) (&(value));
    buf[1] = aux[0];
    buf[2] = aux[1];
    buf[3] = aux[2];
    buf[4] = aux[3];
    buffer_write_adv(wb, 5);
}

void supported_indexes_mng_request_marshall(buffer *wb) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(wb, &n);
    if (n < 18)
        return;
    int i = 0;
    buf[i++] = 0x00;
    buf[i++] = 0x0F;
    buf[i++] = 0x00;
    buf[i++] = 0x01;
    buf[i++] = 0x02;
    buf[i++] = 0x03;
    buf[i++] = 0x04;
    buf[i++] = 0x05;
    buf[i++] = 0x06;
    buf[i++] = 0x07;
    buf[i++] = 0x08;
    buf[i++] = 0x09;
    buf[i++] = 0x0A;
    buf[i++] = 0x0B;
    buf[i++] = 0x0C;
    buf[i++] = 0x0D;
    buf[i++] = 0xFF;

    buffer_write_adv(wb, i);
}

void list_users_mng_request_marshall(buffer *wb, struct selector_key *key) {
    // TODO
}

void error_mng_marshal(buffer *wb, enum mng_reply_status status) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(wb, &n);
    if (n < 1)
        return;
    buf[0] = status;

    buffer_write_adv(wb, 1);
}

//// REQUEST PARSER INIT

struct parser *mng_request_index_add_user_parser_init(struct selector_key *key) {
    char *etiqueta = "MNG REQUEST ADD USER PARSER INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 4;

    d->parser = malloc(sizeof(*d->parser));

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);

    //// Nread for username
    d->parser->states[1] = malloc(sizeof(parser_substate));
    d->parser->states[1]->state = read_N;

    //// Read username
    d->parser->states[2] = malloc(sizeof(parser_substate));
    d->parser->states[2]->state = long_read;
    d->parser->states[2]->check_function = NULL;

    //// Nread for password
    d->parser->states[3] = malloc(sizeof(parser_substate));
    d->parser->states[3]->state = read_N;

    //// Read password
    d->parser->states[4] = malloc(sizeof(parser_substate));
    d->parser->states[4]->state = long_read;
    d->parser->states[4]->check_function = NULL;

    parser_init(d->parser);

    debug(etiqueta, 0, "Finished stage", key->fd);
}

struct parser *mng_request_index_delete_user_parser_init(struct selector_key *key) {
    char *etiqueta = "MNG REQUEST DELETE USER PARSER INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 2;

    d->parser = malloc(sizeof(*d->parser));

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);

    //// Nread for username
    d->parser->states[1] = malloc(sizeof(parser_substate));
    d->parser->states[1]->state = read_N;

    //// Read username
    d->parser->states[2] = malloc(sizeof(parser_substate));
    d->parser->states[2]->state = long_read;
    d->parser->states[2]->check_function = NULL;

    parser_init(d->parser);

    debug(etiqueta, 0, "Finished stage", key->fd);
}

struct parser *mng_request_index_yes_no_value(struct selector_key *key) {
    char *etiqueta = "AUTH READ INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 1;

    d->parser = malloc(sizeof(*d->parser));

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);

    //// Read version
    d->parser->states[0] = malloc(sizeof(parser_substate));
    d->parser->states[0]->state = long_read;
    d->parser->states[0]->remaining = d->parser->states[0]->size = 1;
    d->parser->states[0]->result = malloc(sizeof(uint8_t) + 1);
    d->parser->states[0]->check_function = NULL;

    parser_init(d->parser);

    debug(etiqueta, 0, "Finished stage", key->fd);
}
