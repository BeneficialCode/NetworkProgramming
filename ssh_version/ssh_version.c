#include "chap11.h"

int main() {
	// gcc ssh_version.c -o ssh_version -lcrypto -lgen_pass
	printf("libssh verison: %s\n", ssh_version(0));
	return 0;
}