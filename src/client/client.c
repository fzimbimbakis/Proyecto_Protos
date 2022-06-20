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

    /**
     * int sock= socket(args->mng_family,SOCK_STREAM,IPPROTO_TCP);
    if (sock < 0) {
        debug("FATAL", sock, "socket() failed",0);
    }


    int conn;
    if(args->mng_family == AF_INET || args->mng_family==AF_UNSPEC){
        conn= connect(sock,(const struct sockaddr*) &args->mng_addr_info,args->mng_addr_info.sin_len);
    }else{
        conn= connect(sock,(const struct sockaddr*) &args->mng_addr_info6,args->mng_addr_info6.sin6_len);
    }
    if (conn < 0) {
        debug("FATAL", conn, "connect() failed",0);
    }
     */

    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");


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
    close(sockfd);
    free(args->user);
    free(args);

    return 0;

}






