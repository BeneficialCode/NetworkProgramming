#include "../common/chap13.h"

int main(int argc, char* argv[]) {

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    if (argc < 3) {
        fprintf(stderr, "usage: big_send hostname port\n");
        return 1;
    }

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* peer_address;
    if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
        address_buffer, sizeof(address_buffer),
        service_buffer, sizeof(service_buffer),
        NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);


    printf("Creating socket...\n");
    SOCKET socket_peer;
    socket_peer = socket(peer_address->ai_family,
        peer_address->ai_socktype, peer_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Connecting...\n");
    if (connect(socket_peer,
        peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    freeaddrinfo(peer_address);

    printf("Connected.\n");
    printf("Sending lots of data.\n");


    const int send_size = 10000;
    char* buffer;
    buffer = (char*)malloc(send_size);
    if (!buffer) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }


    int i;
    for (i = 1; i <= 10000; ++i) {

        fd_set set;
        FD_ZERO(&set);
        FD_SET(socket_peer, &set);

        struct timeval timeout;
        timeout.tv_sec = 0; timeout.tv_usec = 0;
        select(socket_peer + 1, 0, &set, 0, &timeout);

        if (FD_ISSET(socket_peer, &set)) {
            printf("Socket is ready to write.\n");
        }
        else {
            printf("Socket is not ready to write.\n");
        }


        printf("Sending %d bytes (%d total).\n", send_size, i * send_size);
        int r = send(socket_peer, buffer, send_size, 0);
        if (r < 0) {
            fprintf(stderr, "send() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }
        if (r != send_size) {
            printf("send() only consumed %d bytes.\n", r);
            return 1;
        }

    }


    free(buffer);

    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;
}