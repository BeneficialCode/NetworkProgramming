#include <sys/types.h>

#include <event2/event-config.h>

#include <sys/stat.h>
#ifndef _WIN32
#include <sys/queue.h>
#include <unistd.h>
#include <sys/time.h>
#else
#include <winsock2.h>
#include <windows.h>
#endif
#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <event2/event.h>
#include <event2/thread.h>

// Need to link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"Iphlpapi.lib")

int called = 0;

HANDLE g_hEvent = nullptr;

static void
signal_cb(evutil_socket_t fd, short event, void* arg)
{
	struct event* signal = (struct event*)arg;

	printf("signal_cb: got signal %d\n", event_get_signal(signal));

	event_del(signal);
}

struct event_base* base;
struct event* signal_int = NULL;

DWORD WINAPI thread_fn(void* arg) {
	
	
	int ret = 0;
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);

	(void)WSAStartup(wVersionRequested, &wsaData);
#endif

	evthread_use_windows_threads();

	/* Initialize the event library */
	base = event_base_new();
	if (!base) {
		ret = 1;
		goto out;
	}

	/* Initialize one event */
	signal_int = evsignal_new(base, SIGINT, signal_cb, event_self_cbarg());
	if (!signal_int) {
		ret = 2;
		goto out;
	}
	event_add(signal_int, NULL);

	SetEvent(g_hEvent);

	event_base_dispatch(base);

	SetEvent(g_hEvent);
out:
	if (signal_int)
		event_free(signal_int);
	if (base)
		event_base_free(base);
	return ret;
}

int main(int argc, char** argv){
	HANDLE hThread = ::CreateThread(nullptr, 0, thread_fn, nullptr, 0, nullptr);
	::CloseHandle(hThread);

	g_hEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	
	::WaitForSingleObject(g_hEvent, INFINITE);
	
	// https://blog.csdn.net/cs_sword2000/article/details/108124304?spm=1001.2101.3001.6650.9&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-9-108124304-blog-38556059.pc_relevant_3mothn_strategy_recovery&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7EBlogCommendFromBaidu%7ERate-9-108124304-blog-38556059.pc_relevant_3mothn_strategy_recovery&utm_relevant_index=10
	event_base_loopbreak(base);

	::WaitForSingleObject(g_hEvent, INFINITE);

	system("pause");

	return 0;
}
