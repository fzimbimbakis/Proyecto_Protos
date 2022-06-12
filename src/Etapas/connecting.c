#include <errno.h>
#include <string.h>
#include "../../include/connecting.h"



#define IPV4_LEN 4
#define IPV6_LEN 16
extern const struct fd_handler socks5_handler;

 #include <errno.h>
#include <string.h>
#include "../../include/connecting.h"

void connection(struct selector_key *key);
enum socks_reply_status errno_to_socks(int e);

//// INIT

void connecting_init(const unsigned state, struct selector_key *key){
    char * etiqueta = "CONNECTION";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct socks5 * data = ATTACHMENT(key);
    data->orig.conn.wb = &data->write_buffer;
    data->orig.conn.client_fd = data->client_fd;
    data->orig.conn.origin_fd = -1;

    int *fd= &data->origin_fd;

    debug(etiqueta, 0, "Creating socket", key->fd);
    *fd= socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);

    if(*fd < 0){
        debug(etiqueta, *fd, "Error creating socket for origin", key->fd);
        goto fail;
    }else
        debug(etiqueta, *fd, "Created socket for origin", key->fd);

    //// Socket no bloqueante
    int flag_setting = selector_fd_set_nio(*fd);
    if(flag_setting == -1) {
        debug(etiqueta, flag_setting, "Error setting socket flags", key->fd);
        goto fail;
    }

    connection(key);

    return;

    fail:
    debug(etiqueta, 0, "Fail", 0);
    fprintf(stderr, "%s\n",strerror(errno));
    exit(EXIT_FAILURE);     //// TODO Exit?
}

//// WRITE
unsigned connecting_write(struct selector_key *key){
    char * etiqueta = "CONNECTING WRITE";
    debug(etiqueta, 0, "Starting stage", key->fd);

    int error;
    socklen_t len= sizeof(error);
    struct socks5 * data = ATTACHMENT(key);

    if(getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0){
        //// Error on getsockopt

        debug(etiqueta, 0, "Error on getsockopt -> REQUEST_WRITE to reply error to client", key->fd);
        data->orig.conn.status = status_general_socks_server_failure;
        request_marshall(data->orig.conn.status, &data->write_buffer);
        selector_set_interest_key(key, OP_WRITE);
        return REQUEST_WRITE;

    }

    if(error== 0){                                                              //// Check connection status

                                                                                //// Connection succeeded
        debug(etiqueta, 0, "Connection succeed", key->fd);
        data->orig.conn.status=status_succeeded;
        data->orig.conn.origin_fd = key->fd;
        request_marshall(data->orig.conn.status, &data->write_buffer);
        selector_set_interest(key->s,data->client_fd, OP_WRITE);
        selector_set_interest_key(key, OP_NOOP);
        return REQUEST_WRITE;

    }else{                                                                      //// Connection refused, check next IP if any

        debug(etiqueta, 0, "Connection failed. Checking other IPs", key->fd);
        data->orig.conn.status = errno_to_socks(error);

        if(data->origin_resolution_current->ai_next != NULL){                   //// Check if next IP exists


            debug(etiqueta, 0, "Checking next IP", key->fd);
            struct addrinfo * current = data->origin_resolution_current = data->origin_resolution_current->ai_next;

            //// IPv4
            if(current->ai_family == AF_INET)
                memcpy((struct sockaddr_in *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in));

            //// IPv6
            if(current->ai_family == AF_INET6)
                memcpy((struct sockaddr_in6 *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in6));

            connection(key);

        } else{
            debug(etiqueta, 0, "No more IPs -> REQUEST_WRITE to reply error to client", key->fd);
            request_marshall(errno_to_socks(error), &data->write_buffer);
            selector_set_interest_key(key, OP_WRITE);
            return REQUEST_WRITE;
        }

    }


    int request_marshall_result = request_marshall(data->orig.conn.status, &data->write_buffer);
    if(-1 == request_marshall_result){
        debug(etiqueta, request_marshall_result, "Error request marshall", key->fd);
        abort();
    }

    selector_status s=0;
    s|= selector_set_interest(key->s, data->orig.conn.client_fd, OP_WRITE);
    s|= selector_set_interest_key(key, OP_NOOP);

    debug(etiqueta, s, "Finished stage", key->fd);
    return SELECTOR_SUCCESS == s ? REQUEST_CONNECTING:ERROR;
}

//// CLOSE
void connecting_close(const unsigned state, struct selector_key *key){
    char * etiqueta = "CONNECTING CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct request_st *d = &ATTACHMENT(key)->client.request;
    request_parser_close(d->parser);
    free(d->parser);
    d->parser = NULL;
    freeaddrinfo(ATTACHMENT(key)->origin_resolution);
    debug(etiqueta, 0, "Finished stage", key->fd);
}


enum socks_reply_status errno_to_socks(int e){
    enum socks_reply_status ret;

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
        default:
            ret = status_general_socks_server_failure;
            break;
    }

    return ret;
}

void connection(struct selector_key *key){
    // TODO(bruno) Error handling
    char * etiqueta = "CONNECTION";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct socks5 * data = ATTACHMENT(key);

    debug(etiqueta, 0, "Connecting socket to origin", key->fd);
    int *fd= &data->origin_fd;
    int connectResult = connect(*fd, (const struct sockaddr*)&ATTACHMENT(key)->origin_addr, ATTACHMENT(key)->origin_addr_len);

    if(connectResult != 0 && errno != EINPROGRESS){
        debug(etiqueta, connectResult, "Connection for origin socket failed", key->fd);
        *data->client.request.status = errno_to_socks(errno);
        goto fail;
    }

    if(connectResult != 0){     //// EINPROGRESS
        selector_status st= selector_set_interest_key(key, OP_NOOP);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            goto fail;
        }

        debug(etiqueta, connectResult, "Me suscribo a escritura para esperar que se complete la conexión", key->fd);
        st= selector_register(key->s, *fd, &socks5_handler,OP_WRITE, key->data);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            goto fail;
        }
        ATTACHMENT(key)->references += 1;           // TODO ?
    }
    else{     //// Connected with no EINPROGRESS
        ATTACHMENT(key)->references += 1;           // TODO ?
        selector_status st= selector_set_interest_key(key, OP_READ);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            goto fail;
        }

        debug(etiqueta, connectResult, "Me suscribo a escritura para esperar que se complete la conexión", key->fd);
        st= selector_register(key->s, *fd, &socks5_handler,OP_READ, key->data);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            goto fail;
        }
    }


    debug(etiqueta, 0, "Finished stage", key->fd);
    return;

    fail:
    debug(etiqueta, 0, "Fail", 0);
    fprintf(stderr, "%s\n",strerror(errno));
    exit(EXIT_FAILURE);     //// TODO Exit?
}
