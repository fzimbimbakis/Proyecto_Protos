#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 8080

int main(int argc, char const *argv[]) {
    int sock = 0, valread, client_fd;
    struct sockaddr_in serv_addr;
    char msg[1024];
    msg[0] = 0x01;
    msg[1] = 0x02;
    msg[2] = 0x00;
    msg[3] = 0x02;
    unsigned char buffer[1024] = {0};
    printf("HELLO\n\tVersion: 0x%02X\tAuth: 0x%02X 0x%02X 0x%02X\n", msg[0], msg[1], msg[2], msg[3]);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
        <= 0) {
        printf(
                "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((client_fd
                 = connect(sock, (struct sockaddr *) &serv_addr,
                           sizeof(serv_addr)))
        < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    send(sock, msg, 4, 0);

    valread = read(sock, buffer, 2);
    printf("HELLO ANSWER\n\tVersion: 0x%02X\tAuth method: 0x%02X\n", buffer[0], buffer[1]);

    int index = atoi(argv[1]);
    printf("REQUEST\n\tIndex: 0x%02X\n", index);
    int count = 0, aux;
    msg[count++] = index;
    switch (index) {
        case 10: {
            if (argc != 4) {
                printf("Index 0x0A, username or password are missing on arguments\n");
                return 0;
            }
            //// Username
            aux = strlen(argv[2]);
            msg[count++] = aux;
            memcpy(msg + count, argv[2], aux);
            count += aux;
            //// Password
            aux = strlen(argv[3]);
            msg[count++] = aux;
            memcpy(msg + count, argv[3], aux);
            count += aux;
            break;
        }
        case 11: {
            if (argc != 3) {
                printf("Index 0x0B, Missing username\n");
                return 0;
            }
            //// Username
            aux = strlen(argv[2]);
            msg[count++] = aux;
            memcpy(msg + count, argv[2], aux);
            count += aux;
            break;
        }
        case 12: {
            if (argc != 3) {
                printf("Index 0x0C, Missing new auth status\n");
                return 0;
            }
            msg[count++] = atoi(argv[2]);
            break;
        }
        case 13: {
            if (argc != 3) {
                printf("Index 0x0D, Missing password dissector status\n");
                return 0;
            }
            msg[count++] = atoi(argv[2]);
            break;
        }
        default:
            break;
    }
    send(sock, msg, count, 0);
    valread = read(sock, buffer, 1024);
    printf("\tStatus: 0x%02X\n\t", buffer[0]);
    if (buffer[0] != 0)
        return 0;
    switch (index) {
        case 0: {
            printf("List of supported indexes 0x%02X\n", buffer[1]);
            for (int i = 0; i < buffer[1]; ++i) {
                printf("0x%02X\t", buffer[i + 2]);
            }
            printf("\n");
        }
            break;
        case 1: {
            int n, j, k = 2;
            printf("List of active users 0x%02X\n", buffer[1]);
            for (int i = 0; i < buffer[1]; ++i) {
                n = buffer[k++];
                printf("\t - ");
                for (j = 0; j < n; ++j) {
                    putchar(buffer[k + j]);
                }
                k += n;
                printf("\n");
            }
            printf("\n");
            break;
        }
        default: {
            switch (index) {
                case 2: {
                    printf("Historic connections to the server\n");
                    break;
                }
                case 3: {
                    printf("Actual concurrent connections\n");
                    break;
                }
                case 4: {
                    printf("Max concurrent connections\n");
                    break;
                }
                case 5: {
                    printf("Historic byte transferred\n");
                    break;
                }
                case 6: {
                    printf("Historic authentication attempts\n");
                    break;
                }
                case 7: {
                    printf("Historic connections attempts\n");
                    break;
                }
                case 8: {
                    printf("Average byte size per server read\n");
                    break;
                }
                case 9: {
                    printf("Average byte size per server write\n");
                    break;
                }
                case 10: {
                    printf("Added new user\n");
                    return 0;
                }
                case 11: {
                    printf("Delete user\n");
                    return 0;
                }
                case 12: {
                    printf("Disable server authentication\n");
                    return 0;
                }
                case 13: {
                    printf("Disable password dissectors\n");
                    return 0;
                }
            }
            printf("\tRequested stat: %u", *((unsigned int *) (buffer + 1)));
        }
    }
    printf("%s\n", buffer);
    // closing the connected socket
    close(client_fd);
    return 0;
}