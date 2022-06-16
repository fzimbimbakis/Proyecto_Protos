#include <string.h>
#include <tests.h>
#include "../include/mng_nio.h"
#include "../include/hello.h"
#include "../include/authentication.h"
#include "../include/socks5nio.h"
#include "../include/mng_request.h"

const struct fd_handler mng_handler = {
        .handle_read   = mng_read,
        .handle_write  = mng_write,
        .handle_close  = mng_close,
        .handle_block  = mng_block,
};

/** definición de handlers para cada estado */
static const struct state_definition client_mng[] = {
        {
                .state = HELLO_READ,
                .on_arrival = hello_read_init,
                .on_departure = hello_read_close,
                .on_read_ready = hello_read,
        },
        {.state = HELLO_WRITE,
                .on_arrival = hello_write_init,
                .on_departure = hello_write_close,
                .on_write_ready = hello_write

        },
        {
                .state = USERPASS_READ,
                .on_arrival = auth_read_init,
                .on_departure = auth_read_close,
                .on_read_ready = auth_read,
        },
        {
                .state = USERPASS_WRITE,
                .on_arrival = auth_write_init,
                .on_write_ready = auth_write,
                .on_departure = auth_write_close,
        },
        {
                .state = MNG_REQUEST_READ_INDEX,
                .on_arrival = mng_request_index_init,
                .on_departure = mng_request_index_close,
                .on_read_ready = mng_request_index_read,
        },
        {
                .state = MNG_REQUEST_READ,
                .on_arrival = mng_request_init,
                .on_departure = mng_request_close,
                .on_read_ready = mng_request_read,
        },
        {
                .state = MNG_REQUEST_WRITE,
                //               .on_arrival = request_init,
                //               .on_departure = request_close,
                .on_write_ready = mng_request_write,
        },
        {
                .state = DONE,
                // For now, no need to define any handlers, all in sockv5_done
        },
        {
                .state = ERROR,
                // No now, no need to define any handlers, all in sockv5_done
        }

};

/**
 * Pool de struct mng, para ser reusados.
 *
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención.
 */

static const unsigned max_pool = 50;
static unsigned pool_size = 0;
static struct socks5 *pool = 0;

static const struct state_definition *mng_describe_states();

/** crea un nuevo struct mng */
static struct socks5 *mng_new(int client_fd) {
    char *etiqueta = "MNG NEW";
    struct socks5 *ret;

    if (pool == NULL) {
        ret = malloc(sizeof(*ret));
    } else {
        ret = pool;
        pool = pool->next;
        ret->next = 0;
    }

    if (ret == NULL) {
        goto finally;
    }
    ret->isSocks = false;
    memset(ret, 0x00, sizeof(*ret));

    ret->client_fd = client_fd;
    ret->client_addr_len = sizeof(ret->client_addr);
    ret->origin_fd = -1;

    //// INITIAL STATE
    debug(etiqueta, HELLO_READ, "Setting first state", client_fd);
    ret->stm.initial = HELLO_READ;
    ret->stm.max_state = ERROR;
    ret->stm.current = &client_mng[0];
    ret->stm.states = client_mng;
    stm_init(&ret->stm);

    // TODO El tamaño del buffer podría depender de la etapa
    debug(etiqueta, 0, "Init buffers", client_fd);
    buffer_init(&ret->read_buffer, N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    ret->references = 1;
    return ret;
    finally:
    debug(etiqueta, 0, "Error creating mng struct", client_fd);
    return ret;
}

/** Intenta aceptar la nueva conexión entrante*/
void
mng_passive_accept(struct selector_key *key) {
    char *etiqueta = "MNG PASSIVE ACCEPT";
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct socks5 *state = NULL;

    debug(etiqueta, 0, "Starting pasive accept", key->fd);
    const int client = accept(key->fd, (struct sockaddr *) &client_addr, &client_addr_len);
    debug(etiqueta, client, "Accept connection", key->fd);
    if (client == -1) {
        goto fail;
    }
    if (selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    debug(etiqueta, 0, "Creating mng struct", key->fd);
    state = mng_new(client);
    if (state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberÃ³ alguna conexiÃ³n.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;
    debug(etiqueta, 0, "Registering client with read interest to selector", key->fd);
    selector_status ss = selector_register(key->s, client, &mng_handler,
                                           OP_READ, state);
    if (SELECTOR_SUCCESS != ss) {
        debug(etiqueta, ss, "Error registering", key->fd);
        goto fail;
    }
    return;
    fail:
    debug(etiqueta, 0, "Fail", key->fd);
    if (client != -1) {
        close(client);
    }
    socks5_destroy(state);
}


///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexiÃ³n pasiva.
// son los que emiten los eventos a la maquina de estados.
void
mng_done(struct selector_key *key);

void
mng_read(struct selector_key *key) {
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum mng_state st = stm_handler_read(stm, key);

    if (ERROR == st || DONE == st) {
        mng_done(key);
    }
}

void
mng_write(struct selector_key *key) {
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum mng_state st = stm_handler_write(stm, key);

    if (ERROR == st || DONE == st) {
        mng_done(key);
    }
}

void
mng_block(struct selector_key *key) {
    struct state_machine *stm = &ATTACHMENT(key)->stm;
    const enum mng_state st = stm_handler_block(stm, key);

    if (ERROR == st || DONE == st) {
        mng_done(key);
    }
}

void
mng_close(struct selector_key *key) {
    socks5_destroy(ATTACHMENT(key));
}

void
mng_done(struct selector_key *key) {
    const int fds = ATTACHMENT(key)->client_fd, ;
    if (fds != -1) {
        if (SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds)) {
            abort();
        }
        close(fds);
    }
}

