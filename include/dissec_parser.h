#ifndef PROYECTO_PROTOS_DISSEC_PARSER_H
#define PROYECTO_PROTOS_DISSEC_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 *   Password dissector parser (bastante básico)
 *
 *   Es un parser que busca la siguiente secuencia: "user username\r\n+OK\r\npass password\r\n" (Also considers \r\n)
 *
 *   No toma en cuenta que se pisen posibles entradas.
 *   Por ejemplo, "user pepito user juan pass contraseña" este caso se toma como user "pepito USER juan".
 *
 *   El maximo de largo de un user o una pass es 255.
 */


#define USER_L 5
#define PASS_L 10
static uint8_t user_reserved_pop3_word[] = {0x75, 0x73, 0x65, 0x72, 0x20};
static uint8_t pass_reserved_pop3_word[] = {0x2B, 0x4F, 0x4B, 0x0D, 0x0A, 0x70, 0x61, 0x73, 0x73, 0x20};

enum dissec_parser_state {
    user_word_search,
    user_read,
    pass_word_search,
    pass_read
};

typedef struct dissec_parser {

    enum dissec_parser_state current;

    size_t current_index;

    bool last;
    uint8_t *username;
    uint8_t *password;

    struct sockaddr_storage *client;
    struct sockaddr_storage *origin;
    int *userIndex;

} dissec_parser;

/**
 * Initializes password dissectors parser
 * @param p
 */
void dissec_parser_init(struct dissec_parser *p);

/**
 * Feed parser. Used in dissec_consume.
 * @param p
 * @param b
 * @return
 */
enum dissec_parser_state dissec_parser_feed(struct dissec_parser *p, uint8_t b);

/**
 * Feeds the buffer to the parser. Buffer is not
 * @param buffer
 * @param size
 * @param parser
 */
void dissec_consume(uint8_t *buffer, size_t size, struct dissec_parser *parser);

#endif //PROYECTO_PROTOS_DISSEC_PARSER_H
