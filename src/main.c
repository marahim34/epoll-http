/*
 * Milestone 1: a blocking HTTP server that serves one client at a time.
 *
 * Deliberately simple: accept() blocks until a client connects, then read()
 * blocks until that client sends its request. While one client is being
 * served, nobody else is. Milestone 2 replaces this with an epoll event loop.
 */
#include "http.h"

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Requests whose headers do not fit in this buffer are rejected with 400. */
#define REQUEST_BUF_SIZE 8192

/* write() may send fewer bytes than asked; keep going until all are sent. */
static int write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        buf += n;
        len -= (size_t)n;
    }
    return 0;
}

static void send_response(int fd, int status, const char *reason, const char *body)
{
    char head[256];
    size_t body_len = strlen(body);
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: text/plain; charset=utf-8\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     status, reason, body_len);
    if (write_all(fd, head, (size_t)n) == 0)
        write_all(fd, body, body_len);
}

/*
 * TCP is a byte stream, not a sequence of messages: one read() can return
 * half a request, or a request and a half. Keep reading until the blank line
 * that ends the headers arrives. Returns the number of bytes read, or -1.
 */
static ssize_t read_request(int fd, char *buf, size_t cap)
{
    size_t used = 0;
    while (used < cap) {
        ssize_t n = read(fd, buf + used, cap - used);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1; /* client closed before finishing its request */
        used += (size_t)n;
        if (http_find_header_end(buf, used))
            return (ssize_t)used;
    }
    return -1; /* headers too large */
}

static void handle_client(int fd)
{
    char buf[REQUEST_BUF_SIZE];
    struct http_request req;

    ssize_t len = read_request(fd, buf, sizeof buf);
    if (len < 0 || http_parse_request_line(buf, (size_t)len, &req) != 0) {
        send_response(fd, 400, "Bad Request", "bad request\n");
        return;
    }
    if (strcmp(req.method, "GET") != 0) {
        send_response(fd, 405, "Method Not Allowed", "method not allowed\n");
        return;
    }
    if (strcmp(req.path, "/") != 0) {
        send_response(fd, 404, "Not Found", "not found\n");
        return;
    }
    send_response(fd, 200, "OK", "hello from epoll-http\n");
}

static int open_listener(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    /* Without this, restarting the server fails with EADDRINUSE for about a
     * minute while the previous socket sits in TIME_WAIT. */
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((unsigned short)port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(fd, SOMAXCONN) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 8080;
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        return 2;
    }

    /* A client that disconnects mid-response would otherwise raise SIGPIPE
     * on our next write() and kill the whole server. */
    signal(SIGPIPE, SIG_IGN);

    int listener = open_listener(port);
    if (listener < 0) {
        perror("listen");
        return 1;
    }
    printf("listening on port %d\n", port);
    fflush(stdout);

    for (;;) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }
        handle_client(client);
        close(client);
    }
}
