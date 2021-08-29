#include "chap11.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ssh_auth hostname port user\n");
        return 1;
    }
    const char* hostname = argv[1];
    int port = atol(argv[2]);
    const char* user = argv[3];


    ssh_session ssh = ssh_new();
    if (!ssh) {
        fprintf(stderr, "ssh_new() failed.\n");
        return 1;
    }

    ssh_options_set(ssh, SSH_OPTIONS_HOST, hostname);
    ssh_options_set(ssh, SSH_OPTIONS_PORT, &port);
    ssh_options_set(ssh, SSH_OPTIONS_USER, user);

    int ret = ssh_connect(ssh);
    if (ret != SSH_OK) {
        fprintf(stderr, "ssh_connect() failed.\n%s\n", ssh_get_error(ssh));
        return -1;
    }

    printf("Connected to %s on port %d.\n", hostname, port);

    printf("Banner:\n%s\n", ssh_get_serverbanner(ssh));



    ssh_key key;
    if (ssh_get_server_publickey(ssh, &key) != SSH_OK) {
        fprintf(stderr, "ssh_get_server_publickey() failed.\n%s\n",
            ssh_get_error(ssh));
        return -1;
    }

    unsigned char* hash;
    size_t hash_len;
    if (ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA1,
        &hash, &hash_len) != SSH_OK) {
        fprintf(stderr, "ssh_get_publickey_hash() failed.\n%s\n",
            ssh_get_error(ssh));
        return -1;
    }

    printf("Host public key hash:\n");
    ssh_print_hash(SSH_PUBLICKEY_HASH_SHA1, hash, hash_len);

    ssh_clean_pubkey_hash(&hash);
    ssh_key_free(key);




    printf("Checking ssh_session_is_known_server()\n");
    enum ssh_known_hosts_e known = ssh_session_is_known_server(ssh);
    switch (known) {
    case SSH_KNOWN_HOSTS_OK: printf("Host Known.\n"); break;

    case SSH_KNOWN_HOSTS_CHANGED: printf("Host Changed.\n"); break;
    case SSH_KNOWN_HOSTS_OTHER: printf("Host Other.\n"); break;
    case SSH_KNOWN_HOSTS_UNKNOWN: printf("Host Unknown.\n"); break;
    case SSH_KNOWN_HOSTS_NOT_FOUND: printf("No host file.\n"); break;

    case SSH_KNOWN_HOSTS_ERROR:
        printf("Host error. %s\n", ssh_get_error(ssh)); return 1;

    default: printf("Error. Known: %d\n", known); return 1;
    }



    if (known == SSH_KNOWN_HOSTS_CHANGED ||
        known == SSH_KNOWN_HOSTS_OTHER ||
        known == SSH_KNOWN_HOSTS_UNKNOWN ||
        known == SSH_KNOWN_HOSTS_NOT_FOUND) {
        printf("Do you want to accept and remember this host? Y/N\n");
        char answer[10];
        fgets(answer, sizeof(answer), stdin);
        if (answer[0] != 'Y' && answer[0] != 'y') {
            return 0;
        }

        ssh_session_update_known_hosts(ssh);
    }

	printf("Password:");
	char password[128];
	// includes the newline character with the input
	fgets(password, sizeof(password), stdin);
	// removing the newline character
	password[strlen(password) - 1] = 0;

	if (ssh_userauth_password(ssh, 0, password) != SSH_AUTH_SUCCESS) {
		fprintf(stderr, "ssh_userauth_password() failed.\n%s\n",
			ssh_get_error(ssh));
		return 0;
	}
	else {
		printf("Authentication successful!\n");
	}

    ssh_channel channel = ssh_channel_new(ssh);
    if (!channel) {
        fprintf(stderr, "ssh_channel_new() failed.\n");
        return 0;
    }

    if (ssh_channel_open_session(channel) != SSH_OK) {
        fprintf(stderr, "ssh_channel_open_session() failed.\n");
        return 0;
    }

    printf("Remote command to execute: ");
    char command[128];
    fgets(command, sizeof(command), stdin);
    command[strlen(command) - 1];

    if (ssh_channel_request_exec(channel, command) != SSH_OK) {
        fprintf(stderr, "ssh_channel_open_session() failed.\n");
        return 1;
    }

    char output[1024];
    int bytes_received;
    while ((bytes_received = ssh_channel_read(channel, output, sizeof(output), 0))) {
        if (bytes_received < 0) {
            fprintf(stderr, "ssh_channel_read() failed.\n");
            return 1;
        }
        printf("%.*s", bytes_received, output);
    }

    printf("\n");


    while ((bytes_received = ssh_channel_read(channel, output, sizeof(output), 0))) {
        if (bytes_received < 0) {
            fprintf(stderr, "ssh_channel_read() failed.\n");
            return 1;
        }
        printf("%.*s", bytes_received, output);
    }

    printf("\n");

    printf("Remote file to download: ");
    char filename[128];
    fgets(filename, sizeof(filename), stdin);
    filename[strlen(filename) - 1] = 0;

    printf("filename: %s\n", filename);
    ssh_scp scp = ssh_scp_new(ssh, SSH_SCP_READ, filename);
    if (!scp) {
        fprintf(stderr, "ssh_scp_new() failed.\n%s\n", ssh_get_error(ssh));
        return 1;
    }

    if (ssh_scp_init(scp) != SSH_OK) {
        fprintf(stderr, "ssh_scp_init() failed.\n%s\n", ssh_get_error(ssh));
        return 1;
    }

    if (ssh_scp_pull_request(scp) != SSH_SCP_REQUEST_NEWFILE) {
        fprintf(stderr, "ssh_scp_pull_request() failed.\n%s\n",
            ssh_get_error(ssh));
        return 1;
    }
    
    int fsize = ssh_scp_request_get_size(scp);
    char* fname = strdup(ssh_scp_request_get_filename(scp));
    int fpermission = ssh_scp_request_get_permissions(scp);

    printf("Downloading file %s (%d bytes,permissions 0%o\n", fname, fsize, fpermission);
    free(fname);

    char* buffer = malloc(fsize);
    if (!buffer) {
        fprintf(stderr, "malloc() failed.\n");
        return 1;
    }

    ssh_scp_accept_request(scp);
    if (ssh_scp_read(scp, buffer, fsize) == SSH_ERROR) {
        fprintf(stderr, "ssh_scp_read() failed.\n%s\n", ssh_get_error(ssh));
        return 1;
    }

    printf("Received %s:\n", filename);
    printf("%.*s\n", fsize, buffer);
    free(buffer);

    if (ssh_scp_pull_request(scp) != SSH_SCP_REQUEST_EOF) {
        fprintf(stderr, "ssh_scp_pull_request() unexpected.\n%s\n",
            ssh_get_error(ssh));
        return 1;
    }
    
    ssh_scp_close(scp);
    ssh_scp_free(scp);

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

	ssh_disconnect(ssh);
	ssh_free(ssh);

	return 0;
}