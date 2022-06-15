#ifndef PROYECTO_PROTOS_MNG_NIO_H
#define PROYECTO_PROTOS_MNG_NIO_H

#include "states.h"
#include <sys/socket.h>
#include "stm.h"
#include "selector.h"

/**handler del socket pasivo que atiende conexiones mng */

void
mng_passive_accept(struct selector_key *key);

/* declaraciÃ³n forward de los handlers de selecciÃ³n de una conexiÃ³n
 * establecida entre un cliente y el proxy.
 */

void mng_read(struct selector_key *key);

void mng_write(struct selector_key *key);

void mng_block(struct selector_key *key);

void mng_close(struct selector_key *key);


/** maquina de estados general */
enum mng_state {
    /**
     * recibe el mensaje `hello` del cliente, y lo procesa
     *
     * Intereses:
     *     - OP_READ sobre client_fd
     *
     * Transiciones:
     *   - HELLO_READ  mientras el mensaje no estÃ© completo
     *   - HELLO_WRITE cuando estÃ¡ completo
     *   - ERROR       ante cualquier error (IO/parseo)
     */
    HELLO_READ,

    /**
     * envÃ­a la respuesta del `hello' al cliente.
     *
     * Intereses:
     *     - OP_WRITE sobre client_fd
     *
     * Transiciones:
     *   - HELLO_WRITE  mientras queden bytes por enviar
     *   - REQUEST_READ cuando se enviaron todos los bytes
     *   - ERROR        ante cualquier error (IO/parseo)
     */
    HELLO_WRITE,

    //AUTH,
    USERPASS_READ,
    USERPASS_WRITE,

    // Requests
    MNG_REQUEST_READ_INDEX,
    MNG_REQUEST_READ,
    MNG_REQUEST_WRITE,

    // estados terminales
    MNG_DONE,
    MNG_ERROR,
};

/** obtiene el struct (mng *) desde la llave de selecciÃ³n  */
#endif //PROYECTO_PROTOS_MNG_NIO_H
