#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/logger.h"
#include "../include/tcpClientUtil.h"

#define BUFSIZE 512

char buffer[1024] = {0};
int idx = 0;

int main(int argc, char *argv[]) {

    if (argc != 3) {
        log(FATAL, "usage: %s <Server Name/Address> <Server Port/Name>", argv[0]);
    }

    char *server = argv[1];     // First arg: server name IP address

    // Third arg server port
    char * port = argv[2];

    // Create a reliable, stream socket using TCP
    int sock = tcpClientSocket(server, port);
    if (sock < 0) {
        log(FATAL, "socket() failed")
    }


    // Send the string to the server
   /* size_t numBytes = send(sock, echoString, echoStringLen, 0);
    if (numBytes < 0 || numBytes != echoStringLen)
        log(FATAL, "send() failed expected %zu sent %zu", echoStringLen, numBytes);*/

    // Receive the same string back from the server
    //unsigned int totalBytesRcvd = 0; // Count of total bytes received
    //log(INFO, "Received: ")     // Setup to print the echoed string
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


        //while (totalBytesRcvd < echoStringLen && numBytes >= 0) {
            //char buffer[BUFSIZE];
            /* Receive up to the buffer size (minus 1 to leave space for a null terminator) bytes from the sender */
            // TODO si no recibe nada traba los envios al server también
            size_t numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
            if (numBytes < 0) {
                log(ERROR, "recv() failed")
            } else if (numBytes == 0)
                log(ERROR, "recv() connection closed prematurely")
            else {
                //totalBytesRcvd += numBytes; // Keep tally of total bytes
                buffer[numBytes] = '\0';    // Terminate the string!
                printf("Received: %s", buffer);      // Print the echo buffer
            }
        //}
    }

    close(sock);
    return 0;
}


