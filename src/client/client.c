#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include "../../include/clientArgs.h"
#include "../../include/debug.h"
#include "../../include/clientUtils.h"
#include <errno.h>


#define MAX 80
#define PORT 8080
#define SA struct sockaddr


int main(const int argc, const char **argv)
{

    struct m16args * args = malloc(sizeof(struct m16args));
    if(args == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(args, 0, sizeof(*args));
    args->user= malloc(sizeof(struct user));
    if(args->user == NULL) {
        exit(EXIT_FAILURE);
    }





    int parse_args_result = parse_args(argc, (char *const *)argv, args);

    if(parse_args_result == -1){
        free(args);
        exit(1);
    }

    int debug_option = args->debug;
    debug_init(debug_option);
    int sockfd;
    switch (args->mng_family) {
        case AF_UNSPEC:{
            sockfd= socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
            if (sockfd < 0) {
                debug("M16 CLIENT FATAL", sockfd, "ipv4 socket() failed",0);
            }
            if (connect(sockfd,(const struct sockaddr*) &args->mng_addr_info, sizeof(struct sockaddr)) < 0) {
                debug("M16 CLIENT FATAL", 0, "ipv4 connect() failed",0);
                debug("M16 CLIENT FATAL", 0, strerror(errno),0);
                goto end;
            }
            break;
        }
        case AF_INET:{
            sockfd= socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
            if (sockfd < 0) {
                debug("M16 CLIENT FATAL", sockfd, "ipv4 socket() failed",0);
            }
            if (connect(sockfd,(const struct sockaddr*) &args->mng_addr_info, sizeof(struct sockaddr)) < 0) {
                debug("M16 CLIENT FATAL", 0, "ipv4 connect() failed",0);
                debug("M16 CLIENT FATAL", 0, strerror(errno),0);
                goto end;
            }
            break;
        }
        case AF_INET6:{
            sockfd= socket(AF_INET6, SOCK_STREAM,IPPROTO_TCP);
            if (sockfd < 0) {
                debug("M16 CLIENT FATAL", sockfd, "ipv6 socket() failed",0);
            }
            if (connect(sockfd,(const struct sockaddr*) &args->mng_addr_info6, sizeof(struct sockaddr_in6)) < 0) {
                debug("M16 CLIENT FATAL", 0, "ipv6 connect() failed",0);
                debug("M16 CLIENT FATAL", 0, strerror(errno),0);
                goto end;
            }
            break;
        }
    }


    int ret=handshake(sockfd, args->user);
    if(ret < 0){
        printf("Error in sending handshake\n");
        goto end;
    }



    uint8_t response=handshake_response(sockfd);
    if(response==0xFF) {
        printf("Error in handshake response\n");
        goto end;
    }

    if(response==0x02) {
        if(send_credentials(sockfd, args->user)< 0) {
            printf("Error sending credentials\n");
            goto end;
        }
        //printf("send credentials");
    }

    response=credentials_response(sockfd);
    if(response!=0x00) {
        printf("Error on user authentication.\n");
        goto end;
    }


    int req_index;
    ret= send_request(sockfd, &req_index);
    if(ret < 0){
        printf("Send request error\n");
        goto end;
    }

    if(request_response(sockfd, req_index) <0)
        goto end;


    end:
    if(sockfd >= 0)
        close(sockfd);
    free(args->user);
    free(args);

    return 0;

}






