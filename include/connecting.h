#ifndef PROYECTO_PROTOS_CONNECTING_H
#define PROYECTO_PROTOS_CONNECTING_H
#include "selector.h"
#include "debug.h"
#include "socks5nio.h"
#include "stm.h"
#include "address_utils.h"
#include "request.h"
#include <netinet/in.h>

/**
 * Etapa de conexión
 * Cuando termina pasa al estado REQUEST_WRITE.
 */
/**
 * Inicializa estructuras.
 * @param state
 * @param key
 */
void connecting_init(const unsigned state, struct selector_key *key);
/**
 * Se fija si se completo la conexión:
 *      - Se completo: Pasa a REQUEST_WRITE con status_succeed
 *      - No se completo: Prueba otra IP si es que hay alguna. Si no hay IPs, pasa a REQUEST_WRITE con status de error.
 * @param key
 * @return
 */
unsigned connecting_write(struct selector_key *key);
/**
 * Cierra recursos
 * @param state
 * @param key
 */
void connecting_close(const unsigned state, struct selector_key *key);
#endif //PROYECTO_PROTOS_CONNECTING_H
