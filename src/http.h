#ifndef EPOLL_HTTP_HTTP_H
#define EPOLL_HTTP_HTTP_H

#include <stddef.h>

/* Longest method and path we accept. Anything longer is a 400 (or 414 later). */
#define HTTP_MAX_METHOD 16
#define HTTP_MAX_PATH 1024

struct http_request {
    char method[HTTP_MAX_METHOD];
    char path[HTTP_MAX_PATH];
    int version_minor; /* HTTP/1.<minor>; only 0 and 1 are accepted */
};

/*
 * Parse the request line ("GET /path HTTP/1.1\r\n") at the start of buf.
 * buf does not need to be NUL-terminated; len is the number of valid bytes.
 * Returns 0 on success, -1 if the line is malformed or too long.
 */
int http_parse_request_line(const char *buf, size_t len, struct http_request *req);

/*
 * Return a pointer just past the blank line that ends the headers
 * ("\r\n\r\n"), or NULL if the headers are not complete yet.
 */
const char *http_find_header_end(const char *buf, size_t len);

#endif
