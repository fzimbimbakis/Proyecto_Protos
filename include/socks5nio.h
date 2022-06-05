#ifndef PROYECTO_PROTOS_SOCKS5NIO_H
#define PROYECTO_PROTOS_SOCKS5NIO_H

#include <netdb.h>
#include "selector.h"



/**handler del socket pasivo que atiende conexiones socksv5 */

void
socksv5_passive_accept(struct selector_key *key);

/** libreria de pools internos*/
void
socksv5_pool_destroy(void);

#endif //PROYECTO_PROTOS_SOCKS5NIO_H
