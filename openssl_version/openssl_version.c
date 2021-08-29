#include <openssl/ssl.h>

// gcc openssl_version.c -o openssl_version.exe -lcrypto
int main(int argc, char argv[]) {
	printf("OpenSSL version %s\n", OpenSSL_version(SSLEAY_VERSION));
	return 0;
}