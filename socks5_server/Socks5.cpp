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

int SelectMethod(int sock) {
	char read[1024] = { 0 };
	char reply[2] = { 0 };

	METHOD_SELECT_REQUEST* method_request;
	METHOD_SELECT_RESPONSE* method_response;

	int bytes_received = recv(sock, read, sizeof(read), 0);
	if (bytes_received < 1) { // the client has disconnected
		return -1;
	}

	method_request = (METHOD_SELECT_REQUEST*)read;
	method_response = (METHOD_SELECT_RESPONSE*)reply;

	method_response->version = VERSION;
	if (method_request->version != VERSION) {
		method_response->select_method = 0xFF;
		send(sock, reply, sizeof(METHOD_SELECT_RESPONSE), 0);
		return -1;
	}

	method_response->select_method = AUTH_CODE;
	send(sock, reply, sizeof(METHOD_SELECT_RESPONSE), 0);

	return 0;
}

int ParseCommand(int sock) {
	char read[1024] = { 0 };
	char reply[1024];

	SOCKS5_REQUEST* socks5_request;
	SOCKS5_RESPONSE* socks5_response;

	int bytes_received = recv(sock, read, sizeof(read), 0);
	if (bytes_received < 1) { // the client has disconnected
		return -1;
	}

	socks5_request = (PSOCKS5_REQUEST)read;
	if (socks5_request->version != VERSION ||
		socks5_request->cmd != CONNECT || socks5_request->address_type == IPV6) {
		return -1;
	}

	printf("begin process connect request\n");
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	printf("get real server's ip address\n");
	if (socks5_request->address_type == IPV4) {
		memcpy(&sin.sin_addr.S_un.S_addr, &socks5_request->address_type +
			sizeof(socks5_request->address_type), 4);
		memcpy(&sin.sin_port, &socks5_request->address_type +
			sizeof(socks5_request->address_type) + 4, 2);
		printf("Real Server: %s:%d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
	}
	else if (socks5_request->address_type == DOMAIN) {
		char domainLenth = *(&socks5_request->address_type + sizeof(socks5_request->address_type));
		char target_domain[256] = { 0 };

		strncpy(target_domain, &socks5_request->address_type + 2, (unsigned int)domainLenth);
		printf("target: %s\n", target_domain);
		struct hostent* phost = gethostbyname(target_domain);
		if (phost == nullptr) {
			fprintf(stderr, "resolve %s error!\n", target_domain);
			return -1;
		}
		memcpy(&sin.sin_addr, phost->h_addr_list[0], phost->h_length);
		memcpy(&sin.sin_port, &socks5_request->address_type +
			sizeof(socks5_request->address_type) + sizeof(domainLenth) + domainLenth, 2);
	}

	printf("try to creat socket with real server...\n");
	int real_server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (real_server_sock < 0) {
		fprintf(stderr, "socket() failed (%d)\n", GETSOCKETERRNO());
		return -1;
	}

	memset(reply, 0, sizeof(reply));

	socks5_response = (PSOCKS5_RESPONSE)reply;
	socks5_response->version = VERSION;
	socks5_response->reserved = 0x0;
	socks5_response->address_type = 0x01;
	memset(socks5_response + 4, 0, 6);

	printf("Connecting real server...\n");
	if (connect(real_server_sock, (struct sockaddr*)&sin,
		sizeof(sockaddr_in))) {
		socks5_response->reply = 0x01;
		send(sock, reply, 10, 0);
		return -1;
	}

	socks5_response->reply = 0x00;
	send(sock, reply, 10, 0);

	return real_server_sock;
}

int ForwardData(int sock, int real_server_sock) {
	char read[4096] = { 0 };

	FD_SET fd_reads;
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;

	while (1) {
		FD_ZERO(&fd_reads);
		FD_SET(sock, &fd_reads);
		FD_SET(real_server_sock, &fd_reads);

		if (select((sock > real_server_sock ?  sock : real_server_sock) + 1, &fd_reads, 0, 0, &timeout) < 0) {
			fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
			break;
		}

		if (FD_ISSET(sock, &fd_reads)) {
			//memset(read, 0, sizeof(read));
			int bytes_received = recv(sock, read, sizeof(read), 0);
			if (bytes_received < 1) {
				printf("Connection closed by peer.\n");
				break;
			}
			else {
				printf("Received (%d bytes): %.*s", bytes_received, bytes_received, read);
			}
			send(real_server_sock, read, bytes_received, 0);
		}
		else if (FD_ISSET(real_server_sock, &fd_reads)) {
			int bytes_received = recv(real_server_sock, read, sizeof(read), 0);
			if (bytes_received < 1) {
				printf("Connection closed by peer.\n");
				break;
			}
			else {
				send(sock, read, bytes_received, 0);
			}
		}
	}

	return 0;
}

DWORD WINAPI Socks5(void* client_sock) {
	int sock = *(int*)client_sock;

	if (SelectMethod(sock) == -1) {
		return -1;
	}

	int real_server_sock = ParseCommand(sock);
	if (real_server_sock == -1) {
		return -1;
	}

	ForwardData(sock, real_server_sock);

	CLOSESOCKET(sock);
	CLOSESOCKET(real_server_sock);
	return 0;
}