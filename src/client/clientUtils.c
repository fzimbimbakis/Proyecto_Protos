#include "../../include/clientUtils.h"
#include "../../include/clientArgs.h"
#include "../../include/states.h"
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <stdint.h>


int handshake(int sockfd, struct user* user){
    uint8_t buffer[4];
    int bytes_to_send;
    buffer[0]=0x01; //VERSION
    if(user->credentials){
        buffer[1]=0x02; //NMETODS
        buffer[2]=0x00; //NO AUTHENTICATION REQUIRED
        buffer[3]=0x02; //USERNAME/PASSWORD
        bytes_to_send=4;
    }else{
        buffer[1]=0x01; //NMETODS
        buffer[2]=0x00; //NO AUTHENTICATION REQUIRED
        bytes_to_send=3;
    }

    return send(sockfd, buffer, bytes_to_send,0);
}


uint8_t handshake_response(int sockfd){
    uint8_t buff[2];
    int rec=recv(sockfd, buff, 2, 0);
    if(rec != 2) {
        printf("Error handshake_response\n");
        return 0xFF;
    }
    //printf("%x", buff[1]);
    return buff[1];
}


int send_credentials(int sockfd, struct user* user){
    uint8_t buffer[510];
    int i = 0;
    buffer[i++]=0x01;//subnegotiation version

    int userlen=strlen(user->username);
    buffer[i++]=userlen;
    strcpy((char*)buffer + i, user->username);
    int passlen= strlen(user->password);
    buffer[i++ + userlen]=passlen;
    strcpy((char*)buffer +  i + userlen, user->password);

    return send(sockfd, buffer, userlen + passlen + i,0);
}

uint8_t credentials_response(int sockfd){
    uint8_t buffer[2];
    int rec=recv(sockfd, buffer, 2, 0);
    if(rec != 2) {
        printf("Error credentials_response\n");
        return 0xFF;
    }
    return buffer[1];
}


int add_user(uint8_t* buffer){
    uint8_t username[255];
    uint8_t password[255];


    printf("Enter new username: ");
    scanf("%s",username);
    int nusername= strlen((char*)username);
    printf("Enter new password: ");
    scanf("%s", password);
    int npassword= strlen((char*)password);
    buffer[1]=nusername;
    strcpy((char*)buffer+2,(char*) username);
    buffer[nusername+2]=npassword;
    strcpy((char*)buffer+ nusername+ 3,(char*) password);

    return nusername+npassword+3;
}

int delete_user(uint8_t* buffer){
    uint8_t username[255];

    printf("Enter username to delete: ");
    scanf("%s",username );
    int nusername= strlen((char*)username);
    buffer[1]=nusername;
    strcpy((char*)buffer+2,(char*) username);

    return nusername+2;
}

int disable_enable(uint8_t* buffer, char* print_string){
    uint8_t on_off[50];

    printf("%s", print_string);
    scanf("%s",(char*)on_off );

    int i=0;
    while(on_off[i] != 0){
        on_off[i]= tolower((char)on_off[i]);
        i++;
    }

    if (strcmp((char*)on_off, "on")==0){
        buffer[1]=0x00;
    }else if(strcmp((char*)on_off, "off")==0){
        buffer[1]=0x01;
    }else {
        printf("Invalid option\n");
        return -1;
    }
    return 2;
}

int send_request(int sockfd, int * index){
    uint8_t buffer[512];

    int bytes_to_send=1;

    printf("Enter index request: ");

    scanf("%d",index );

    switch (*index) {
        case 0: //A list of all the REQUEST INDEX that the server supports.
            buffer[0]=0x00;
            break;
        case 1://List of active users on the server.
            buffer[0]=0x01;
            break;
        case 2://Amount of historic connections to the server.
            buffer[0]=0x02;
            break;
        case 3://Amount of actual concurrent connections to the server.
            buffer[0]=0x03;
            break;
        case 4://Max concurrent connections to the server.
            buffer[0]=0x04;
            break;
        case 5://Amount of historic byte transferred by the server.
            buffer[0]=0x05;
            break;
        case 6://Amount of historic authentication attempts to the server.
            buffer[0]=0x06;
            break;
        case 7://Amount of historic connections attempts to the server.
            buffer[0]=0x07;
            break;
        case 8://Average bytes per single server read.
            buffer[0]=0x08;
            break;
        case 9://Average bytes per single server write.
            buffer[0]=0x09;
            break;
        case 10://Add a new user.
            buffer[0]=0x0A;
            bytes_to_send= add_user(buffer);

            break;
        case 11://Delete a user.
            buffer[0]=0x0B;
            bytes_to_send= delete_user(buffer);
            break;
        case 12://Disable server authentication.
            buffer[0]=0x0C;
            bytes_to_send= disable_enable(buffer, "Turn on/off authentication: ");
            break;
        case 13://Disable password dissectors.
            buffer[0]=0x0D;
            bytes_to_send= disable_enable(buffer, "Turn on/off password dissectors: ");
            break;
        default:
            buffer[0]=0xFF;
            break;
    }

    if(bytes_to_send < 0)
        return -1;

    return send(sockfd, buffer, bytes_to_send,0);
}

void supported_indexes(char* buffer){
    uint8_t b=buffer[1];//length
    for(uint8_t i=0; i < b ; i++){
        printf("Index %d supported\n", buffer[2+i]);
    }
}


void list_users(char* buffer){
    uint8_t b=buffer[1];//length
    int index=2;
    char aux[256];
    uint8_t user_len;
    for(uint8_t i=0 ; i < b ; i++){
        user_len=buffer[index++];
        strncpy(aux, buffer + index, user_len);
        aux[user_len] = 0;
        printf("User: %s\n", aux);
        index+=user_len;
    }
}

uint32_t cast_uint32(char* buffer){
    uint8_t v4[4] = {buffer[1],buffer[2],buffer[3],buffer[4]};
    uint32_t *allOfIt;
    allOfIt = (uint32_t*)v4;
    return *allOfIt;
}

extern FILE * append_file;
int request_response(int sockfd, int req_index){
    uint8_t buffer[500];
    recv(sockfd, buffer, 500, 0);
    if(buffer[0]!=0x00){
        switch (buffer[0]) {
            case mng_status_index_not_supported:
                printf("Index not supported\n");
                break;
            case mng_status_server_error:
                printf("Server error\n");
                break;
            case mng_status_max_users_reached:
                printf("Max users reached\n");
                break;
        }
        return -1;
    }


    uint32_t stats;

    switch (req_index) {
        case 0: //A list of all the REQUEST INDEX that the server supports.
            supported_indexes((char*)buffer);
            break;
        case 1://List of active users on the server.
            printf("List of active users on the server\n\n");
            list_users((char*)buffer);
            break;
        case 2://Amount of historic connections to the server.
            stats=cast_uint32((char*) buffer);
            printf("Amount of historic connections to the server: %u\n", stats);
            break;
        case 3://Amount of actual concurrent connections to the server.
            stats=cast_uint32((char*) buffer);
            printf("Amount of actual concurrent connections to the server: %u\n", stats);
            break;
        case 4://Max concurrent connections to the server.
            stats=cast_uint32((char*) buffer);
            printf("Max concurrent connections to the server: %u\n", stats);
            break;
        case 5://Amount of historic byte transferred by the server.
            stats=cast_uint32((char*) buffer);
            printf("Amount of historic byte transferred by the server: %u\n", stats);
            break;
        case 6://Amount of historic authentication attempts to the server.
            stats=cast_uint32((char*) buffer);
            printf("Amount of historic authentication attempts to the server: %u\n", stats);
            break;
        case 7://Amount of historic connections attempts to the server.
            stats=cast_uint32((char*) buffer);
            printf("Amount of historic connections attempts to the server: %u\n", stats);
            break;
        case 8://Average bytes per single server read.
            stats=cast_uint32((char*) buffer);
            printf("Average bytes per single server read: %u\n", stats);
            break;
        case 9://Average bytes per single server write.
            stats= cast_uint32((char*) buffer);
            printf("Average bytes per single server write: %u\n", stats);
            break;
        case 10://Delete a user.
            printf("Added user successfully\n");
            break;
        case 11://Delete a user.
            printf("Deleted user successfully\n");
            break;
        case 12://Disable server authentication.
            printf("Disabled/enabled server authentication successfully\n");
            break;
        case 13://Disable password dissectors.
            printf("Disabled/enabled password dissectors successfully\n");
            break;
        default:
            return -1;

    }
    if(append_file != NULL)
        fprintf(append_file, "%u ", stats);
    return 0;


}
