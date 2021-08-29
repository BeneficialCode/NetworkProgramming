#include "chap13.h"
const char* get_error_text();

int main() {

#if defined(_WIN32)
	WSADATA d;
	if (WSAStartup(MAKEWORD(2, 2), &d)) {
		fprintf(stderr, "Failed to initilaize.\n");
		return 1;
	}
#endif

	printf("Calling socket() with invalid parameters.\n");
	socket(0, 0, 0);
	printf("Last error was: %s\n", get_error_text());

	printf("Finished.\n");

	return 0;

}

const char* get_error_text() {

#if defined(_WIN32)
	static char message[256] = { 0 };
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		0, WSAGetLastError(), 0, message, 256, 0);
	char* nl = strstr(message, "\n");
	if (nl)
		*nl = 0;
	return message;
#else
	return strerror(errno);
#endif

}