#ifndef PROYECTO_PROTOS_MNG_REQUEST_H
#define PROYECTO_PROTOS_MNG_REQUEST_H

#include "selector.h"

//// REQUEST READ INDEX

void mng_request_index_init(unsigned state, struct selector_key *key);

unsigned mng_request_index_read(struct selector_key *key);

void mng_request_index_close(unsigned state, struct selector_key *key);

//// REQUEST READ

void mng_request_init(unsigned state, struct selector_key *key);

unsigned mng_request_read(struct selector_key *key);

void mng_request_close(unsigned state, struct selector_key *key);

//// REQUEST WRITE

unsigned mng_request_write(struct selector_key *key);

#endif //PROYECTO_PROTOS_MNG_REQUEST_H
