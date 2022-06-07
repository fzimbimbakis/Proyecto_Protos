#include <errno.h>
#include <string.h>
#include "../../include/connecting.h"

#define FIXED_IP "127.0.0.1"
#define FIXED_PORT 3000
extern const struct fd_handler socks5_handler;
void connecting_init(const unsigned state, struct selector_key *key){
    // TODO(bruno) Error handling

    char * etiqueta = "CONNECTING INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct socks5 * data = key->data;
    //// Me conecto al destino fijo
    debug(etiqueta, 0, "Creating socket for fixed destination", key->fd);

    const int D_new_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(D_new_socket < 0){
        debug(etiqueta, D_new_socket, "Error creating socket for fixed destination", key->fd);
        goto fail;
    }else
        debug(etiqueta, D_new_socket, "Created socket for fixed destination", key->fd);
    //// Socket no bloqueante
    int flag_setting = selector_fd_set_nio(D_new_socket);
    if(flag_setting == -1) {
        debug(etiqueta, flag_setting, "Error setting socket flags", key->fd);
        // TODO(bruno) Ver que se hace acá
        exit(EXIT_FAILURE);
    }

    debug(etiqueta, 0, "Connecting socket for fixed destination", key->fd);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(FIXED_IP);
    address.sin_port = htons(FIXED_PORT);
    int connectResult = connect(D_new_socket, (struct sockaddr*)&address, sizeof(address));
    if(connectResult != 0 && errno != 36){
        debug(etiqueta, connectResult, "Connection socket for fixed destination failed", key->fd);
        exit(EXIT_FAILURE);
    }
    debug(etiqueta, connectResult, "Me suscribo a escritura para esperar que se complete la conexión", key->fd);
    data->origin_fd = D_new_socket;
    selector_set_interest(key->s, key->fd, OP_WRITE);
    debug(etiqueta, 0, "Finished stage", key->fd);
    return;
    fail:
    debug(etiqueta, 0, "Fail", 0);
    fprintf(stderr, "%s\n",strerror(errno));
    exit(EXIT_FAILURE);
}
unsigned connecting_read(struct selector_key *key){
    char * etiqueta = "CONNECTING READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    return REQUEST_CONNECTING;
}
unsigned connecting_write(struct selector_key *key){
    char * etiqueta = "CONNECTING WRITE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    debug(etiqueta, 0, "Se completo la conexión -> Cambio de estado a COPY", key->fd);
    selector_set_interest_key(key, OP_NOOP);
    struct socks5 * data = key->data;
    selector_register(key->s, data->origin_fd, &socks5_handler, OP_NOOP, data);
    debug(etiqueta, 0, "Finished stage", key->fd);
    return COPY;
}
void connecting_close(const unsigned state, struct selector_key *key){
    char * etiqueta = "CONNECTING CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);

}