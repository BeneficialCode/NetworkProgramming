#pragma once

#include <stdint.h>
#include "Socks5Common.h"

// Socks协议版本
#define SVERSION			(0x05)
// 不需要认证
#define METHOD_NOAUTH		(0x00)
// 不支持的认证方法
#define METHOD_UNACCEPTABLE (0xFF)

// 连接的类型
#define SOCKS5_CMD_CONNECT			(0x01)
#define SOCKS5_CMD_BIND				(0x02)
#define SOCKS5_CMD_UDP_ASSOCIATE	(0x03)

// 地址类型
#define SOCKS5_ATYP_IPV4	(0x01)
#define SOCKS5_ATYP_DOMAIN	(0x03)
#define SOCKS5_ATYP_IPV6	(0x04)

// 状态码，表示此次连接的状态
#define SOCKS5_REP_SUCCEEDED			(0x00)
#define SOCKS5_REP_GENERAL				(0x01)
#define SOCKS5_REP_CONN_DISALLOWED		(0x02)
#define SOCKS5_REP_NETWORK_UNREACHABLE	(0x03)
#define SOCKS5_REP_HOST_UNREACHABLE		(0x04)
#define SOCKS5_REP_CONN_REFUSED			(0x05)
#define SOCKS5_REP_TTL_EXPIRED			(0x06)

#pragma pack(push,1)

typedef struct _METHOD_SELSCT_REQUEST {
	unsigned char ver;
	unsigned char nmethods;
	unsigned char methods[0];
}METHOD_SELECT_REQUEST, * PMETHOD_SELECT_REQUEST;

typedef struct _METHOD_SELECT_RESPONSE {
	unsigned char ver;
	unsigned char method;
}METHOD_SELECT_RESPONSE,*PMETHOD_SELECT_RESPONSE;

// 用户认证
typedef struct _AUTH_RESPONSE {
	char version;
	char name_len;
	char name[255];
	char pwd_len;
	char pwd[255];
}AUTH_RESPONSE,*PAUTH_RESPONSE;

typedef struct _SOCKS5_REQUEST {
	unsigned char ver;
	unsigned char cmd;
	unsigned char reserved;
	unsigned char atyp;
}SOCKS5_REQUEST,*PSOCKS5_REQUEST;

typedef struct _SOCKS5_RESPONSE {
	unsigned char ver;
	unsigned char reply;
	unsigned char reserved;
	unsigned char atyp;
}SOCKS5_RESPONSE,*PSOCKS5_RESPONSE;

#pragma pack(pop)

// Select auth method
// return 0 if success,-1 if failed
int SelectMethod(int sock);

// Parse command, and try to connect real server
// return 0 if success, -1 if failed
int ParseCommand(int sock);

int ForwardData(int sock, int real_server_sock);

DWORD WINAPI Socks5(void* client_sock);

