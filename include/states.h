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

    int addr_family;

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

//// MNG REQUEST

enum mng_reply_status
{
    mng_status_succeeded = 0x00,
    mng_status_server_error = 0x01,
    mng_status_index_not_supported = 0x02,
    mng_status_max_users_reached = 0x03
};

enum mng_request_indexes
{
    mng_request_index_supported_indexes = 0x00,
    mng_request_index_list_users = 0x01,
    mng_request_index_historic_connections = 0x02,
    mng_request_index_concurrent_connections = 0x03,
    mng_request_index_max_concurrent_connections = 0x04,
    mng_request_index_historic_bytes_transferred = 0x05,
    mng_request_index_historic_auth_attempts = 0x06,
    mng_request_index_historic_connections_attempts = 0x07,
    mng_request_index_average_bytes_per_read = 0x08,
    mng_request_index_average_bytes_per_write = 0x09,
    mng_request_index_add_user = 0x0A,
    mng_request_index_delete_user = 0x0B,
    mng_request_index_disable_auth = 0x0C,
    mng_request_index_disable_password_disectors = 0x0D,
    mng_request_index_shutdown_server = 0xFF,
};

typedef struct mng_request_st
{
    buffer *rb, *wb;

    struct parser * parser;

    enum mng_reply_status status;

    enum mng_request_indexes index;

    const int *client_fd;

}mng_request_st;

#endif //PROYECTO_PROTOS_STATES_H
