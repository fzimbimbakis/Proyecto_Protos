#include "../include/address_utils.h"
int address_processing(char * address, struct sockaddr_in *addr, struct sockaddr_in6 *addr6, uint16_t port){

    //// IPv4
    int result = inet_pton(AF_INET, address, &addr->sin_addr);

    if (result <= 0)
    {
        //// IPv6
        result = inet_pton(AF_INET6, address, &addr6->sin6_addr);

        if(result <= 0)
            return -1;
        else {
            addr6->sin6_family = AF_INET6;
            addr6->sin6_port = htons(port);
            return AF_INET6;
        }
    }
    else {
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = inet_addr(address);
        addr->sin_port = htons(port);
        return AF_INET;
    }
}
