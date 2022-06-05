#ifndef PROYECTO_PROTOS_SOCKS5NIO_H
#define PROYECTO_PROTOS_SOCKS5NIO_H

#include <netdb.h>
#include "selector.h"


enum socks_cmd
{
    socks_req_cmd_connect = 0x01,
    socks_req_cmd_bind = 0x02,
};

enum socks_atyp
{
    socks_req_addrtype_ipv4 = 0x01,
    socks_req_addrtype_domain = 0x03,
    socks_req_addrtype_ipv6 = 0x04,
};

/**handler del socket pasivo que atiende conexiones socksv5 */

void
socksv5_passive_accept(struct selector_key *key);

/** libreria de pools internos*/
void
socksv5_pool_destroy(void);

#endif //PROYECTO_PROTOS_SOCKS5NIO_H
