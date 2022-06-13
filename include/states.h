#ifndef PROYECTO_PROTOS_STATES_H
#define PROYECTO_PROTOS_STATES_H
#include "buffer.h"
#include "selector.h"
#include "myParser.h"
#include <sys/socket.h>

//// Definición de variables para cada estado


/** Used by the HELLO_READ and HELLO_WRITE states */
typedef struct hello_st
{
    /** Buffers used for IO */
    buffer *rb, *wb;
    /** Pointer to hello parser */
    struct hello_parser * parser;
    /** Selected auth method */
    uint8_t method;
}hello_st;

/** Used by the USERPASS_READ and USERPASS_WRITE states */
typedef struct userpass_st
{
/** Buffers used for IO */
buffer *rb, *wb;
/** Pointer to user-pass parser */
struct parser * parser;
/** Selected user */
//uint8_t * user;
/** Selected password */
//uint8_t * password;
//uint8_t auth_result;
} userpass_st;

/** Used by the REQUEST_READ, REQUEST_WRITE and REQUEST_RESOLV state */

enum socks_reply_status
{
    status_succeeded = 0x00,
    status_general_socks_server_failure = 0x01,
    status_connection_not_allowed_by_ruleset = 0x02,
    status_network_unreachable = 0x03,
    status_host_unreachable = 0x04,
    status_connection_refused = 0x05,
    status_ttl_expired = 0x06,
    status_command_not_supported = 0x07,
    status_address_type_not_supported = 0x08,
};

typedef struct request_st
{
    buffer *rb, *wb;


//    struct request request;
    struct request_parser* parser;


    enum socks_reply_status status;

    struct sockaddr_storage *origin_addr;
    socklen_t *origin_addr_len;
    int *origin_domain;

    const int *client_fd;
    int *origin_fd;
}request_st;

/** Used by REQUEST_CONNECTING */
typedef struct connecting{
    buffer *wb;
    int client_fd;
    int origin_fd;
    enum socks_reply_status status;
}connecting;

/** Used by the COPY state */
typedef struct copy_st
{
    /** File descriptor */
    int fd;
    /** Reading buffer */
    buffer * rb;
    /** Writing buffer */
    buffer * wb;
    /** Interest of the copy */
    enum fd_interest interest;
    /** Pointer to the structure of the opposing copy state*/
    struct copy_st * other_copy;

}copy_st;
#endif //PROYECTO_PROTOS_STATES_H
