#include "../../include/dissec_parser.h"
#include <ctype.h>
#include <stdlib.h>
#include <printf.h>
#include "../../include/socks5nio.h"
#include "../../include/debug.h"
#include <sys/socket.h>
#include <netutils.h>

#define BUFFER_SIZE 5
extern struct users users[MAX_USERS];

void dissec_parser_init(struct dissec_parser *p) {
    p->current_index = 0;
    p->current = user_word_search;
    p->last = false;
}

enum dissec_parser_state user_word_search_handler(struct dissec_parser *p, uint8_t b);

enum dissec_parser_state pass_word_search_handler(struct dissec_parser *p, uint8_t b);

enum dissec_parser_state user_read_handler(struct dissec_parser *pParser, uint8_t b);

enum dissec_parser_state pass_read_handler(struct dissec_parser *pParser, uint8_t b);

enum dissec_parser_state dissec_parser_feed(struct dissec_parser *p, uint8_t b) {
    enum dissec_parser_state ret;
    switch (p->current) {
        case user_word_search: {
            ret = user_word_search_handler(p, b);
            break;
        }
        case user_read: {
            ret = user_read_handler(p, b);
            break;
        }
        case pass_word_search: {
            ret = pass_word_search_handler(p, b);
            break;
        }
        case pass_read: {
            ret = pass_read_handler(p, b);
            break;
        }
    }
    return ret;
}

void dissec_consume(uint8_t *buffer, size_t size, struct dissec_parser *parser) {
    for (size_t i = 0; i < size; ++i) {
        parser->current = dissec_parser_feed(parser, buffer[i]);
    }
}

//// States functions

uint8_t *checkSize(uint8_t *buffer, size_t size) {
    if (size % BUFFER_SIZE == 0) {
        if (size == 0)
            buffer = malloc(size + BUFFER_SIZE);
        else
            buffer = realloc(buffer, size + BUFFER_SIZE);
    }
    return buffer;
}

enum dissec_parser_state user_read_reset(struct dissec_parser *p, uint8_t b) {
    p->current_index = 0;
    p->last = false;
    if (p->current_index != 0)
        free(p->username);
    if (user_reserved_pop3_word[p->current_index] == b)
        p->current_index++;
    return user_word_search;
}

enum dissec_parser_state user_read_handler(struct dissec_parser *p, uint8_t b) {
    char *etiqueta = "DISSEC USER READ";
    if (p->current_index >= 255)
        return user_read_reset(p, b);

    if (p->last && b != 0x0A) {
        return user_read_reset(p, b);
    }

    if (b == 0x0D) {
        p->last = true;
        return user_read;
    }
    if (b == 0x0A) {
        p->username = checkSize(p->username, p->current_index);
        p->username[p->current_index] = 0;
        debug(etiqueta, (int) p->current_index, (char *) p->username, 0);
        p->current_index = 0;
        p->last = false;
        return pass_word_search;
    }

    if (isascii(b)) {
        p->username = checkSize(p->username, p->current_index);
        p->username[p->current_index++] = b;
        return user_read;
    }

    return user_read_reset(p, b);
}

enum dissec_parser_state pass_read_reset(struct dissec_parser *p, uint8_t b) {
    if (p->current_index != 0)
        free(p->password);
    free(p->username);
    p->current_index = 0;
    p->last = false;
    if (user_reserved_pop3_word[p->current_index] == b)
        p->current_index++;
    return user_word_search;
}

enum dissec_parser_state pass_read_handler(struct dissec_parser *p, uint8_t b) {
    char *etiqueta = "DISSEC PASS READ";
    if (p->current_index >= 255)
        return pass_read_reset(p, b);
    if (p->last && b != 0x0A) {
        return pass_read_reset(p, b);
    }

    if (b == 0x0D) {
        p->last = true;
        return pass_read;
    }
    if (b == 0x0A) {
        p->password = checkSize(p->password, p->current_index);
        p->password[p->current_index] = 0;
        char *orig = malloc(100);             // TODO size?
        char *client = malloc(100);
        printf("Username: %s Password: %s\t Active user: %s Client address: %s Origin address: %s\n",
               p->username, p->password,
               users[*p->userIndex].name,
               sockaddr_to_human(client, 100, (struct sockaddr *) p->client),
               sockaddr_to_human(orig, 100, (struct sockaddr *) p->origin));
        free(orig);
        free(client);
        debug(etiqueta, (int) p->current_index, (char *) p->password, 0);
        return pass_read_reset(p, b);
    }

    if (isascii(b)) {
        p->password = checkSize(p->password, p->current_index);
        p->password[p->current_index++] = b;
        return pass_read;
    }

    return user_read_reset(p, b);
}

enum dissec_parser_state pass_word_search_handler(struct dissec_parser *p, uint8_t b) {
    if (pass_reserved_pop3_word[p->current_index++] == b) {   //// OK
        if (p->current_index == PASS_L) {
            p->current_index = 0;
            return pass_read;
        } else {
            return pass_word_search;
        }
    } else {                                                 //// Re start
        free(p->username);
        p->current_index = 0;
        p->last = false;
        if (user_reserved_pop3_word[p->current_index] == b)
            p->current_index++;
        return user_word_search;
    }
}

enum dissec_parser_state user_word_search_handler(struct dissec_parser *p, uint8_t b) {
    if (user_reserved_pop3_word[p->current_index++] == b) {   //// OK
        if (p->current_index == USER_L) {
            p->current_index = 0;
            return user_read;
        } else {
            return user_word_search;
        }
    } else {                                                 //// Re start
        p->current_index = 0;
        if (user_reserved_pop3_word[p->current_index] == b)
            p->current_index++;
        return user_word_search;
    }
}

