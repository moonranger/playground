#include "ioloop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#define MAX_CALLBACKS 20

struct _callback {
    callback_handler    callback;
    void                *args;
};

struct _io_callback {
    io_handler      callback;
    void            *args;
};

struct _ioloop {
    int                 epoll_fd;
    int                 state;
    struct _io_callback *handlers;
    struct _callback    callbacks[MAX_CALLBACKS];
    int                 callback_num;
};

enum IOLOOP_STATES {
    INITIALIZED = 0,
    RUNNING = 1,
    STOPPED = 2
};


ioloop_t *ioloop_create(unsigned int maxfds) {
    ioloop_t                 *loop = NULL;
    int                      epoll_fd;
    struct _io_callback      *handlers = NULL;
    
    loop = (ioloop_t*) calloc (1, sizeof(ioloop_t));
    if (loop == NULL) {
        perror("Could not allocate memory for IO loop");
        return NULL;
    }
    bzero(loop, sizeof(ioloop_t));


    handlers = (struct _io_callback*) calloc(maxfds, sizeof(struct _io_callback));
    if (handlers == NULL) {
        perror("Could not allocate memory for IO handlers");
        return NULL;
    }

    bzero(handlers, maxfds * sizeof(struct _io_callback));


    epoll_fd = epoll_create(maxfds);
    if (epoll_fd == -1) {
        perror("Error initializing epoll");
        return NULL;
    }

    loop->handlers = handlers;
    loop->epoll_fd = epoll_fd;
    loop->state = INITIALIZED;
    loop->callback_num = 0;
    return loop;
}


int ioloop_destroy(ioloop_t *loop) {
    free(loop->handlers);
    free(loop);
    return 0;
}


int ioloop_update_handler(ioloop_t *loop, int fd, unsigned int events, io_handler handler, void *args) {
    struct epoll_event     ev;

    if (handler == NULL) {
        fprintf(stderr, "Handler should not be NULL!");
        return -1;
    }

    ev.data.fd = fd;
    ev.events = events | EPOLLET;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("Error adding fd to epoll");
        return -1;
    }

    loop->handlers[fd].callback = handler;
    loop->handlers[fd].args = args;
    return 0;
}


io_handler  ioloop_remove_handler(ioloop_t *loop, int fd) {
    int         res;
    io_handler  handler;
    printf("Removing handler for fd %d\n", fd);
    handler = loop->handlers[fd].callback;
    loop->handlers[fd].callback = NULL;
    loop->handlers[fd].args = NULL;
    res = epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if (res < 0) {
        perror("Error removing fd from epoll");
    }
    return handler;
}


#define MAX_EVENTS    20
#define EPOLL_TIMEOUT 100

int ioloop_start(ioloop_t *loop) {
    struct epoll_event  events[MAX_EVENTS];
    int                 epoll_fd, nfds, i, fd;
    io_handler          handler;
    void                *args;

    if (loop->state != INITIALIZED) {
        fprintf(stderr, "Could not restart an IO loop");
        return -1;
    }
    epoll_fd = loop->epoll_fd;
    loop->state = RUNNING;
    while (loop->state == RUNNING) {
        // Handle callbacks
        if (loop->callback_num > 0) {
            for (i = 0; i < loop->callback_num; i++) {
                loop->callbacks[i].callback(loop, loop->callbacks[i].args);
            }
            loop->callback_num = 0;
        }

        // Wait for events
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (nfds == -1) {
            perror("epoll_wait");
            continue;
        }

        // Handle events
        for (i = 0; i < nfds; i++) {
            fd = events[i].data.fd;
            printf("Event %d triggered on fd %d\n", events[i].events, fd);
            handler = loop->handlers[fd].callback;
            args = loop->handlers[fd].args;
            if (handler == NULL) {
                printf("Event triggered with a NULL handler on fd %d\n", fd);
                continue;
            }
            handler(loop, fd, events[i].events, args);
        }
    }

    close(epoll_fd);
    //TODO Other cleanup work here.
    return 0;
}


int ioloop_stop(ioloop_t *loop) {
    loop->state = STOPPED;
    return 0;
}


int ioloop_add_callback(ioloop_t *loop, callback_handler handler, void *args) {
    int n = loop->callback_num;
    loop->callbacks[n].callback = handler;
    loop->callbacks[n].args = args;
    loop->callback_num ++;
    return 0;
}
