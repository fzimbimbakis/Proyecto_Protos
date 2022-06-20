#include <string.h>
#include "../../include/resolv.h"

void *request_resolv_blocking(void *ptr) {
    char *etiqueta = "REQUEST RESOLV BLOCKING";
    struct selector_key *key = (struct selector_key *) ptr;
    struct socks5 *data = ATTACHMENT(key);
    debug(etiqueta, 0, "Starting stage", 0);
    struct request *request_data = data->client.request.parser->request;
    struct sockaddr_fqdn fqdn = data->client.request.parser->request->fqdn;

    /**
     * The pthread_detach() function is used to indicate to the implementation
     * that storage for the thread thread can be reclaimed when the thread terminates.
     */
    pthread_detach(pthread_self());

    data->origin_resolution = 0;

    //// Hints for getaddrinfo

    struct addrinfo hints = {
            .ai_family=AF_UNSPEC,
            .ai_socktype=SOCK_STREAM,
            .ai_flags= AI_PASSIVE,
            .ai_protocol=0,
            .ai_canonname=NULL,
            .ai_addr=NULL,
            .ai_next=NULL,
    };

    //// Set port
    char buff[7];
    uint16_t port = ntohs(request_data->dest_port);
    debug(etiqueta, port, "Port", 0);
    snprintf(buff, sizeof(buff), "%d", port);

    //// Just debugging
    char *buffer = malloc((fqdn.size + 1));
    for (int i = 0; i < fqdn.size; ++i) {
        buffer[i] = fqdn.host[i];
    }
    buffer[fqdn.size] = 0;
    debug(etiqueta, fqdn.size, buffer, 0);
    ////

    debug(etiqueta, 0, "Starting getaddrinfo", 0);
    int getaddrinfo_result = getaddrinfo(buffer, buff, &hints, &data->origin_resolution);
    free(buffer);
    buffer = NULL;
    if (getaddrinfo_result != 0) {
        debug(etiqueta, 0, "getaddrinfo error:", 0);
        debug(etiqueta, 0, (char *) gai_strerror(getaddrinfo_result), 0);
    }

    debug(etiqueta, getaddrinfo_result, "Notify block over", 0);
    selector_notify_block(key->s, key->fd);

    free(key);
    key = NULL;
    debug(etiqueta, 0, "Finished stage", 0);
    return 0;
}

unsigned request_resolv_done(struct selector_key *key) {
    char *etiqueta = "REQUEST RESOLV DONE";
    debug(etiqueta, 0, "Starting stage", key->fd);
    struct socks5 *data = ATTACHMENT(key);

    if (data->origin_resolution == NULL) {
        debug(etiqueta, 0, "Error FQDN resolution came back empty -> REQUEST_WRITE to reply error to client", key->fd);
        data->orig.conn.status = status_general_socks_server_failure;
        data->client.request.status=status_general_socks_server_failure;
        return error_handler(data->client.request.status, key);
    }

    //// Seteo current
    struct addrinfo *current = data->origin_resolution_current = data->origin_resolution;

    //// IPv4
    if (current->ai_family == AF_INET) {
        data->origin_domain = AF_INET;
        data->origin_addr_len = sizeof(struct sockaddr);
        memcpy((struct sockaddr *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr));
    }

    //// IPv6
    if (current->ai_family == AF_INET6) {
        data->origin_domain = AF_INET6;
        data->origin_addr_len = sizeof(struct sockaddr);
        memcpy((struct sockaddr *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr));
    }
    debug(etiqueta, 0, "", key->fd);
    debug(etiqueta, 0, "Finishing stage", key->fd);
    return REQUEST_CONNECTING;
}
