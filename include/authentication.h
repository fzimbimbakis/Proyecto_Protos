#ifndef PROYECTO_PROTOS_AUTHENTICATION_H
#define PROYECTO_PROTOS_AUTHENTICATION_H

#include "selector.h"
#include "states.h"
/**
 * @section Authentication
 *
 * Read and parse client Username/Password request:
 *      +-----+------+----------+------+----------+
 *      | VER | ULEN |   UNAME  | PLEN |  PASSWD  |
 *      +-----+------+----------+------+----------+
 *      |  1  |   1  | 1 to 255 |   1  | 1 to 255 |
 *      +-----+------+----------+------+----------+
 *
 *      Leaves result on userpass_st (user and password) received in key.
 *      @note Both fields in userpass_st need to be freed later.
 *
 *      The VER field contains the current version of the subnegotiation,
 *      which is X’01’. The ULEN field contains the length of the UNAME field
 *      that follows. The UNAME field contains the username as known to the
 *      source operating system. The PLEN field contains the length of the
 *      PASSWD field that follows. The PASSWD field contains the password
 *      association with the given UNAME.
 *
 * Checks authentication and writes answer to client:
 *      +-----+--------+
 *      | VER | STATUS |
 *      +-----+--------+
 *      |  1  |    1   |
 *      +-----+--------+
 *
 *      A STATUS field of X’00’ indicates success. If the server returns a
 *      ‘failure’ (STATUS value other than X’00’) status, it MUST close the
 *      connection.
 */

/**
 *  Sub-negotiation with USERNAME/PASSWORD method version
 */
#define USERPASS_METHOD_VERSION 0x01

/**
 * auth_read_init
 * Initializes userpass_st variables
 * @param state
 * @param key
 */
void auth_read_init(unsigned state, struct selector_key *key);
/**
 * auth_read
 * Reads and parses client input
 * @param key
 */
unsigned auth_read(struct selector_key *key);
/**
 * auth_read_close
 * Close resources
 * @param state
 * @param key
 */
void auth_read_close(unsigned state, struct selector_key *key);

/**
 * auth_write_init
 * Initializes userpass_st variables
 * @param state
 * @param key
 */
void auth_write_init(unsigned state, struct selector_key *key);
/**
 * auth_write
 * Checks authentication and wirtes answer to client
 * @param key
 */
unsigned auth_write( struct selector_key *key);
/**
 * auth_write_close
 * Close resources
 * @param state
 * @param key
 */
void auth_write_close( unsigned state, struct selector_key *key);



int auth_process(struct userpass_st *d, struct selector_key * data);

#endif //PROYECTO_PROTOS_AUTHENTICATION_H
