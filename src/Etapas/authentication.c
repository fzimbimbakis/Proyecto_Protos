#include "../../include/authentication.h"
#include "selector.h"
#include "myParser.h"
#include "states.h"
#include "socks5nio.h"
#include "parser.h"
#include <string.h>
#include <stdlib.h>
extern struct users users[MAX_USERS];
extern int nusers;
//#define MSG_NOSIGNAL      0x2000  /* don't raise SIGPIPE */
#define VERSION_ERROR 32

//// READ   //////////////////////////////////////////////////

int checkVersion(uint8_t const *ptr, uint8_t size, uint8_t *error) {
    if (size != 1 || *ptr != USERPASS_METHOD_VERSION) {
        *error = VERSION_ERROR;
        return false;
    }
    return true;
}

/**
 * auth_read_init
 * Initializes userpass_st variables
 * @param state
 * @param key
 */
void auth_read_init(unsigned state, struct selector_key *key) {
    char *etiqueta = "AUTH READ INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 5;

    d->parser = malloc(sizeof(*d->parser));

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);

    //// Read version
    d->parser->states[0] = malloc(sizeof(parser_substate));
    d->parser->states[0]->state = long_read;
    d->parser->states[0]->remaining = d->parser->states[0]->size = 1;
    d->parser->states[0]->result = malloc(sizeof(uint8_t) + 1);
    d->parser->states[0]->check_function = checkVersion;

    //// Nread for username
    d->parser->states[1] = malloc(sizeof(parser_substate));
    d->parser->states[1]->state = read_N;

    //// Read username
    d->parser->states[2] = malloc(sizeof(parser_substate));
    d->parser->states[2]->state = long_read;
    d->parser->states[2]->check_function = NULL;

    //// Nread for username
    d->parser->states[3] = malloc(sizeof(parser_substate));
    d->parser->states[3]->state = read_N;

    //// Read password
    d->parser->states[4] = malloc(sizeof(parser_substate));
    d->parser->states[4]->state = long_read;
    d->parser->states[4]->check_function = NULL;

    parser_init(d->parser);

    debug(etiqueta, 0, "Finished stage", key->fd);
}


/**
 * auth_read
 * Reads and parses client input
 * @param key
 */
unsigned auth_read(struct selector_key *key) {
    char *etiqueta = "AUTH READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    unsigned ret = USERPASS_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    debug(etiqueta, 0, "Reading from client", key->fd);
    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);
    if (n > 0) {

        buffer_write_adv(d->rb, n);
        debug(etiqueta, n, "Finished reading", key->fd);

        debug(etiqueta, 0, "Starting userpass consume", key->fd);
        const enum parser_state st = consume(d->rb, d->parser, &error);

        if (is_done(st, 0)) {
            debug(etiqueta, error, "Finished userpass consume", key->fd);
            debug(etiqueta, 0, "Setting selector interest to write", key->fd);
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                debug(etiqueta, 0, "Starting authorization data processing", 0);
                ret = auth_process(d, key);
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


int auth_reply(buffer *b, const uint8_t result) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(b, &n);
    if (n < 2) {
        return -1;
    }
    buf[0] = USERPASS_METHOD_VERSION;
    buf[1] = result;
    buffer_write_adv(b, 2);
    return 2;
}

/**
 * auth_read_close
 * Close resources
 * @param state
 * @param key
 */
void auth_read_close(unsigned state, struct selector_key *key) {
    char *etiqueta = "AUTH READ CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct parser *p = ATTACHMENT(key)->client.userpass.parser;
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


//// WRITE   /////////////////////////////////////////////////

/**
 * auth_write_init
 * Initializes userpass_st variables
 * @param state
 * @param key
 */
void auth_write_init(unsigned state, struct selector_key *key) {
    char *etiqueta = "AUTH WRITE INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);
    debug(etiqueta, 0, "Finished stage", key->fd);
}


/**
 * auth_write
 * Checks authentication and wirtes answer to client
 * @param key
 */
unsigned auth_write(struct selector_key *key) {
    char *etiqueta = "AUTH WRITE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    struct socks5 * data = ATTACHMENT(key);
    unsigned ret = USERPASS_WRITE;
    uint8_t *ptr;
    size_t count;
    ssize_t n;


    debug(etiqueta, 0, "Writing to client", key->fd);
    auth_reply(d->wb, ATTACHMENT(key)->authentication);
    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);
    if (n == -1) {
        ret = ERROR;
    } else {
        buffer_read_adv(d->wb, n);
        debug(etiqueta, 0, "Finished writing auth result to client", key->fd);
        if (!buffer_can_read(d->wb)) {
            if(data->authentication != 0x00){
                debug(etiqueta, 0, "Access denied -> Closing connection", key->fd);
                return DONE;
            }
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                debug(etiqueta, 0, "Setting interest to read", key->fd);
                ret = REQUEST_READ;
            } else {
                debug(etiqueta, 0, "Error on selector", key->fd);
                ret = ERROR;
            }
        }
    }
    debug(etiqueta, 0, "Finished stage", key->fd);
    return ret;
}


/**
 * auth_write_close
 * Close resources
 * @param state
 * @param key
 */
void auth_write_close(unsigned state, struct selector_key *key) {
    char *etiqueta = "AUTH WRITE CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);

    debug(etiqueta, 0, "Finished stage", key->fd);
}

/**
 * Auxiliar function for auth_process
 * @param username
 * @param password
 * @return
 */
uint8_t checkCredentials(uint8_t *username, uint8_t *password) {
    for (int i = 0; i < nusers; ++i) {
        if (strcmp((char *) username, users[i].name) == 0) {
            if (strcmp((char *) password, users[i].pass) == 0)
                return 0x00;
        }
    }
    return 0x01;
}

/**
 *
 * @param d Userpass State (Contains client input credentials)
 * @param data
 * @return
 */
int auth_process(struct userpass_st *d, struct selector_key * key) {
    char *etiqueta = "AUTH PROCESS";
    debug(etiqueta, 0, "Starting authorization data processing", 0);
    struct socks5 * data = key->data;
    uint8_t *username = d->parser->states[2]->result;
    uint8_t *password = d->parser->states[4]->result;
    data->authentication = checkCredentials(username, password);
    if (data->authentication == 0x00)
        debug(etiqueta, 0, "Access granted", 0);
    else
        debug(etiqueta, 0, "Access Denied", 0);
    return USERPASS_WRITE;
}


