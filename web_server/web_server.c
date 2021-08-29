#include "../common/pch.h"

#define MAX_REQUEST_SIZE 2047

struct client_info {
	socklen_t address_length;
	struct sockaddr_storage address;
	SOCKET socket;
	SSL* ssl;
	char request[MAX_REQUEST_SIZE + 1];
	// the number of bytes stored in that array
	int received;
	struct client_info* next;
};

struct client_info* get_client(struct client_info** client_list, SOCKET s);
void drop_client(struct client_info** client_list, struct client_info* client);

const char* get_client_address(struct client_info* ci);

fd_set wait_on_clients(struct client_info** client_list, SOCKET server);
void send_400(struct client_info** client_list, struct client_info* client);
void send_404(struct client_info** client_list, struct client_info* client);

void serve_resource(struct client_info** client_list,struct client_info* client, const char* path);

const char* get_content_type(const char* path);
SOCKET create_socket(const char* host, const char* port);

int main() {

#if defined(_WIN32)
	WSADATA d;
	if (WSAStartup(MAKEWORD(2, 2), &d)) {
		fprintf(stderr, "Failed to initialize.\n");
		return 1;
	}
#endif
	
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
	if (!ctx) {
		fprintf(stderr, "SSL_CTX_new() failed.\n");
		return 1;
	}

	if (!SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM)
		|| !SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM)) {
		fprintf(stderr, "SSL_CTX_use_certificate_file() failed.\n");
		ERR_print_errors_fp(stderr);
		return 1;
	}

	SOCKET server = create_socket(0, "8080");

	// accept connections from only the local system,and not outside systems
	// SOCKET server = create_socket("127.0.0.1","8080");
	struct client_info* client_list = 0;

	while (1) {
		fd_set reads;
		reads = wait_on_clients(&client_list, server);

		if (FD_ISSET(server, &reads)) {
			// -1 is not a valid socket specifier,so get_client creates a new struct client_info
			struct client_info* client = get_client(&client_list, -1);

			// accept the new connection and place the connected clients address information
			// into the respective client fields.
			client->socket = accept(server,
				(struct sockaddr*)&(client->address),
				&(client->address_length));
			if (!ISVALIDSOCKET(client->socket)) {
				fprintf(stderr, "accpet() failed. (%d)\n",
					GETSOCKETERRNO());
				return 1;
			}

			client->ssl = SSL_new(ctx);
			if (!client->ssl) {
				fprintf(stderr, "SSL_new() failed.\n");
				return 1;
			}

			SSL_set_fd(client->ssl, client->socket);
			if (SSL_accept(client->ssl) != 1) {
				ERR_print_errors_fp(stderr);
				drop_client(&client_list, client);
			}
			else {
				printf("New connection from %s.\n", get_client_address(client));

				printf("SSL connection using %s\n", SSL_get_cipher(client->ssl));
			}
		}

		struct client_info* client = client_list;
		while (client) {
			struct client_info* next = client->next;

			if (FD_ISSET(client->socket, &reads)) {
				if (MAX_REQUEST_SIZE == client->received) {
					send_400(&client_list, client);
					client = next;
					continue;
				}

				/*int r = recv(client->socket,
					client->request + client->received,
					MAX_REQUEST_SIZE - client->received, 0);*/

				int r = SSL_read(client->ssl,
					client->request + client->received,
					MAX_REQUEST_SIZE - client->received);
				if (r < 1) { // A client that disconnects unexpectedly
					printf("Unexpected disconnect from %s.\n",
						get_client_address(client));
					drop_client(&client_list, client);
				}
				else {
					client->received += r;
					client->request[client->received] = 0;

					// dectects whether the HTTP header has been received
					char* q = strstr(client->request, "\r\n\r\n");
					if (q) {
						*q = 0;

						if (strncmp("GET /", client->request, 5)) {
							send_400(&client_list, client);
						}
						else {
							char* path = client->request + 4;
							char* end_path = strstr(path, " ");
							if (!end_path) {
								send_400(&client_list, client);
							}
							else {
								*end_path = 0;
								serve_resource(&client_list, client, path);
							}
						}
					}

				}
			}

			client = next;
		}
	}

	printf("\nClosing socket...\n");
	CLOSESOCKET(server);

#if defined(_WIN32)
	WSACleanup();
#endif

	printf("Finished.\n");
	return 0;
}

const char* get_content_type(const char* path) {
	const char* last_dot = strrchr(path, '.');
	if (last_dot) {
		if (strcmp(last_dot, ".css") == 0)
			return "text/css";
		if (strcmp(last_dot, ".csv") == 0)
			return "text/csv";
		if (strcmp(last_dot, ".gif") == 0)
			return "image/gif";
		if (strcmp(last_dot, ".htm") == 0)
			return "text/html";
		if (strcmp(last_dot, ".html") == 0)
			return "text/html";
		if (strcmp(last_dot, ".ico") == 0)
			return "image/x-icon";
		if (strcmp(last_dot, ".jpeg") == 0)
			return "image/jpeg";
		if (strcmp(last_dot, ".jpg") == 0)
			return "image/jpeg";
		if (strcmp(last_dot, ".js") == 0)
			return "application/javascript";
		if (strcmp(last_dot, ".json") == 0)
			return "application/json";
		if (strcmp(last_dot, ".png") == 0)
			return "image/png";
		if (strcmp(last_dot, ".pdf") == 0)
			return "application/pdf";
		if (strcmp(last_dot, ".svg") == 0)
			return "image/svg+xml";
		if (strcmp(last_dot, ".txt") == 0)
			return "text/plain";
	}

	// unknown binary blob
	return "application/octet-stream";
}

SOCKET create_socket(const char* host, const char* port) {
	printf("Configuring local address...\n");
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo* bind_address;
	// find the listening address
	getaddrinfo(host, port, &hints, &bind_address);
	
	printf("Creating socket...\n");
	SOCKET socket_listen;
	socket_listen = socket(bind_address->ai_family,
		bind_address->ai_socktype, bind_address->ai_protocol);
	if (!ISVALIDSOCKET(socket_listen)) {
		fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
		exit(1);
	}

	printf("Binding socket to local address...\n");
	if (bind(socket_listen,
		bind_address->ai_addr, bind_address->ai_addrlen)) {
		fprintf(stderr, "bind() failed (%d)\n", GETSOCKETERRNO());
		exit(1);
	}
	freeaddrinfo(bind_address);

	printf("Listening...\n");
	if (listen(socket_listen, 10) < 0) {
		fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
		exit(1);
	}

	return socket_listen;
}

struct client_info* get_client(struct client_info** client_list,SOCKET s) {
	struct client_info* ci = *client_list;

	// linked list search functionality
	while (ci) {
		if (ci->socket == s)
			break;
		ci = ci->next;
	}

	if (ci)
		return ci;

	struct client_info* n =
		(struct client_info*)calloc(1, sizeof(struct client_info));
	
	if (!n) {
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}

	n->address_length = sizeof(n->address);
	// 头插法
	n->next = *client_list;
	*client_list = n;
	return n;
}

void drop_client(struct client_info** client_list,struct client_info* client) {
	SSL_shutdown(client->ssl);
	// close and clean up the client's connection
	CLOSESOCKET(client->socket);
	SSL_free(client->ssl);

	struct client_info** p = client_list;

	// seraches through our linked list of clients
	while (*p) {
		if (*p == client) {
			// removes a given client
			*p = client->next;
			free(client);
			return;
		}
		// 遍历指针域
		p = &(*p)->next;
	}

	fprintf(stderr, "drop_client not found.\n");
	exit(1);
}

const char* get_client_address(struct client_info* ci) {
	// ensures that its memory is avaliable after the function returns
	static char address_buffer[100];
	getnameinfo((struct sockaddr*)&ci->address,
		ci->address_length,
		address_buffer, sizeof(address_buffer),0,0,
		NI_NUMERICHOST);
	return address_buffer;
}

fd_set wait_on_clients(struct client_info** client_list,SOCKET server) {
	fd_set reads;
	FD_ZERO(&reads);
	FD_SET(server, &reads);
	SOCKET max_socket = server;

	struct client_info* ci = *client_list;

	// loops through the linked list of connected clients
	while (ci) {
		// adds the socket for each one in turn
		FD_SET(ci->socket, &reads);
		if (ci->socket > max_socket)
			max_socket = ci->socket;
		ci = ci->next;
	}

	// select() returns when one or more of the sockets in reads is ready
	if (select(max_socket + 1, &reads,0,0,0)<0) {
		fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
		exit(1);
	}

	return reads;
}

void send_400(struct client_info** client_list,struct client_info* client) {
	const char* c400 = "HTTP/1.1 400 Bad Request\r\n"
		"Connection: close\r\n"
		"Content-Length: 11\r\n\r\nBad Request";
	//send(client->socket, c400, strlen(c400), 0);
	SSL_write(client->ssl, c400, strlen(c400));
	drop_client(client_list, client);
}

void send_404(struct client_info** client_list,struct client_info* client) {
	const char* c404 = "HTTP/1.1 404 Not Found\r\n"
		"Connection: close\r\n"
		"Content-Length: 9\r\n\r\nNot Found";
	//send(client->socket, c404, strlen(c404), 0);
	SSL_write(client->ssl, c404, strlen(c404));
	drop_client(client_list,client);
}

void serve_resource(struct client_info** client_list,struct client_info* client, const char* path) {
	printf("server_resource %s %s\n", get_client_address(client), path);

	if (strcmp(path, "/") == 0)
		path = "/index.html";
	
	if (strlen(path) > 100) {
		send_400(client_list, client);
		return;
	}

	if (strstr(path, "..")) {
		send_404(client_list, client);
		return;
	}

	char full_path[128];
	sprintf(full_path, "public%s", path);

#if defined(_WIN32)
	char* p = full_path;
	while (*p) {
		if (*p == '/') 
			// overwritten with a backslash
			*p = '\\';
		++p;
	}
#endif
	FILE* fp = fopen(full_path, "rb");

	if (!fp) {
		send_404(client_list, client);
		return;
	}
	// determine the requested file's size
	fseek(fp, 0L, SEEK_END);
	size_t cl = ftell(fp);
	rewind(fp);

	const char* ct = get_content_type(full_path);
#define BSIZE 1024
	char buffer[BSIZE];
	sprintf(buffer, "HTTP/1.1 200 OK\r\n");
	//send(client->socket, buffer, strlen(buffer), 0);
	SSL_write(client->ssl, buffer, strlen(buffer));

	sprintf(buffer, "Connection: close\r\n");
	//send(client->socket, buffer, strlen(buffer), 0);
	SSL_write(client->ssl, buffer, strlen(buffer));


	sprintf(buffer, "Content-Length: %u\r\n", cl);
	//send(client->socket, buffer, strlen(buffer), 0);
	SSL_write(client->ssl, buffer, strlen(buffer));


	sprintf(buffer, "Content-Type: %s\r\n", ct);
	//send(client->socket, buffer, strlen(buffer), 0);
	SSL_write(client->ssl, buffer, strlen(buffer));


	// is used by the client to delineate the HTTP header from the beginning of the HTTP body
	sprintf(buffer, "\r\n");
	//send(client->socket, buffer, strlen(buffer), 0);
	SSL_write(client->ssl, buffer, strlen(buffer));


	// may block on large files
	int r = fread(buffer, 1, BSIZE, fp);
	while (r) {
		//send(client->socket, buffer, r, 0);
		SSL_write(client->ssl, buffer, r);
		r = fread(buffer, 1, BSIZE, fp);
	}

	fclose(fp);
	drop_client(client_list, client);
}