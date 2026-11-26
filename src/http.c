#include "http.h"

#include <string.h>

/* Copy the token that ends at the first `stop` byte into dst (NUL-terminated).
 * Returns the number of bytes consumed including the stop byte, or 0 on error. */
static size_t take_token(const char *buf, size_t len, char stop, char *dst, size_t cap)
{
    size_t i = 0;
    while (i < len && buf[i] != stop) {
        /* Tokens in the request line never contain control characters or spaces. */
        if ((unsigned char)buf[i] <= ' ' || buf[i] == 0x7f)
            return 0;
        if (i + 1 >= cap)
            return 0; /* would not fit with the terminating NUL */
        dst[i] = buf[i];
        i++;
    }
    if (i == 0 || i == len)
        return 0; /* empty token, or stop byte never found */
    dst[i] = '\0';
    return i + 1;
}

int http_parse_request_line(const char *buf, size_t len, struct http_request *req)
{
    size_t n, pos = 0;

    n = take_token(buf, len, ' ', req->method, sizeof req->method);
    if (n == 0)
        return -1;
    pos += n;

    n = take_token(buf + pos, len - pos, ' ', req->path, sizeof req->path);
    if (n == 0 || req->path[0] != '/')
        return -1;
    pos += n;

    /* The version must be exactly "HTTP/1.0" or "HTTP/1.1" followed by CRLF. */
    static const char prefix[] = "HTTP/1.";
    const size_t plen = sizeof prefix - 1;
    if (len - pos < plen + 3 || memcmp(buf + pos, prefix, plen) != 0)
        return -1;
    pos += plen;

    if (buf[pos] != '0' && buf[pos] != '1')
        return -1;
    req->version_minor = buf[pos] - '0';
    pos++;

    if (buf[pos] != '\r' || buf[pos + 1] != '\n')
        return -1;
    return 0;
}

const char *http_find_header_end(const char *buf, size_t len)
{
    for (size_t i = 3; i < len; i++) {
        if (buf[i - 3] == '\r' && buf[i - 2] == '\n' && buf[i - 1] == '\r' && buf[i] == '\n')
            return buf + i + 1;
    }
    return NULL;
}
