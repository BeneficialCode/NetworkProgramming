#include "../common/chap13.h"

#if defined(_WIN32)
#define INPROGRESS WSAEWOULDBLOCK
#else
#define INPROGRESS EINPROCESS
#endif

int main(int argc, char* argv[]) {

#if defined(_WIN32)
	WSADATA d;
	if (WSAStartup(MAKEWORD(2, 2), &d)) {
		fprintf(stderr, "Failed to initialize.\n");
		return 1;
	}
#endif

	if (argc < 3) {
		fprintf(stderr, "usage: connect_timeout hostname port\n");
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

#if defined(_WIN32)
	unsigned long nonblock = 1;
	ioctlsocket(socket_peer, FIONBIO, &nonblock);
#else
	int flags;
	flags = fcntl(socket_peer, F_GETFL, 0);
	fcntl(socket_peer, F_SETFL, flags | O_NONBLOCK);
#endif

	printf("Connecting...\n");
	int ret;
	if ((ret = connect(socket_peer,
		peer_address->ai_addr, peer_address->ai_addrlen))) {
		if (GETSOCKETERRNO() != INPROGRESS) {
			fprintf(stderr, "connect() faile. (%d)\n", GETSOCKETERRNO());
			return 1;
		}
	}

	freeaddrinfo(peer_address);

#if defined(_WIN32)
	nonblock = 0;
	ioctlsocket(socket_peer, FIONBIO, &nonblock);
#else
	fcntl(socket_peer, F_SETFL, flags);
#endif

	if (ret == 0) {
		printf("Already connected.\n");
		printf("Perhaps non-blocking failed?\n");
	}
	else {
		printf("Waiting up to 5 seconds for connection...\n");

		fd_set set;
		FD_ZERO(&set);
		FD_SET(socket_peer, &set);

		struct timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		select(socket_peer + 1, 0, &set, 0, &timeout);
	}

	printf("Testing for connection...\n");
	if (send(socket_peer, "a", 1, 0) < 0) {
		printf("First send() failed. Connection was not successful.\n");
	}
	else {
		printf("First send() succeeded. Connection was successful.\n");
	}

	printf("Closing socket...\n");
	CLOSESOCKET(socket_peer);

#if defined(_WIN32)
	WSACleanup();
#endif

	printf("Finished.\n");
	return 0;
}