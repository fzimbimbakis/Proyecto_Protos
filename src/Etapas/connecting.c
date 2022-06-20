#include <errno.h>
#include <string.h>
#include "../../include/connecting.h"
#include "../../include/request.h"
#include "netutils.h"
#include <time.h>

#define IPV4_LEN 4
#define IPV6_LEN 16
extern const struct fd_handler socks5_handler;

enum socks_reply_status connection(struct selector_key *key);
enum socks_reply_status errno_to_socks(int e);

//// INIT

void connecting_init(const unsigned state, struct selector_key *key){
    char * etiqueta = "CONNECTION";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct socks5 * data = ATTACHMENT(key);
    data->orig.conn.wb = &data->write_buffer;
    data->orig.conn.client_fd = data->client_fd;
    data->orig.conn.origin_fd = -1;
    data->orig.conn.status=status_succeeded;

    int *fd= &data->origin_fd;

    debug(etiqueta, 0, "Creating socket", key->fd);
    *fd= socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);
    if(*fd < 0){
        debug(etiqueta, *fd, "Error creating socket for origin", key->fd);
        data->orig.conn.status=status_general_socks_server_failure;
        error_handler(data->orig.conn.status, key);
        return;
    }else
        debug(etiqueta, *fd, "Created socket for origin", key->fd);

    //// Socket no bloqueante
    int flag_setting = selector_fd_set_nio(*fd);
    if(flag_setting == -1) {
        debug(etiqueta, flag_setting, "Error setting socket flags", key->fd);
        data->orig.conn.status=status_general_socks_server_failure;
        error_handler(data->orig.conn.status, key);
        return;
    }

    data->orig.conn.status=connection(key);
    return;

    /*fail:
    debug(etiqueta, 0, "Fail", 0);
    fprintf(stderr, "%s\n",strerror(errno));
    exit(EXIT_FAILURE);     //// TODO Exit?*/
}

//// WRITE
extern struct users users[MAX_USERS];
extern size_t metrics_historic_connections;
extern size_t metrics_concurrent_connections;
extern size_t metrics_max_concurrent_connections;
unsigned connecting_write(struct selector_key *key){
    struct socks5 * data = ATTACHMENT(key);
    char * etiqueta = "CONNECTING WRITE";


    if(data->orig.conn.status != status_succeeded){
        debug(etiqueta, 0, "status != succeeded from init", key->fd);
        if(data->origin_resolution_current != NULL && data->origin_resolution_current->ai_next != NULL){                   //// Check if next IP exists


            debug(etiqueta, 0, "Checking next IP", key->fd);
            struct addrinfo * current = data->origin_resolution_current = data->origin_resolution_current->ai_next;

            //// IPv4
            if(current->ai_family == AF_INET)
                memcpy((struct sockaddr_in *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in));

            //// IPv6
            if(current->ai_family == AF_INET6)
                memcpy((struct sockaddr_in6 *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in6));

            connection(key);

            return REQUEST_CONNECTING;//vuelve a probar la siguiente ip

        }
        selector_unregister_fd(key->s, data->origin_fd);
        close(data->origin_fd);
        data->origin_fd=-1;
        return error_handler(data->orig.conn.status, key);
    }

    //TODO agregar un status al registro, quizas habria que moverlo
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    struct sockaddr * origAddr = (struct sockaddr *) &ATTACHMENT(key)->origin_addr;
    struct sockaddr * clientAddr = (struct sockaddr *) &ATTACHMENT(key)->client_addr;
    char * orig = malloc(ATTACHMENT(key)->origin_addr_len);
    char * client = malloc(ATTACHMENT(key)->client_addr_len);
    //printf("%s\t to: %s \t from: %s \t %s\n", users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, 100, origAddr), sockaddr_to_human(client, 100, clientAddr), asctime (timeinfo));


    debug(etiqueta, 0, "Starting stage", key->fd);


    int error;
    socklen_t len= sizeof(error);


    if(getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0){
        //// Error on getsockopt
        debug(etiqueta, 0, "Error on getsockopt -> REQUEST_WRITE to reply error to client", key->fd);
        data->orig.conn.status=status_general_socks_server_failure;
        printf("%s\t to: %s \t from: %s \t %s \t status: %d\n", users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, 100, origAddr), sockaddr_to_human(client, 100, clientAddr), asctime (timeinfo), data->orig.conn.status);
        return error_handler(data->orig.conn.status, key);
    }

    if(error== 0){                                                              //// Check connection status

                                                                                //// Connection succeeded
        //// Add connection to metrics
        metrics_historic_connections += 1;
        metrics_concurrent_connections += 1;
        if(metrics_concurrent_connections > metrics_max_concurrent_connections)
            metrics_max_concurrent_connections = metrics_concurrent_connections;

        debug(etiqueta, 0, "Connection succeed", key->fd);
        printf("%s\t to: %s \t from: %s \t %s \t status: %d\n", users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, 100, origAddr), sockaddr_to_human(client, 100, clientAddr), asctime (timeinfo), data->orig.conn.status);

        if(data->client.request.addr_family == socks_req_addrtype_domain)
            freeaddrinfo(data->origin_resolution);
        data->orig.conn.status=status_succeeded;
        data->orig.conn.origin_fd = key->fd;
        //request_marshall(data->orig.conn.status, &data->write_buffer);
        //selector_set_interest(key->s,data->client_fd, OP_WRITE);
        //selector_set_interest_key(key, OP_NOOP);
        //return REQUEST_WRITE;

    }else{                                                                      //// Connection refused, check next IP if any

        if(data->origin_resolution_current==NULL){
            debug(etiqueta, 0, "Connection refused -> REQUEST_WRITE to reply error to client", key->fd);
            data->orig.conn.status= errno_to_socks(error);
            printf("%s\t to: %s \t from: %s \t %s \t status: %d\n", users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, 100, origAddr), sockaddr_to_human(client, 100, clientAddr), asctime (timeinfo), data->orig.conn.status);

            return error_handler(data->orig.conn.status, key);
        }

        debug(etiqueta, 0, "Connection failed. Checking other IPs", key->fd);
        data->orig.conn.status = errno_to_socks(error);

        if(data->origin_resolution_current != NULL && data->origin_resolution_current->ai_next != NULL){                   //// Check if next IP exists


            debug(etiqueta, 0, "Checking next IP", key->fd);
            struct addrinfo * current = data->origin_resolution_current = data->origin_resolution_current->ai_next;

            //// IPv4
            if(current->ai_family == AF_INET)
                memcpy((struct sockaddr_in *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in));

            //// IPv6
            if(current->ai_family == AF_INET6)
                memcpy((struct sockaddr_in6 *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in6));

            connection(key);

            return REQUEST_CONNECTING;//vuelve a probar la siguiente ip

        } else{
            debug(etiqueta, 0, "No more IPs -> REQUEST_WRITE to reply error to client", key->fd);
            data->orig.conn.status=errno_to_socks(error);
            printf("%s\t to: %s \t from: %s \t %s \t status: %d\n", users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, 100, origAddr), sockaddr_to_human(client, 100, clientAddr), asctime (timeinfo), data->orig.conn.status);

            if(data->client.request.addr_family == socks_req_addrtype_domain)
                freeaddrinfo(data->origin_resolution);
            return error_handler(data->orig.conn.status, key);
        }

    }




    int request_marshall_result = request_marshall(data->orig.conn.status, &data->write_buffer);
    if(-1 == request_marshall_result){
        debug(etiqueta, request_marshall_result, "Error request marshall", key->fd);
        return ERROR;
        //abort();//TODO: ver este caso porque sin request marshall no se puede enviar
    }

    selector_status s=0;
    s|= selector_set_interest(key->s, data->orig.conn.client_fd, OP_WRITE);
    s|= selector_set_interest_key(key, OP_NOOP);

    debug(etiqueta, s, "Finished stage", key->fd);
    return SELECTOR_SUCCESS == s ? REQUEST_WRITE:ERROR;
}

//// CLOSE
void connecting_close(const unsigned state, struct selector_key *key){
    char * etiqueta = "CONNECTING CLOSE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct request_st *d = &ATTACHMENT(key)->client.request;
    if(d->parser != NULL) {
        // TODO ver estos free porque rompen las cosas. Como que hacen el free antes de lo debido
        request_parser_close(d->parser);
        free(d->parser);
    }
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

extern size_t metrics_historic_connections_attempts;
enum socks_reply_status connection(struct selector_key *key){

    char * etiqueta = "CONNECTION";
    debug(etiqueta, 0, "Starting stage", key->fd);

    struct socks5 * data = ATTACHMENT(key);


    debug(etiqueta, 0, "Connecting socket to origin", key->fd);
    int *fd= &data->origin_fd;

    //// Add connection attempt to metrics
    metrics_historic_connections_attempts += 1;

    int connectResult = connect(*fd, (const struct sockaddr*)&ATTACHMENT(key)->origin_addr, ATTACHMENT(key)->origin_addr_len);

    if(connectResult != 0 && errno != EINPROGRESS){
        debug(etiqueta, connectResult, "Connection for origin socket failed", key->fd);
        data->client.request.status = errno_to_socks(errno);
        data->orig.conn.status=errno_to_socks(errno);
        error_handler(data->orig.conn.status, key);
        return data->orig.conn.status;
        /*debug(etiqueta, connectResult, "Connection for origin socket failed", key->fd);
        data->client.request.status = errno_to_socks(errno);
        goto fail;*/
    }

    if(connectResult != 0){     //// EINPROGRESS
        selector_status st= selector_set_interest_key(key, OP_NOOP);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler(data->orig.conn.status, key);
            return data->orig.conn.status;
        }

        debug(etiqueta, connectResult, "Me suscribo a escritura para esperar que se complete la conexión", key->fd);
        st= selector_register(key->s, *fd, &socks5_handler,OP_WRITE, key->data);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler(data->orig.conn.status, key);
            close((*fd));
            *fd=-1;
            return data->orig.conn.status;
        }
        ATTACHMENT(key)->references += 1;           // TODO ?
    }
    else{     //// Connected with no EINPROGRESS
    //TODO: ver este caso
        ATTACHMENT(key)->references += 1;           // TODO ?
        selector_status st= selector_set_interest_key(key, OP_READ);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler(data->orig.conn.status, key);
            return data->orig.conn.status;
        }

        debug(etiqueta, connectResult, "Me suscribo a escritura para esperar que se complete la conexión", key->fd);
        st= selector_register(key->s, *fd, &socks5_handler,OP_READ, key->data);
        if(SELECTOR_SUCCESS != st){
            debug(etiqueta, st, "Error setting interest", key->fd);
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler(data->orig.conn.status, key);
            return data->orig.conn.status;
        }
    }


    debug(etiqueta, 0, "Finished stage", key->fd);
    return status_succeeded;

}
