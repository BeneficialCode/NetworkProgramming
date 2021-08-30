// socks5_server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#endif

#if defined(_WIN32)
#define ISVALIDSOCKET(s) ((s)!=INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

#else
#define ISVALIDSOCKET(s) ((s)>=0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#endif

#include <stdio.h>
#include <string.h>

#include "Socks5.h"


int main(){
#ifdef _WIN32
	WSADATA d;
	if (::WSAStartup(MAKEWORD(2, 2), &d)) {
		fprintf(stderr, "Failed to initialize.\n");
		return 1;
	}
#endif

	printf("Configuring local address...\n");
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	addrinfo* bind_address;
	getaddrinfo(0, "8080", &hints, &bind_address);

	printf("Create socket...\n");
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
		fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
		return 1;
	}

	printf("Waiting for connections...\n");
	sockaddr_storage client_address;
	socklen_t client_len = sizeof(client_address);
	SOCKET socket_client;
	while (socket_client = accept(socket_listen,
		(sockaddr*)&client_address, &client_len)) {
		if (!ISVALIDSOCKET(socket_client)) {
			fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
			return 1;
		}
		char address_buffer[100];
		getnameinfo((sockaddr*)&client_address, client_len,
			address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
		printf("New connection from %s\n", address_buffer);

		HANDLE hThread = ::CreateThread(nullptr, 0, Socks5, (void*)&socket_client, 0, nullptr);
		if (hThread == nullptr) {
			fprintf(stderr, "Create Thread failed.\n");
			goto end;
		}
		::WaitForSingleObject(hThread, INFINITE);
	}
	
end:
	printf("Closing listening socket...\n");
	CLOSESOCKET(socket_listen);

#ifdef _WIN32
	WSACleanup();
#endif
	printf("Finished.\n");
	return 0;
}


