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

#include "server.h"
#include "phase.h"
#include "common.h"
#include "location.h"

#define MAX_BACKLOG     20
#define MAX_EVENTS      20
#define BUF_PAGE_SIZE   4096
#define INIT_PAGES      1

static int _server_init(server_t *server);
static int _server_run(server_t *server);
static int _server_stop(server_t *server);

static int _accept_conn(server_t *server);
static int _close_conn(server_t *server, connection_t *conn);
static int _dispatch_conn_event(server_t *server, struct epoll_event *ev);
static int _run_conn_phases(server_t *server, connection_t *conn);

static int _set_nonblocking(int fd);


server_t* server_create(unsigned short port) {
    server_t    *server;
    server = (server_t*) calloc(1, sizeof(server_t));
    if (server == NULL) {
        perror("Error allocating memory for server");
        return NULL;
    }
    server->port = port;
    return server;
}

int server_destroy(server_t *server) {
    // --------- Destroy the site object
    site_destroy(server->site);

    // --------- Destroy server -------------------
    free(server);
    return 0;
}

int  server_main(server_t *server) {
    int rc = 0;

    if (_server_init(server) == -1) {
        perror("Error detected during server initialization");
        return -1;
    }

    server->_state = SERV_STAT_RUNNING;

    for(;;) {
        switch (server->_state) {
            case SERV_STAT_RUNNING:
                printf("Server starts running on 0.0.0.0:%d\n", server->port);
                if (_server_run(server) == -1) {
                    perror("Error detected during server running");
                    server->_state = SERV_STAT_STOPPING;
                }
                break;

            case SERV_STAT_STOPPING:
                if (_server_stop(server) == -1) {
                    perror("Error detected when stopping server");
                }
                server->_state = SERV_STAT_COMPLETE;
                break;

            case SERV_STAT_COMPLETE:
                return rc;

            default:
                fprintf(stderr, "Invalid state: %d", server->_state);
                return -1;
        }

    }
}


static int  _server_init(server_t *server) {
    int                     listen_fd, epoll_fd;
    struct sockaddr_in      addr;
    struct epoll_event      ev;

    // ---------- Create and bind listen socket fd --------------
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("Error creating socket");
        return -1;
    }

    bzero(&addr, sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(server->port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
        perror("Error binding address");
        return -1;
    }

    // ------------ Start listening ------------------------------
    if (listen(listen_fd, MAX_BACKLOG) == -1) {
        perror("Error listening");
        return -1;
    }
    server->_listen_fd = listen_fd;

    // ------------ Init epoll ----------------------------------
    epoll_fd = epoll_create(10);
    if (epoll_fd == -1) {
        perror("Error initializing epoll");
        return -1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("Error adding listen socket fd to epoll");
        return -1;
    }

    server->_epoll_fd = epoll_fd;
    
    return 0;
}


static int _server_run(server_t *server) {
    int                 listen_fd, epoll_fd, nfds, i;
    struct epoll_event  events[MAX_EVENTS];

    epoll_fd = server->_epoll_fd;
    listen_fd = server->_listen_fd;

    for(;;) {
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("Error detected during epoll waiting");
            return -1;
        }

        for (i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                _accept_conn(server);
            } else {
                _dispatch_conn_event(server, &events[i]);
            }
        }

    }
}


static int _server_stop(server_t *server) {
    /*TODO: Implement me*/
    return 0;
}


static int _set_nonblocking(int sockfd) {
    int opts;
    opts = fcntl(sockfd, F_GETFL);
    if (opts < 0) 
        return -1;
    opts |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, opts) < 0)
        return -1;
    return 0;
}


static int _accept_conn(server_t *server) {
    int                 conn_fd;
    socklen_t           addr_len;
    connection_t        *conn;
    struct epoll_event  ev;

    // -------- Allocating memory for the new connection --------
    conn = (connection_t*) calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        perror("Error allocating memory for new connection");
        return -1;
    }

    // -------- Accepting connection ----------------------------
    printf("Accepting new connection...\n");
    addr_len = sizeof(struct sockaddr_in);
    conn_fd = accept(server->_listen_fd, (struct sockaddr*) &conn->remo_addr, &addr_len);
    printf("Connection fd: %d...\n", conn_fd);
    if (conn_fd == -1) {
        perror("Error accepting new connection");
        return -1;
    }

    // -------- Adding connection fd to epoll ------------------
    printf("Adding new connection to epoll...\n");
    if (_set_nonblocking(conn_fd) == -1) {
        perror("Error configuration connection to non-blocking");
        return -1;
    }

    ev.data.ptr = conn;
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (epoll_ctl(server->_epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1) {
        perror("Error adding new connection to epoll");
        close(conn_fd);
        free(conn);
        return -1;
    }

    // -------- Init members -------------------
    conn->next = NULL;
    conn->prev = NULL;
    conn->sock_fd = conn_fd;
    conn->server = server;

    // -------- Allocating read and write buffers -----
    conn->read_buf_q = buf_create(BUF_PAGE_SIZE, INIT_PAGES);
    conn->write_buf_q = buf_create(BUF_PAGE_SIZE, INIT_PAGES);

    // Add the connection to the server's connection pool, using
    // fd as index.
    if (server->_conn_head == NULL) {
        server->_conn_head = conn;
        server->_conn_tail = conn;
    } else {
        server->_conn_tail->next = conn;
        conn->prev = server->_conn_tail;
        server->_conn_tail = conn;
    }

    // Init state
    conn->phase_ctx.current_phase = PHASE_IDLE;
    conn->phase_ctx.data = NULL;

    printf("Ready to serve the client...\n");

    return 0;
}


static int _close_conn(server_t *server, connection_t *conn) {
    printf("Closing connection[fd: %d]\n", conn->sock_fd);

    if(epoll_ctl(server->_epoll_fd, EPOLL_CTL_DEL, conn->sock_fd, NULL)) {
        perror("Error removing fd from epoll");
    }

    close(conn->sock_fd);

    if (conn->prev != NULL)
        conn->prev->next = conn->next;
    if (conn->next != NULL) 
        conn->next->prev = conn->prev;

    buf_destroy(conn->read_buf_q);
    buf_destroy(conn->write_buf_q);
    free(conn);
    return 0;
}


// Currently, read until EOF or EAGAIN or error.
int conn_read(connection_t *conn) {
    ssize_t     sz_read = 0;
    void        *dest_ptr = NULL;
    size_t      buf_cap = 0;
    buf_page_t  *pg = NULL;

    assert (conn->read_buf_q != NULL);
    pg = buf_get_tail(conn->read_buf_q);
    if (pg == NULL) {
        pg = buf_alloc_new(conn->read_buf_q);
    } else {
        buf_compact_page(pg);
    }

    buf_cap = pg->page_size - pg->data_size - pg->data_offset;
    dest_ptr = pg->data + pg->data_offset + pg->data_size;

    for(;;) {
        if (buf_cap <= 0) {
            pg = buf_alloc_new(conn->read_buf_q);
            dest_ptr = pg->data;
            buf_cap = pg->page_size;
        }
        sz_read = read(conn->sock_fd, dest_ptr, buf_cap);
        if (sz_read > 0) {
            pg->data_size += sz_read;
            dest_ptr += sz_read;
            buf_cap -= sz_read;
        } else if (sz_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            perror("Error reading from socket");
            return -1;
        } else {
            fprintf(stderr, "Read EOF on connection %d\n", conn->sock_fd);
            return 0;
        }
    }

    return 0;
}


int conn_write_to_buffer(connection_t *conn, const void* data, size_t data_len) {
    buf_page_t      *pg;
    size_t          cap, sz_to_cpy;
    void            *src_ptr;
    void            *dest_ptr;

    src_ptr = data;

    pg = buf_get_tail(conn->write_buf_q);
    if (pg == NULL) {
        pg = buf_alloc_new(conn->write_buf_q);
        if (pg == NULL) {
            fprintf(stderr, "Error allocating new buffer page");
            return -1;
        }
    } else {
        buf_compact_page(pg);
    }

    dest_ptr = pg->data + pg->data_size;
    cap = pg->page_size - pg->data_size;

    while (data_len > 0) {
        sz_to_cpy = data_len < cap ? data_len : cap;
        memcpy(dest_ptr, src_ptr, sz_to_cpy);
        cap -= sz_to_cpy;
        data_len -= sz_to_cpy;
        src_ptr += sz_to_cpy;
        dest_ptr += sz_to_cpy;
        pg->data_size += sz_to_cpy;
        if (cap <= 0 && data_len > 0) {
            pg = buf_alloc_new(conn->write_buf_q);
            if (pg == NULL) {
                fprintf(stderr, "Error allocating new buffer page");
                return -1;
            }
            dest_ptr = pg->data;
            cap = pg->page_size;
        }
    }

    return 0;
}


int conn_do_write(connection_t *conn) {
    ssize_t     sz_written = 0;
    size_t      sz_to_write = 0;
    buf_page_t  *pg = NULL;
    void        *offset;
    
    pg = buf_get_head(conn->write_buf_q);
    if (pg == NULL)
        return 0;

    offset = pg->data + pg->data_offset;
    sz_to_write = pg->data_size;

    while (sz_to_write > 0) {
        sz_written = write(conn->sock_fd, offset, sz_to_write);

        if (sz_written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            else 
                return -1;
        } else if (sz_written == 0) {
            return 0;
        }

        offset += sz_written;
        sz_to_write -= sz_written;
        pg->data_offset += sz_written;
        pg->data_size -= sz_written;

        if (sz_to_write <= 0) {
            // Move to next page
            buf_remove_head(conn->write_buf_q);
            pg = buf_get_head(conn->write_buf_q);
            if (pg != NULL) {
                offset = pg->data + pg->data_offset;
                sz_to_write = pg->data_size;
            }
        }

    }

    return 0;
}


int conn_toggle_write_event(connection_t *conn, unsigned short on) {
}

#define EPOLL_CLOSE_EVENTS (EPOLLRDHUP | EPOLLHUP | EPOLLERR)
#define CONN_BUFF_SIZE      2048

static int _dispatch_conn_event(server_t *server, struct epoll_event *ev) {
    connection_t    *conn;

    conn = (connection_t*) ev->data.ptr;
    if (conn == NULL) {
        printf("Invalid event: connection ptr is NULL\n");
        return -1;
    }

    printf("New event[fd:%d, event:%d]\n", conn->sock_fd, ev->events);

    if (EPOLL_CLOSE_EVENTS & ev->events) {
        printf("Client closed connection or error detected\n");
        return _close_conn(server, conn);
    }

    // read event and write event should not be raised at the same time.
    assert( (EPOLLIN & ev->events) ^ (EPOLLOUT & ev->events) );

    return _run_conn_phases(server, conn);
}


#define CHANGE_PHASE(conn, phase) { \
    (conn)->phase_ctx.current_phase = (phase); \
    (conn)->phase_ctx.data = NULL; \
}

static int _run_conn_phases(server_t *server, connection_t *conn) {
    int             rc;
    conn_phase      cur_phase;
    
    for(;;) {
        cur_phase = conn->phase_ctx.current_phase;
        rc = std_phases[cur_phase].handle(server, conn);

        printf("Finished phase: %d, return code: %d\n", cur_phase, rc);

        switch (rc) {
            case STATUS_COMPLETE:
                CHANGE_PHASE(conn, std_phases[cur_phase].next_phase);
                break;

            case STATUS_ERROR:
                CHANGE_PHASE(conn, PHASE_ERROR);
                break;

            case STATUS_CONTINUE:
                goto phase_end;
        }

    }

phase_end:
    // Finished running all the phases
    if (PHASE_NULL == conn->phase_ctx.current_phase) {
        if (conn->keep_alive) {
            // Change phase to IDLE, waiting for next request
            CHANGE_PHASE(conn, PHASE_IDLE);
        } else {
            // Close the connection otherwise
            _close_conn(server, conn);
        }
    }

    return 0;
}


