#ifndef PROYECTO_PROTOS_MYPARSER_H
#define PROYECTO_PROTOS_MYPARSER_H
#include "buffer.h"
#include "debug.h"
/**
 * Parser states
 */
enum parser_state
{
    single_read,
    read_N,
    long_read,
    done_read,
    error_read
};
typedef int (*check)(uint8_t const * ptr, uint8_t size, uint8_t *error);

typedef struct parser_substate{
    enum parser_state state;
    uint8_t * result;
    check check_function;
    uint8_t size;
    uint8_t remaining;
}parser_substate;

typedef struct parser{
    uint8_t index;
    uint8_t size;
    enum parser_state current;

    struct parser_substate ** states;      //// Hay que hacer malloc y después free

    uint8_t error;

}parser;


void parser_init(struct parser *p);

enum parser_state parser_feed(struct parser *p, uint8_t b);

enum parser_state consume(buffer *b, struct parser *p, bool *error);

uint8_t is_done(const enum parser_state state, bool *error);

#endif //PROYECTO_PROTOS_MYPARSER_H
