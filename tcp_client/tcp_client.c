#include "../common/pch.h"

// tcp_client.exe example.com http

// gcc tcp_client.c -o tcp_client.exe -lssl -lcrypto -lws2_32
// tcp_client.exe example.com https
#if defined(_WIN32)
#include <conio.h>
#endif



int main(int argc, char* argv[]) {
#if defined(_WIN32)
	WSADATA d;
	if (WSAStartup(MAKEWORD(2, 2), &d)) {
		fprintf(stderr, "Failed to initialize.\n");
		return 1;
	}
#endif

#ifdef OPENSSL
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
	if (!ctx) {
		fprintf(stderr, "SSL_CTX_new() failed.\n");
		return 1;
	}
#endif

	if (argc < 3) {
		fprintf(stderr, "usage:tcp_client hostname port\n");
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
		peer_address->ai_addr,peer_address->ai_addrlen)) {
		fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
		return 1;
	}
	freeaddrinfo(peer_address);

#ifdef  OPENSSL
	SSL* ssl = SSL_new(ctx);
	if (!ssl) {
		fprintf(stderr, "SSL_new() failed.\n");
		return 1;
	}

	SSL_set_fd(ssl, socket_peer);
	if (SSL_connect(ssl) == -1) {
		fprintf(stderr, "SSL_connect() failed.\n");
		ERR_print_errors_fp(stderr);
		return 1;
	}

	printf("SSL/TLS using %s\n", SSL_get_cipher(ssl));

	X509* cert = SSL_get_peer_certificate(ssl);
	if (!cert) {
		fprintf(stderr, "SSL_get_peer_certificate() failed.\n");
		return 1;
	}

	char* tmp;
	if ((tmp = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0))) {
		printf("subject: %s\n", tmp);
		OPENSSL_free(tmp);
	}

	if ((tmp = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0))) {
		printf("issuer: %s\n", tmp);
		OPENSSL_free(tmp);
	}

	X509_free(cert);
#endif //  OPENSSL

	// Set to non-blocking. This needed to for the SSL 
	// function. Without this, SSL_read() might block.
#if defined(_WIN32)
	unsigned long nonblock = 1;
	ioctlsocket(socket_peer, FIONBIO, &nonblock);
#else
	int flags;
	flags = fcntl(socket_peer, F_GETFL, 0);
	fcntl(socket_peer, F_SETFL, flags | O_NONBLOCK);
#endif


	printf("Connected.\n");
	printf("To send data,enter text followed by enter.\n");

	while (1) {
		fd_set reads;
		FD_ZERO(&reads);
		FD_SET(socket_peer, &reads);
#if !defined(_WIN32)
		FD_SET(0, &reads); // monitor for terminal input
#endif

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;
		if (select(socket_peer + 1, &reads, 0, 0, &timeout) < 0) {
			fprintf(stderr, "Select() failed. (%d)\n", GETSOCKETERRNO());
			return 1;
		}

		if (FD_ISSET(socket_peer, &reads)) {
			char read[4096];
#ifdef OPENSSL
			int bytes_received = SSL_read(ssl, read, 4096);
#else
			int bytes_received = recv(socket_peer, read, 4096, 0);
			printf("read before\n");
#endif
			if (bytes_received < 1) { // the connection has ended
#ifdef OPENSSL
				int err;
				if ((err = SSL_get_error(ssl, bytes_received)) &&
					(err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_ASYNC)) {
					// Just wating on SSL,nothing to do.
				}
				else {
					printf("Connection closed by peer.\n");
					break;
				}
#else
				printf("Connection closed by peer.\n");
				break;
#endif
			}
			else {
				printf("Received (%d bytes): %.*s",
					bytes_received, bytes_received, read);
			}
		}

		//printf("_kbhit() before\n");
#if defined(_WIN32)
		if (_kbhit()) { // whether any console input is waiting
#else
		if (FD_ISSET(0, &reads)) {
#endif
			//printf("_kbhit() after");
			char read[4096];
			if (!fgets(read, 4096, stdin))
				break;
			printf("Sending: %s", read);
			
#ifdef OPENSSL
			int bytes_sent = SSL_write(ssl, read, strlen(read));
#else
			int bytes_sent = send(socket_peer, read, strlen(read), 0);
#endif
			printf("Sent %d bytes.\n", bytes_sent);
		}
	}

	printf("Closing socket...\n");
	CLOSESOCKET(socket_peer);
#if defined(_WIN32)
	WSACleanup();
#endif

	printf("Finished.\n");
	return 0;
}