#pragma once

#define VERSION (0x05)
#define CONNECT (0x01)
#define IPV4	(0x01)
#define DOMAIN	(0x03)
#define IPV6	(0x04)

#define AUTH_CODE (0x00)

// 服务端响应
typedef struct _METHOD_SELECT_RESPONSE {
	char version;
	char select_method;
}METHOD_SELECT_RESPONSE,*PMETHOD_SELECT_RESPONSE;

// 服务器请求
typedef struct _METHOD_SELSCT_REQUEST {
	char version;
	char number_method;
	char methods[255];
}METHOD_SELECT_REQUEST,*PMETHOD_SELECT_REQUEST;

// 用户认证
typedef struct _AUTH_RESPONSE {
	char version;
	char name_len;
	char name[255];
	char pwd_len;
	char pwd[255];
}AUTH_RESPONSE,*PAUTH_RESPONSE;

typedef struct _SOCKS5_REQUEST {
	char version;
	char cmd;
	char reserved;
	char address_type;
}SOCKS5_REQUEST,*PSOCKS5_REQUEST;

typedef struct _SOCKS5_RESPONSE {
	char version;
	char reply;
	char reserved;
	char address_type;
	char address_port[1];
}SOCKS5_RESPONSE,*PSOCKS5_RESPONSE;

// Select auth method
// return 0 if success,-1 if failed
int SelectMethod(int sock);

// Parse command, and try to connect real server
// return 0 if success, -1 if failed
int ParseCommand(int sock);

int ForwardData(int sock, int real_server_sock);

DWORD WINAPI Socks5(void* client_sock);