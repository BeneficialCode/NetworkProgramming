#include "..\common\chap13.h"

#if !defined(_WIN32)
#include <signal.h>
#endif


int main() {

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);


    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
        bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
        bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);


    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Waiting for connection...\n");
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client = accept(socket_listen,
        (struct sockaddr*)&client_address, &client_len);
    if (!ISVALIDSOCKET(socket_client)) {
        fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Client is connected.\n");
    printf("Waiting for client to disconnect.\n");

    int bytes_received;
    while (1) {
        char read[1024];
        bytes_received = recv(socket_client, read, 1024, 0);
        if (bytes_received < 1) {
            break;
        }
        else {
            printf("Received %d bytes.\n", bytes_received);
        }
    }

    printf("Client has disconnected.\n");
    printf("recv() returned %d\n", bytes_received);

    printf("Attempting to send first data.\n");

    int sent;

    sent = send(socket_client, "a", 1, 0);
    if (sent != 1) {
        fprintf(stderr, "first send() failed. (%d, %d)\n", sent, GETSOCKETERRNO());
    }

    printf("Attempting to send second data.\n");

    sent = send(socket_client, "a", 1, 0);
    if (sent != 1) {
        fprintf(stderr, "second send() failed. (%d, %d)\n", sent, GETSOCKETERRNO());
    }

    printf("Closing socket.\n");

    CLOSESOCKET(socket_client);

    printf("Finished.\n");

    return 0;
}

