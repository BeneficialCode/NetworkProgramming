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

	METHOD_SELECT_REQUEST* request;
	int bytes_received = recv(sock, read, sizeof(read), 0);
	if (bytes_received < 1) { // the client has disconnected
		return -1;
	}

	size_t request_len = sizeof(METHOD_SELECT_REQUEST);
	if (bytes_received < request_len)
		return -1;

	request = (METHOD_SELECT_REQUEST*)read;
	
	int method_len = request->nmethods + sizeof(METHOD_SELECT_REQUEST);
	if (bytes_received < method_len)
		return -1;

	METHOD_SELECT_RESPONSE response;
	response.ver = SVERSION;
	response.method = METHOD_UNACCEPTABLE;
	for(int i=0;i<request->nmethods;i++)
		if (request->methods[i] == METHOD_NOAUTH) {
			response.method = METHOD_NOAUTH;
			break;
		}

	if (response.method == METHOD_UNACCEPTABLE)
		return -1;

	char* send_buf = (char*)&response;
	send(sock, send_buf, sizeof(response), 0);

	return 0;
}

int ParseCommand(int sock) {
	char read[1024] = { 0 };
	char reply[64];

	SOCKS5_REQUEST* request;

	int bytes_received = recv(sock, read, sizeof(read), 0);
	if (bytes_received < 1) { // the client has disconnected
		return -1;
	}

	request = (PSOCKS5_REQUEST)read;
	size_t request_len = sizeof(SOCKS5_REQUEST);
	if (bytes_received < request_len) {
		return -1;
	}

	SOCKS5_RESPONSE response;
	response.ver = SVERSION;
	response.reply = SOCKS5_REP_SUCCEEDED;
	response.reserved = 0;
	response.atyp = SOCKS5_ATYP_IPV4;

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	printf("begin process socks5 request\n");
	if (request->cmd == SOCKS5_CMD_UDP_ASSOCIATE) {
		printf("udp assc request accepted\n");
		socklen_t addr_len = sizeof(sin);
		if (getsockname(sock, (struct sockaddr*)&sin, &addr_len) < 0) {
			return -1;
		}

		memcpy(reply, &response, sizeof(response));
		memcpy(reply + sizeof(response), &sin.sin_addr, sizeof(sin.sin_addr));
		memcpy(reply + sizeof(response) + sizeof(sin.sin_addr), &sin.sin_port, sizeof(sin.sin_port));

		int reply_size = sizeof(SOCKS5_RESPONSE) + sizeof(sin.sin_addr) + sizeof(sin.sin_port);

		int s = send(sock, reply, reply_size, 0);
		if (s < reply_size) {
			return -1;
		}

		return -1;
	}
	else if (request->cmd != SOCKS5_CMD_CONNECT) {
		printf("unsupported command: %d", request->cmd);
		return -1;
	}

	int real_server_sock = -1;

	int atyp = request->atyp;
	printf("get real server's ip address\n");
	if (atyp == SOCKS5_ATYP_IPV4) {
		sin.sin_family = AF_INET;
		size_t in_addr_len = sizeof(in_addr);
		if (bytes_received < request_len + in_addr_len + 2) {
			return -1;
		}
		memcpy(&sin.sin_addr.S_un.S_addr, &request->atyp +
			sizeof(request->atyp), 4);
		memcpy(&sin.sin_port, &request->atyp +
			sizeof(request->atyp) + 4, 2);
		printf("Real Server: %s:%d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		printf("Connecting real server...\n");
		real_server_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (real_server_sock < 0) {
			return -1;
		}
		if (connect(real_server_sock, (struct sockaddr*)&sin, sizeof(sockaddr_in))) {
			response.reply = SOCKS5_REP_GENERAL;
			memcpy(reply, &response, sizeof(response));
			memcpy(reply + sizeof(response), &sin.sin_addr, sizeof(sin.sin_addr));
			memcpy(reply + sizeof(response) + sizeof(sin.sin_addr), &sin.sin_port, sizeof(sin.sin_port));

			int reply_size = sizeof(SOCKS5_RESPONSE) + sizeof(sin.sin_addr) + sizeof(sin.sin_port);

			int s = send(sock, reply, reply_size, 0);
			if (s < reply_size) {
				return -1;
			}
		}
	}
	else if (atyp == SOCKS5_ATYP_DOMAIN) {
		char domainLength = *(&request->atyp + sizeof(request->atyp));
		char target_domain[256] = { 0 };
		strncpy(target_domain, (char*)&request->atyp + 2, (unsigned int)domainLength);
		target_domain[domainLength] = '\0';
		printf("target: %s\n", target_domain);
		struct hostent* remoteHost = gethostbyname(target_domain);
		if (remoteHost == nullptr) {
			fprintf(stderr, "resolve %s error!\n", target_domain);
			return -1;
		}
		sin.sin_family = remoteHost->h_addrtype;
		memcpy(&sin.sin_addr, remoteHost->h_addr_list[0], remoteHost->h_length);
		memcpy(&sin.sin_port, &request->atyp +
			sizeof(request->atyp) + sizeof(domainLength) + domainLength, 2);
		printf("Real Server: %s:%d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		printf("Connecting real server...\n");

		switch (remoteHost->h_addrtype)
		{
			case AF_INET:
				real_server_sock = socket(AF_INET, SOCK_STREAM, 0);
				break;

			case AF_INET6:
				real_server_sock = socket(AF_INET6, SOCK_STREAM, 0);
				break;
			default:
				break;
		}
		
		if (real_server_sock < 0) {
			return -1;
		}

		if (connect(real_server_sock, (struct sockaddr*)&sin, sizeof(sockaddr_in))) {
			fprintf(stderr, "socket() failed (%d)\n", GETSOCKETERRNO());
			response.reply = SOCKS5_REP_GENERAL;
			memcpy(reply, &response, sizeof(response));
			memcpy(reply + sizeof(response), &sin.sin_addr, sizeof(sin.sin_addr));
			memcpy(reply + sizeof(response) + sizeof(sin.sin_addr), &sin.sin_port, sizeof(sin.sin_port));

			int reply_size = sizeof(SOCKS5_RESPONSE) + sizeof(sin.sin_addr) + sizeof(sin.sin_port);

			int s = send(sock, reply, reply_size, 0);
			if (s < reply_size) {
				return -1;
			}
		}
	}
	else if (atyp == SOCKS5_ATYP_IPV6) {
		sin.sin_family = AF_INET6;
		return -1;
	}
	else {
		printf("unsupported addrtype: %d", request->atyp);
		return -1;
	}
	
	memcpy(reply, &response, sizeof(response));
	memcpy(reply + sizeof(response), &sin.sin_addr, sizeof(sin.sin_addr));
	memcpy(reply + sizeof(response) + sizeof(sin.sin_addr), &sin.sin_port, sizeof(sin.sin_port));

	int reply_size = sizeof(SOCKS5_RESPONSE) + sizeof(sin.sin_addr) + sizeof(sin.sin_port);

	int s = send(sock, reply, reply_size, 0);
	if (s < reply_size) {
		return -1;
	}

	return real_server_sock;
}

int ForwardData(int sock, int real_server_sock) {
	char read[4096] = { 0 };

	if (real_server_sock == -1) {
		printf("invalid remote\n");
		return -1;
	}

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