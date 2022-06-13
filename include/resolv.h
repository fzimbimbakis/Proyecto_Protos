#ifndef PROYECTO_PROTOS_RESOLV_H
#define PROYECTO_PROTOS_RESOLV_H

#include "debug.h"
#include <netdb.h>
#include <pthread.h>
#include <netinet/in.h>
#include "socks5nio.h"
#include "request.h"
/**
 * Handler del pedido de obtener la información de un FQDN
 * @param ptr
 * @return
 */
void *request_resolv_blocking(void *ptr);

/**
 * Función que se llama cuando se termino la obtención de la información del FQDN
 * @param key
 * @return
 */
unsigned request_resolv_done(struct selector_key *key);

#endif //PROYECTO_PROTOS_RESOLV_H
