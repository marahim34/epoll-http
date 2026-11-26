/* Unit tests for the request-line parser. No network involved. */
#include "../src/http.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,      \
                    __LINE__, #cond);                                   \
            failures++;                                                 \
        }                                                               \
    } while (0)

static int parse(const char *s, struct http_request *req)
{
    return http_parse_request_line(s, strlen(s), req);
}

static void test_valid_requests(void)
{
    struct http_request req;

    CHECK(parse("GET / HTTP/1.1\r\n", &req) == 0);
    CHECK(strcmp(req.method, "GET") == 0);
    CHECK(strcmp(req.path, "/") == 0);
    CHECK(req.version_minor == 1);

    CHECK(parse("POST /api/items?id=7 HTTP/1.0\r\nHost: x\r\n\r\n", &req) == 0);
    CHECK(strcmp(req.method, "POST") == 0);
    CHECK(strcmp(req.path, "/api/items?id=7") == 0);
    CHECK(req.version_minor == 0);
}

static void test_malformed_requests(void)
{
    struct http_request req;
    const char *bad[] = {
        "",                          /* empty */
        "GET\r\n",                   /* no path */
        "GET / \r\n",                /* no version */
        "GET  / HTTP/1.1\r\n",       /* double space: empty path token */
        "GET path HTTP/1.1\r\n",     /* path must start with / */
        "GET / HTTP/2.0\r\n",        /* unsupported version */
        "GET / HTTP/1.2\r\n",        /* unsupported minor version */
        "GET / HTTP/1.1",            /* no CRLF */
        "GET / HTTP/1.1\n",          /* bare LF */
        "GE\tT / HTTP/1.1\r\n",      /* control character in method */
        "GET /a\x01 HTTP/1.1\r\n",   /* control character in path */
        "GET / HTTP/1.",             /* truncated version */
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (parse(bad[i], &req) != -1) {
            fprintf(stderr, "accepted malformed request #%zu\n", i);
            failures++;
        }
    }
}

static void test_length_limits(void)
{
    struct http_request req;
    char line[4096];

    /* A method longer than the buffer must be rejected, not overflow it. */
    memset(line, 'A', 100);
    strcpy(line + 100, " / HTTP/1.1\r\n");
    CHECK(parse(line, &req) == -1);

    /* The longest path that fits is accepted; one byte more is rejected. */
    line[0] = 'G'; line[1] = 'E'; line[2] = 'T'; line[3] = ' '; line[4] = '/';
    memset(line + 5, 'a', HTTP_MAX_PATH - 2);
    strcpy(line + 5 + HTTP_MAX_PATH - 2, " HTTP/1.1\r\n");
    CHECK(parse(line, &req) == 0);
    CHECK(strlen(req.path) == HTTP_MAX_PATH - 1);

    memset(line + 5, 'a', HTTP_MAX_PATH - 1);
    strcpy(line + 5 + HTTP_MAX_PATH - 1, " HTTP/1.1\r\n");
    CHECK(parse(line, &req) == -1);
}

static void test_not_nul_terminated(void)
{
    /* The parser must respect len and never read past it. */
    struct http_request req;
    const char full[] = "GET / HTTP/1.1\r\n";
    for (size_t len = 0; len < sizeof full - 1; len++)
        CHECK(http_parse_request_line(full, len, &req) == -1);
    CHECK(http_parse_request_line(full, sizeof full - 1, &req) == 0);
}

static void test_find_header_end(void)
{
    const char *done = "GET / HTTP/1.1\r\nHost: x\r\n\r\nbody";
    const char *end = http_find_header_end(done, strlen(done));
    CHECK(end != NULL && strcmp(end, "body") == 0);

    const char *partial = "GET / HTTP/1.1\r\nHost: x\r\n";
    CHECK(http_find_header_end(partial, strlen(partial)) == NULL);
    CHECK(http_find_header_end("", 0) == NULL);
    CHECK(http_find_header_end("\r\n\r", 3) == NULL);
}

int main(void)
{
    test_valid_requests();
    test_malformed_requests();
    test_length_limits();
    test_not_nul_terminated();
    test_find_header_end();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all parser tests passed\n");
    return 0;
}
