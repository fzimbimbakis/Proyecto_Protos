#include "../../include/authentication.h"
extern struct users users[MAX_USERS];
extern int nusers;

#define VERSION_ERROR 32
int checkVersion(uint8_t const * ptr, uint8_t size, uint8_t *error){
    if(size != 1 || *ptr != 0x01) {
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
void auth_read_init(unsigned state, struct selector_key *key){
    char * etiqueta = "AUTH READ INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);

    int total_states = 5;

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);

    //// Read version
    d->parser->states[0] = malloc(sizeof(parser_substate));
    d->parser->states[0]->state = long_read;
    d->parser->states[0]->remaining = d->parser->states[0]->size = 1;
    d->parser->states[0]->result = malloc(sizeof(uint8_t));
    d->parser->states[0]->check_function = checkVersion;

    //// Nread for username
    d->parser->states[1] = malloc(sizeof(parser_substate));
    d->parser->states[1]->state = read_N;

    //// Read username
    d->parser->states[2] = malloc(sizeof(parser_substate));
    d->parser->states[2]->state = long_read;

    //// Nread for username
    d->parser->states[3] = malloc(sizeof(parser_substate));
    d->parser->states[3]->state = read_N;

    //// Read password
    d->parser->states[4] = malloc(sizeof(parser_substate));
    d->parser->states[4]->state = long_read;

    parser_init(d->parser);

    debug(etiqueta, 0, "Finished stage", key->fd);
}


/**
 * auth_read
 * Reads and parses client input
 * @param key
 */
unsigned auth_read(struct selector_key *key){
    char * etiqueta = "AUTH READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    unsigned  ret      = USERPASS_READ;
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
        debug(etiqueta, n, "Starting userpass consume", key->fd);
        const enum parser_state st = consume(d->rb, d->parser, &error);
        if(is_done(st, 0)) {
            debug(etiqueta, error, "Finished userpass consume", key->fd);
            debug(etiqueta, 0, "Setting selector interest to write", key->fd);
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                debug(etiqueta, 0, "Starting authorization data processing", 0);
                ret = auth_process(d, key->data);
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


/**
 * auth_read_close
 * Close resources
 * @param state
 * @param key
 */
void auth_read_close(unsigned state, struct selector_key *key){

}


/**
 * auth_write_init
 * Initializes userpass_st variables
 * @param state
 * @param key
 */
void auth_write_init(unsigned state, struct selector_key *key){

}


/**
 * auth_write
 * Checks authentication and wirtes answer to client
 * @param key
 */
void auth_write(struct selector_key *key){

}


/**
 * auth_write_close
 * Close resources
 * @param state
 * @param key
 */
void auth_write_close(unsigned state, struct selector_key *key){

}

/**
 * Auxiliar function for auth_process
 * @param username
 * @param password
 * @return
 */
uint8_t checkCredentials(uint8_t * username, uint8_t * password){
    for (int i = 0; i < nusers; ++i) {
        if(strcmp((char *)username, users[i].name) == 0){
            if(strcmp((char *)password, users[i].pass) == 0)
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
int auth_process(struct userpass_st *d, socks5 * data){
    char * etiqueta = "AUTH PROCESS";
    debug(etiqueta, 0, "Starting authorization data processing", 0);
    uint8_t * username = d->parser->states[2]->result;
    uint8_t * password = d->parser->states[4]->result;
//    d->user = username;
//    d->password = password;
    d->auth_result = checkCredentials(username, password);
    if(d->auth_result == 0x00)
        debug(etiqueta, 0, "Access granted", 0);
    else
        debug(etiqueta, 0, "Access Denied", 0);
    return USERPASS_WRITE;
}


