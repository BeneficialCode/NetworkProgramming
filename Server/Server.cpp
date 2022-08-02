#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#pragma comment(lib,"ws2_32.lib")
#include "resource.h"

#define ISVALIDSOCKET(s) ((s)!=INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

HINSTANCE g_hInstance;
HWND g_hWinMain;
CRITICAL_SECTION g_cs;

SOCKET g_hListenSocket;

DWORD g_dwThreadCount;
DWORD g_dwFlag;

const DWORD F_STOP = 0x0001;

const DWORD TCP_PORT = 9999;

DWORD WINAPI ListenThread(void* lParam) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo* bind_address;
	getaddrinfo(0, "8080", &hints, &bind_address);

	SOCKET socket_listen;
	socket_listen = socket(bind_address->ai_family,
		bind_address->ai_socktype, bind_address->ai_protocol);
	if (!ISVALIDSOCKET(socket_listen)) {
		return 1;
	}

	if (bind(socket_listen,
		bind_address->ai_addr, bind_address->ai_addrlen)) {
		return 1;
	}
	freeaddrinfo(bind_address);
	
}

BOOL WINAPI ProcDlgMain(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg)
	{
	case WM_INITDIALOG:
		g_hWinMain = hWnd;
		HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(ICO_MAIN));
		SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		InitializeCriticalSection(&g_cs);
		WSADATA d;
		WSAStartup(MAKEWORD(2, 2), &d);
		HANDLE hThread = CreateThread(nullptr, 0, ListenThread, nullptr, 0, nullptr);
		CloseHandle(hThread);
		break;
	case WM_CLOSE:
		closesocket(g_hListenSocket);
		WSACleanup();
		while (g_dwThreadCount) {};
		DeleteCriticalSection(&g_cs);
		EndDialog(hWnd, NULL);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR lpstrCmdLine, int nCmdShow) {
	g_hInstance = hInstance;
	DialogBoxParam(hInstance, MAKEINTRESOURCE(DLG_MAIN), nullptr, ProcDlgMain, 0);
	return 0;
}