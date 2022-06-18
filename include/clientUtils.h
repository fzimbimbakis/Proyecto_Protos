#ifndef PROYECTO_PROTOS_CLIENTUTILS_H
#define PROYECTO_PROTOS_CLIENTUTILS_H

#include <stdint.h>
#include "clientArgs.h"


int handshake(int sockfd, struct user* user);

uint8_t handshake_response(int sockfd);

int send_credentials(int sockfd, struct user* user);

uint8_t credentials_response(int sockfd);

int add_user(uint8_t* buffer);

int delete_user(uint8_t* buffer);

int disable_enable(uint8_t* buffer, char* print_string);

int send_request(int sockfd, int * index);

void supported_indexes(char* buffer);

void list_users(char* buffer);

uint32_t cast_uint32(char* buffer);

int request_response(int sockfd, int req_index);

#endif //PROYECTO_PROTOS_CLIENTUTILS_H
