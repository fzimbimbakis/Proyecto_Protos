#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../../include/clientArgs.h"
#include "../../include/debug.h"


#define MAX 80
#define PORT 8080
#define SA struct sockaddr
void func(int sockfd)
{
    char buff[MAX];
    int n;
    for (;;) {
        bzero(buff, sizeof(buff));
        printf("Enter the string : ");
        n = 0;
        while ((buff[n++] = getchar()) != '\n')
            ;
        send(sockfd, buff, sizeof(buff),0);
        bzero(buff, sizeof(buff));
        recv(sockfd, buff, sizeof(buff),0);
        printf("From Server : %s", buff);
        if ((strncmp(buff, "exit", 4)) == 0) {
            printf("Client Exit...\n");
            break;
        }
    }
}

int handshake(int sockfd, struct user* user){
    char buffer[4];
    int bytes_to_send;
    buffer[0]=0x01; //VERSION
    if(user->credentials){
        buffer[1]=0x02; //NMETODS
        buffer[2]=0x00; //NO AUTHENTICATION REQUIRED
        buffer[3]=0x01; //USERNAME/PASSWORD
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

int send_request(int sockfd){
    uint8_t buffer[250];
    int bytes_to_send=1;

    printf("Enter index request: ");

    int index;
    scanf("%d",&index );

    switch (index) {
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
            break;
        case 11://Delete a user.
            buffer[0]=0x0B;
            break;
        case 12://Disable server authentication.
            buffer[0]=0x0C;
            break;
        case 13://Disable password disectors.
            buffer[0]=0x0D;
            break;
        case 255:
            buffer[0]=0xFF;
            break;
        default:
            return -1;
    }

    return send(sockfd, buffer, bytes_to_send,0);
}



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
    if(ret < 0)
        goto error;



    uint8_t response=handshake_response(sockfd);
    if(response==0xFF)
        goto error;

    if(response==0x01) {
        //send_credentials(sockfd, args->user);
        printf("send credentials");
    }

    ret= send_request(sockfd);
    printf("%d", ret);
    if(ret < 0)
        goto error;





    // function for chat
    //func(sockfd);

    // close the socket
    close(sockfd);

    return 0;


    error:
    printf("error\n");
    close(sockfd);
    free(args->user);
    free(args);

}




/*#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../include/debug.h"
#include "../../include/tcpClientUtil.h"
#include "../../include/clientArgs.h"

#define BUFSIZE 512

char buffer[1024] = {0};
int idx = 0;

int main(int argc, char *argv[]) {

    struct m16args * args = malloc(sizeof(struct m16args));

    if(args == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(args, 0, sizeof(*args));


    //  Get configurations and users
    int parse_args_result = parse_args(argc, (char *const *)argv, args);

    if(parse_args_result == -1){
        free(args);
        exit(1);
    }

    int debug_option = args->debug;
    debug_init(debug_option);

    int sock= socket(args->mng_family,SOCK_STREAM,IPPROTO_TCP);
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


    while(1) {
        char c;
        while ((c = getchar()) != '\n') {
            if (c == EOF)
                break;
            buffer[idx++] = c;
        }
        if (c == EOF) {
            send(sock, "", 0, 0);
            break;
        }
        buffer[(idx)++] = '\n';
        buffer[(idx)++] = 0;
        idx = 0;
        send(sock, buffer, strlen(buffer), 0);
        printf("Sent: %s", buffer);
        buffer[0] = 0;


        while (totalBytesRcvd < echoStringLen && numBytes >= 0) {
            char buffer[BUFSIZE];
            // Receive up to the buffer size (minus 1 to leave space for a null terminator) bytes from the sender
            size_t numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
            if (numBytes < 0) {
                debug("ERROR", argc, "recv() failed",0);

            }else if (numBytes == 0) {
                debug("ERROR", argc, "recv() connection closed prematurely", 0);
            }else {
                //totalBytesRcvd += numBytes; // Keep tally of total bytes
                buffer[numBytes] = '\0';    // Terminate the string!
                printf("Received: %s", buffer);      // Print the echo buffer
            }
        }
    }

    close(sock);
    return 0;
}*/




