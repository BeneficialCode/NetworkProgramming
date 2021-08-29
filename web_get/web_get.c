#include "../common/pch.h"
#define TIMEOUT 5.0

void parse_url(char* url, char** hostname, char** port, char** path);
void send_request(SOCKET s, char* hostname, char* port, char* path);
SOCKET connect_to_host(char* hostname, char* port);

int main(int argc, char* argv[]) {

// Winsock is initialized, if needed
#if defined(_WIN32)
	WSADATA d;
	if (WSAStartup(MAKEWORD(2, 2), &d)) {
		fprintf(stderr, "Failed to initialize.\n");
		return 1;
	}
#endif
// check the program's arguments
	if (argc < 2) {
		fprintf(stderr, "usage:web_get url\n");
		return 1;
	}
	char* url = argv[1];
	
	char* hostname, * port, * path;
	// parst the URL into its hostname,port,and path parts
	parse_url(url, &hostname, &port, &path);
	
	// establishing a connection to the target server
	SOCKET server = connect_to_host(hostname, port);
	// sending the HTTP request
	send_request(server, hostname, port, path);

	const clock_t start_time = clock();

#define RESPONSE_SIZE 8192
	// it may be useful to use malloc() to reserve memory on the heap
	char response[RESPONSE_SIZE+1];
	// p is achar pointer that keeps track of how far we have written into response so far
	// q is an additional char pointer that is used later
	char* p = response, * q;
	// to ensure that we don't attempt write past the end of our reserved memory
	char* end = response + RESPONSE_SIZE;
	// is used to remeber the beginning of the HTTP response body once received
	char* body = 0;

	// to list the medhod types
	enum { length, chunked, connection };
	// to store the actual method used
	int encoding = 0;
	// is used to record how many bytes are still 
	// needed to finish the HTTP body or body chunk
	int remaining = 0;
	
	// to receive and process the HTTP response
	while (1) {
		// checks that it hasn't taken too much time
		if ((clock() - start_time) / CLOCKS_PER_SEC > TIMEOUT) {
			fprintf(stderr, "timeout after %.2f seconds", TIMEOUT);
			return 1;
		}
		// checks that we still have buffer space left to store the received data
		if (p == end) {
			fprintf(stderr, "out of buffer space\n");
			return 1;
		}

		fd_set reads;
		FD_ZERO(&reads);
		FD_SET(server, &reads);

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 200000;

		if (select(server + 1, &reads, 0, 0, &timeout) < 0) {
			fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
			return 1;
		}

		// whether new data is available
		if (FD_ISSET(server, &reads)) {
			// read the data into the buffer at the p pointer
			int bytes_received = recv(server, p, end - p, 0);
			// detecting a closed connection
			if (bytes_received < 1) {
				if (encoding == connection && body) {
					// print the HTTP body data that was received
					printf("%.*s", (int)(end - body), body);
				}
				printf("\nConnection closed by peer.\n");
				break;
			}

			printf("Received (%d bytes): '%.*s'",
				bytes_received, bytes_received, p);
			// the p pointer is advanced to point the end of received data
			p += bytes_received;
			// ends with a null terminator
			*p = 0;

			// if the HTTP body hasn't already been found
			// searches through the received data for the blank line
			// that indicates the end of the HTTP header
			if (!body && (body == strstr(response, "\r\n\r\n"))) {
				*body = 0;
				// updates the body pointer to the beginning of the HTTP body
				body += 4;

				printf("Received Headers:\n%s\n", response);


				q = strstr(response, "\nContent-Length: ");
				if (q) {
					encoding = length;
					q = strchr(q, ' ');
					q += 1;
					// store the body length in the remaining variable
					remaining = strtol(q, 0, 10);
				}
				else {
					q = strstr(response, "\nTransfer-Encoding: chunked");
					if (q) {
						encoding = chunked;
						// we haven't read in a chunk length yet
						remaining = 0;
					}
					else {
						encoding = connection;
					}
				}
				printf("\nReceived Body:\n");
			}

			if (body) {
				if (encoding == length) {
					if (p - body >= remaining) {
						// remaining bytes of the HTTP body have been received
						// prints the received body
						printf("%.*s", remaining, body);
						// break from the while loop
						break;
					}
				}
				else if (encoding == chunked) {
					do
					{
						if (remaining == 0) {
							// the program is waiting to receive a new chunk legnth
							if ((q = strstr(body, "\r\n"))) {
								// entire chunk length has been received
								remaining = strtol(body, 0, 16);
								// a chunked message is terminated by a zero-length chunk
								if (!remaining)
									goto finish;
								body = q + 2;
							}
							else {
								break;
							}
						}
						if (remaining && p - body >= remaining) {
							printf("%.*s", remaining, body);
							// is advanced to the end of the current chunk
							body += remaining + 2;
							remaining = 0;
						}
					} while (!remaining);
				}
			}
		}
	}
finish:

	printf("\nClosing socket...\n");
	CLOSESOCKET(server);

#ifdef _WIN32
	WSACleanup();
#endif

	printf("Finished.\n");
	return 0;
}



void parse_url(char* url, char** hostname, char** port, char** path) {
	printf("URL: %s\n", url);

	char* p;
	// to search for :// in the URL
	p = strstr(url, "://");

	// set to 0 to indicate that no protocol has been found
	char* protocol = 0;
	if (p) {
		// set to the beginning of the url
		protocol = url;
		*p = 0;
		// set to one after://, 
		// which should be where the hostname begins.
		p += 3;
	}
	else {
		p = url;
	}

	if (protocol) {
		// checks that it points to the text http
		if (strcmp(protocol, "http")) {
			fprintf(stderr,
				"Unknown protocol '%s'. Only 'http' is supported.\n",
				protocol);
			exit(1);
		}
	}
	// p points to the beginning of the hostname
	*hostname = p;
	// scan for the end of the hostname
	// by looking for the first colon,slash,or hash
	while (*p && *p != ':' && *p != '/' && *p != '#')
		++p;
	// a default port number of 80
	*port = "80";
	// check whether a port number was found
	if (*p == ':') {
		*p++ = 0;
		*port = p;
	}
	while (*p && *p != '/' && *p != '#')
		++p;

	// p pints to the document path
	*path = p;
	if (*p == '/') {
		*path = p + 1;
	}
	*p = 0;

	// attemps to find a hash
	while (*p && *p != '#')
		++p;
	if (*p == '#')
		*p = 0; // overwritten with a terminating null character

	printf("hostname: %s\n", *hostname);
	printf("port: %s\n", *port);
	printf("path: %s\n", *path);
}

void send_request(SOCKET s, char* hostname, char* port, char* path) {
	// to store the HTTP request
	char buffer[2048];

	// write to the buffer until the HTTP request is complete
	sprintf(buffer, "GET /%s HTTP/1.1\r\n", path);
	sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
	sprintf(buffer + strlen(buffer), "Connection: close\r\n");
	sprintf(buffer + strlen(buffer), "User-Agent: honpwc web_get 1.0\r\n");
	// The HTTP request ends with a blank line
	sprintf(buffer + strlen(buffer), "\r\n");

	send(s, buffer, strlen(buffer), 0);
	printf("Sent Headers:\n%s", buffer);
}

SOCKET connect_to_host(char* hostname, char* port) {
	printf("Configuring remote address...\n");
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	struct addrinfo* peer_address;
	if (getaddrinfo(hostname, port, &hints, &peer_address)) {
		fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
		exit(1);
	}

	printf("Remote address is: ");
	char address_buffer[100];
	char service_buffer[100];
	// used to print out the server IP address for debugging purposes
	getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
		address_buffer, sizeof(address_buffer),
		service_buffer, sizeof(service_buffer),
		NI_NUMERICHOST);
	printf("%s %s\n", address_buffer, service_buffer);

	printf("Creating socket...\n");
	SOCKET server;
	server = socket(peer_address->ai_family,
		peer_address->ai_socktype, peer_address->ai_protocol);
	if (!ISVALIDSOCKET(server)) {
		fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
		exit(1);
	}

	printf("Connecting...\n");
	if (connect(server,
		peer_address->ai_addr, peer_address->ai_addrlen)) {
		fprintf(stderr, "connect() failed (%d)\n", GETSOCKETERRNO());
		exit(1);
	}
	freeaddrinfo(peer_address);

	printf("Connected.\n\n");

	// returns the created socket
	return server;
}

