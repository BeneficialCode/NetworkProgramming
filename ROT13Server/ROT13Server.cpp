// ROT13Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#else

#endif

#if defined(_WIN32)
#define ISVALIDSOCKET(s) ((s)!=INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#else

#endif

#include <event2/event.h>

#include <stdio.h>
#include <assert.h>

#define MAX_LINE 16384

void do_read(evutil_socket_t fd, short events, void* arg);
void do_write(evutil_socket_t fd, short events, void* arg);

char 
rot13_char(char c) {
    if ((c >= 'a' && c <= 'm') || (c >= 'A' && c <= 'M'))
        return c + 13;
    else if ((c >= 'n' && c <= 'z') || (c >= 'N' && c <= 'Z'))
        return c - 13;
    else
        return c;
}

struct fd_state {
    char buffer[MAX_LINE];
    size_t buffer_used;

    size_t n_written;
    size_t write_upto;

    struct event* read_event;
    struct event* write_event;
};

struct fd_state* alloc_fd_state(struct event_base* base, evutil_socket_t fd) {
    struct fd_state* state = (fd_state*)malloc(sizeof(struct fd_state));
    if (!state)
        return NULL;
    state->read_event = event_new(base, fd, EV_READ | EV_PERSIST, do_read, state);
    if (!state->read_event) {
        free(state);
        return NULL;
    }
    state->write_event =
        event_new(base, fd, EV_WRITE | EV_PERSIST, do_write, state);

    if (!state->write_event) {
        event_free(state->read_event);
        free(state);
        return NULL;
    }

    state->buffer_used = state->n_written = state->write_upto = 0;

    assert(state->write_event);
    return state;
}

void
free_fd_state(struct fd_state* state) {
    event_free(state->read_event);
    event_free(state->write_event);
    free(state);
}

void
do_read(evutil_socket_t fd, short events, void* arg) {
    struct fd_state* state = (fd_state*)arg;
    char buf[1024];
    int i;
    size_t result;
    while (1) {
        assert(state->write_event);
        result = recv(fd, buf, sizeof(buf), 0);
        if (result <= 0)
            break;

        for (i = 0; i < result; ++i) {
            if (state->buffer_used < sizeof(state->buffer))
                state->buffer[state->buffer_used++] = rot13_char(buf[i]);
            if (buf[i] == '\n') {
                assert(state->write_event);
                event_add(state->write_event, NULL);
                state->write_upto = state->buffer_used;
            }
        }
    }

    if (result == 0) {
        free_fd_state(state);
    }
    else if (result < 0) {
        if (errno == EAGAIN) // XXXX use evutil macro
            return;
        perror("recv");
        free_fd_state(state);
    }
}

void
do_write(evutil_socket_t fd, short events, void* arg)
{
    struct fd_state* state = (fd_state*)arg;

    while (state->n_written < state->write_upto) {
        size_t result = send(fd, state->buffer + state->n_written,
            state->write_upto - state->n_written, 0);
        if (result < 0) {
            if (errno == EAGAIN) // XXX use evutil macro
                return;
            free_fd_state(state);
            return;
        }
        assert(result != 0);

        state->n_written += result;
    }

    if (state->n_written == state->buffer_used)
        state->n_written = state->write_upto = state->buffer_used = 1;

    event_del(state->write_event);
}

void
do_accept(evutil_socket_t listener, short event, void* arg)
{
    struct event_base* base = (event_base*)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);
    if (fd < 0) { // XXXX eagain??
        perror("accept");
    }
    else if (fd > FD_SETSIZE) {
        CLOSESOCKET(fd); // XXX replace all closes with EVUTIL_CLOSESOCKET */
    }
    else {
        struct fd_state* state;
        evutil_make_socket_nonblocking(fd);
        state = alloc_fd_state(base, fd);
        assert(state); /*XXX err*/
        assert(state->write_event);
        event_add(state->read_event, NULL);
    }
}

void
run(void) {
    evutil_socket_t listener;
    struct sockaddr_in sin;
    struct event_base* base;
    struct event* listener_event;

    base = event_base_new();
    if (!base)
        return; /*XXXerr*/

    printf("methods: %s\n", event_base_get_method(base));

    int features;
    features = event_base_get_features(base);
    if ((features & EV_FEATURE_ET))
        printf("  Edge-triggered events are supported.");
    if ((features & EV_FEATURE_O1))
        printf("  O(1) event notification is supported.");
    if ((features & EV_FEATURE_FDS))
        printf("  All FD types are supported.");
    printf("\n");

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(40713);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_socket_nonblocking(listener);

    if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return;
    }

    if (listen(listener, 16) < 0) {
        perror("listen");
        return;
    }

    listener_event = event_new(base, listener, EV_READ | EV_PERSIST, do_accept, (void*)base);
    /*XXX check it */
    event_add(listener_event, NULL);

    event_base_dispatch(base);

    event_base_free(base);
}

int main(){
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    const char** pstr = event_get_supported_methods();
    if (nullptr == pstr) {
        printf("event_get_supported methods failed...\n");
        return 1;
    }

    printf("Libevent version: %s\n", event_get_version());

    for (int i = 0; nullptr != pstr[i]; i++) {
        printf("\t%s\t", pstr[i]);
    }


    run();

    WSACleanup();

    return 0;
}


