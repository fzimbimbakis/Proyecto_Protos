#include <errno.h>
#include <string.h>
#include "../../include/connecting.h"

#define FIXED_IP "127.0.0.1"
#define FIXED_PORT 3000
extern const struct fd_handler socks5_handler;
/*
void connecting_init(const unsigned state, struct selector_key *key){
    // TODO(bruno) Error handling

    char * etiqueta = "CONNECTING INIT";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct socks5 * data = key->data;
    //// Me conecto al destino fijo
    debug(etiqueta, 0, "Creating socket for fixed destination", key->fd);

    const int D_new_socket = socket(data->origin_domain, SOCK_STREAM, IPPROTO_TCP);

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

    int connectResult = connect(D_new_socket, (struct sockaddr*)&data->origin_addr, data->origin_addr_len);
    if(connectResult != 0 && errno != EINPROGRESS){
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
    // TODO FALTA EL LLAMADO A setsockopt
    //  De juan:   if (getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) Me tengo que fijar el error creo.
    selector_set_interest_key(key, OP_WRITE);
    struct socks5 * data = key->data;
    selector_register(key->s, data->origin_fd, &socks5_handler, OP_NOOP, data);
    debug(etiqueta, 0, "Finished stage", key->fd);
    return REQUEST_WRITE;
}
void connecting_close(const unsigned state, struct selector_key *key){
    char * etiqueta = "CONNECTING CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);

}
*/



 #include <errno.h>
#include <string.h>
#include "../../include/connecting.h"

#define FIXED_IP "127.0.0.1"
#define FIXED_PORT 3000
extern const struct fd_handler socks5_handler;

enum socks_reply_status errno_to_socks(int e)
{
    enum socks_reply_status ret = status_general_socks_server_failure;

    switch (e)
    {
        case 0:
            ret = status_succeeded;
            break;
        case ECONNREFUSED:
            ret = status_connection_refused;
            break;
        case EHOSTUNREACH:
            ret = status_host_unreachable;
            break;
        case ENETUNREACH:
            ret = status_network_unreachable;
            break;
        case ETIMEDOUT:
            ret = status_ttl_expired;
            break;
    }

    return ret;
}
void connection(struct selector_key *key);
void connecting_init(const unsigned state, struct selector_key *key){
    struct socks5 * data = ATTACHMENT(key);
    data->orig.conn.wb = &data->write_buffer;
    data->orig.conn.client_fd = data->client_fd;
    data->orig.conn.origin_fd = -1;
    connection(key);
}


void connection(struct selector_key *key){
    // TODO(bruno) Error handling
    //bool error= false;
    char * etiqueta = "CONNECTION";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct socks5 * data = ATTACHMENT(key);
    int *fd= &data->origin_fd;
    //// Me conecto al destino fijo
    debug(etiqueta, 0, "Creating socket", key->fd);

    //const int D_new_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    *fd= socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);
    if(*fd < 0){
        debug(etiqueta, *fd, "Error creating socket for fixed destination", key->fd);
        //error=true;
        goto fail;
    }else
        debug(etiqueta, *fd, "Created socket for fixed destination", key->fd);
    //// Socket no bloqueante
    int flag_setting = selector_fd_set_nio(*fd);
    if(flag_setting == -1) {
        debug(etiqueta, flag_setting, "Error setting socket flags", key->fd);
        // TODO(bruno) Ver que se hace acá
        //error=true;
        goto fail;
    }

    debug(etiqueta, 0, "Connecting socket for fixed destination", key->fd);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(FIXED_IP);
    address.sin_port = htons(FIXED_PORT);
int connectResult = connect(*fd, (const struct sockaddr*)&ATTACHMENT(key)->origin_addr,
                            ATTACHMENT(key)->origin_addr_len);
//connect(D_new_socket, (struct sockaddr*)&address, sizeof(address));
if(connectResult != 0 && errno != EINPROGRESS){
debug(etiqueta, connectResult, "Connection socket for fixed destination failed", key->fd);
//error=true;
*data->client.request.status = errno_to_socks(errno);
goto fail;
}
debug(etiqueta, connectResult, "Me suscribo a escritura para esperar que se complete la conexión", key->fd);
//selector_set_interest(key->s, key->fd, OP_WRITE);

selector_status st= selector_set_interest_key(key, OP_NOOP);
if(SELECTOR_SUCCESS != st){
//error=true;
goto fail;
}

//const struct fd_handler socksv5 = {
//        .handle_read       = socksv5_passive_accept,
//        .handle_write      = socksv5_passive_accept,
//        .handle_close      = NULL, // nada que liberar
//};

st= selector_register(key->s, *fd, &socks5_handler,
                      OP_WRITE, key->data);
if(SELECTOR_SUCCESS != st){
//error=true;
goto fail;
}
ATTACHMENT(key)->references += 1;

debug(etiqueta, 0, "Finished stage", key->fd);
return;

fail:
debug(etiqueta, 0, "Fail", 0);
fprintf(stderr, "%s\n",strerror(errno));
exit(EXIT_FAILURE);
}

//habria que borrarlo
unsigned connecting_read(struct selector_key *key){
    char * etiqueta = "CONNECTING READ";
    debug(etiqueta, 0, "Starting stage", key->fd);
    return REQUEST_CONNECTING;
}

int
request_marshall(struct socks5* data){
    size_t count;
    buffer* b=data->orig.conn.wb;
    uint8_t *buff= buffer_write_ptr(b, &count);
    struct request_st * s = &data->client.request;
    if(count < 10)
        return -1;

    buff[0]=0x05;
    buff[1]=data->orig.conn.status;
    buff[2]=0x00;//rsv
    buff[3]=data->client.request.request->dest_addr_type;
    
    //// IPv4
    if(s->request->dest_addr_type == socks_req_addrtype_ipv4) {
        memcpy(buff + 4, ((uint8_t *) &(s->request->dest_addr.ipv4.sin_addr)), s->request->dest_addr.ipv4.sin_len);
        buffer_write_adv(b, s->request->dest_addr.ipv4.sin_len + 4);
    }
    
    //// IPv6
    if(s->request->dest_addr_type == socks_req_addrtype_ipv6) {
        memcpy(buff + 4, ((uint8_t *) &(s->request->dest_addr.ipv6.sin6_addr)), s->request->dest_addr.ipv6.sin6_len);
        buffer_write_adv(b, 10);
    }
    
    //// FQDN
    if(s->request->dest_addr_type == socks_req_addrtype_domain) {
//        buff[4] = s->request->dest_addr;
//        memcpy(buff + 5, s->request->dest_addr.fqdn, );
//        buffer_write_adv(b, 10);
    }

    return 10;
}


unsigned connecting_write(struct selector_key *key){
    char * etiqueta = "CONNECTING WRITE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    int error;
    socklen_t len= sizeof(error);
//    struct connecting* d= &ATTACHMENT(key)->orig.conn;
    struct socks5 * data = ATTACHMENT(key);
    if(getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0){
        data->orig.conn.status = status_general_socks_server_failure;
    }else{
        if(error== 0){
            data->orig.conn.status=status_succeeded;
            data->orig.conn.origin_fd = key->fd;
        }else{
            data->orig.conn.status = errno_to_socks(error);
        }
    }
    int request_marshall_result = request_marshall (ATTACHMENT(key));
    if(-1 == request_marshall_result){
        (data->orig.conn.status)=status_general_socks_server_failure;
        debug(etiqueta, request_marshall_result, "Error request marshall", key->fd);
        abort();
    }

    selector_status s=0;
    s|= selector_set_interest(key->s, data->orig.conn.client_fd, OP_WRITE);
    s|= selector_set_interest_key(key, OP_NOOP);

    debug(etiqueta, s, "Finished stage", key->fd);
    return SELECTOR_SUCCESS== s ? REQUEST_WRITE:ERROR;
}

//habria que borrarlo
void connecting_close(const unsigned state, struct selector_key *key){
    char * etiqueta = "CONNECTING CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
}

