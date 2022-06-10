#include <stdlib.h>
#include "../include/myParser.h"

void parser_init(struct parser *p)
{
    p->current = p->states[0]->state;
    p->index = 0;
    p->error = 0;
}

enum parser_state parser_feed(struct parser *p, uint8_t b)
{
    static char * etiqueta = "MY PARSER FEED";
    parser_substate * substate = (p->states[p->index]);

    switch (p->current){
        case single_read:
            *(substate->result) = b;
            if(substate->check_function == NULL){
                debug(etiqueta, b, "Single read OK", 0);
                p->index = p->index + 1;
                substate = (p->states[p->index]);
                if(p->index >= p->size)
                    p->current = done_read;
                else
                    p->current = substate->state;
                break;
            }


            if (substate->check_function(substate->result, 1, &p->error))   //// Check result
            {
                debug(etiqueta, b, "Single read OK", 0);
                p->index = p->index + 1;
                substate = (p->states[p->index]);
                if(p->index >= p->size)
                    p->current = done_read;
                else
                    p->current = substate->state;
            }
            else
            {
                debug(etiqueta, b, "Single read check returned false", 0);
                substate->state = error_read;
            }
            break;

        case read_N:


            p->index = p->index + 1;
            if(p->index >= p->size) {
                p->current = error_read;
                break;
            }

            substate = (p->states[p->index]);
            substate->remaining = b;
            substate->size = b;
            p->current = substate->state;

            debug(etiqueta, b, "Received N for long_read", 0);
            if (substate->remaining <= 0)      //// Si es cero salteo la fase de long_read
            {
                debug(etiqueta, b, "N is 0", 0);
                substate->result = NULL;
                p->index = p->index + 1;
                substate = (p->states[p->index]);
                if(p->index >= p->size) {
                    p->current = done_read;
                    break;
                } else p->current = substate->state;
            } else{
                substate->result = malloc(sizeof(uint8_t) * substate->size);
            }


            break;

        case long_read:

            debug(etiqueta, b, "Read", substate->remaining);

            (substate->result)[(substate->size - substate->remaining)] = b;

            (substate->remaining)--;

            if (substate->remaining <= 0)
            {
                debug(etiqueta, 0, "Finished long read", substate->remaining);
                if (substate->check_function != NULL)
                {
                    debug(etiqueta, 0, "Running check on long read", 0);
                    if(!(substate->check_function( substate->result, substate->size, &p->error))) {
                        debug(etiqueta, 0, "Error on check", 0);
                        p->current = error_read;
                        break;
                    }
                }
                p->index = p->index + 1;
                if(p->index >= p->size) {
                    p->current = done_read;
                    break;
                } else {
                    substate = (p->states[p->index]);
                    p->current = substate->state;
                }

            }
            break;
        case done_read:
                debug(etiqueta, 0, "DONE", 0);
            break;
        case error_read:
                debug(etiqueta, (p->error), "ERROR", 0);
            break;
        default:
            abort();
            break;
    }
    return p->current;
}

enum parser_state consume(buffer *b, struct parser *p, bool *error)
{
    enum parser_state st = p->states[0]->state;
    bool finished = false;
    while (buffer_can_read(b) && !finished)
    {
        uint8_t byte = buffer_read(b);
        st = parser_feed(p, byte);
        if (is_done(st, error))
        {
            finished = true;
        }
    }
    return st;
}

uint8_t is_done(const enum parser_state state, bool *error)
{
    bool ret = false;
    switch (state)
    {
        case error_read:
            if (error != 0)
            {
                *error = true;
            }
            ret = true;
            break;
        case done_read:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
    return ret;
}
