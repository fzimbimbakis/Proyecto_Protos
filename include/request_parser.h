#ifndef PROYECTO_PROTOS_REQUEST_PARSER_H
#define PROYECTO_PROTOS_REQUEST_PARSER_H

#include <stdint.h>
#include <stdlib.h>

#define IPV4_LEN 4
#define IPV6_LEN 16

/**
 * Posibles estados del request parser
 */
enum request_state {
    request_version,
    request_cmd,
    request_rsv,
    request_atyp,
    request_dest_addr,
    request_dest_addr_fqdn,
    request_dest_port,

    // done section
    request_done,

    //error section
    request_error,
    request_error_unsupported_cmd,
    request_error_unsupported_type,
    request_error_unsupported_version,
};

/**
 * Estructura con información del request parser
 */
typedef struct request_parser {
    struct request *request;

    enum request_state state;

    //// Bytes a leer del campo actual
    uint8_t remaining;

    //// Bytes leidos
    uint8_t read;

} request_parser;

/**
 * Inicializa los campos del parser
 * @param parser
 */
void request_parser_init(struct request_parser *parser);

/**
 * Libera los recursos del parser que no se van a utilizar más
 * @param parser
 */
void request_parser_close(struct request_parser *parser);

/**
 * Función principal de procesamiento de entrada del parser
 * @param parser
 * @param byte
 * @return estado que sigue
 */
enum request_state request_parser_feed(struct request_parser *parser, uint8_t byte);

#endif //PROYECTO_PROTOS_REQUEST_PARSER_H
