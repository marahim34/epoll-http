# epoll-http

[![CI](https://github.com/marahim34/epoll-http/actions/workflows/ci.yml/badge.svg)](https://github.com/marahim34/epoll-http/actions/workflows/ci.yml)

A small HTTP/1.1 server in C for Linux, built step by step: from a blocking
one-client-at-a-time server to a non-blocking `epoll` event loop, measured
against nginx along the way.

The goal is to understand what a production web server does underneath —
sockets, partial reads and writes, event loops, keep-alive, back-pressure —
by building one and testing it hard (sanitizers, fuzzing, load tests).

## Roadmap

| Milestone | What it adds | Status |
|---|---|---|
| 1. Blocking server | `socket/bind/listen/accept`, read a full request, parse the request line, `200`/`400`/`404`/`405` | done |
| 2. epoll event loop | Non-blocking sockets, one thread serving many clients, per-connection state machine | planned |
| 3. Keep-alive and static files | Persistent connections, `Content-Length`, serving files with `sendfile` | planned |
| 4. Hardening | Request size limits, timeouts for slow clients, AddressSanitizer + UBSan in CI, libFuzzer on the parser | planned |
| 5. Benchmarks | Throughput and latency vs nginx with `wrk`, written up with graphs | planned |

## Build and run

Requires Linux, `gcc` or `clang`, and `make`.

```sh
make
./epoll-http 8080
curl -i http://localhost:8080/
```

## Tests

```sh
make test       # parser unit tests + end-to-end tests against the running server
make sanitize   # the same, built with AddressSanitizer and UBSan
```

CI runs both with gcc and clang on every push.

## Notes from each milestone

### 1. Blocking server

- **TCP is a byte stream.** One `read()` can return half a request, so the
  server keeps reading until the blank line that ends the headers. The tests
  send a request split across three writes to check this.
- **`write()` can be partial** and has to be looped.
- **Lost error responses.** The end-to-end tests showed that a client sending
  oversized headers never received its `400`. Closing a socket while the
  client's unread bytes are still in the receive buffer makes the kernel send
  a TCP RST, and the client's kernel then discards the response it already
  had. The fix is a lingering close: `shutdown(SHUT_WR)`, drain the remaining
  input for a short time, then `close()`.
- **The limit of this design:** a single slow client blocks every other
  client, because `accept()`, `read()` and the lingering close all block.
  Milestone 2 fixes this with non-blocking sockets and `epoll`.
