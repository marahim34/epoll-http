# epoll-http

[![CI](https://github.com/marahim34/epoll-http/actions/workflows/ci.yml/badge.svg)](https://github.com/marahim34/epoll-http/actions/workflows/ci.yml)

A small HTTP/1.1 server in C for Linux that implements **the same server in
every major Linux concurrency model** — blocking, thread-per-connection,
thread pool, `poll`, `epoll` (level- and edge-triggered) and `io_uring` —
behind one interface, then measures them against each other and against nginx.

The goal is to understand what a production web server does underneath —
sockets, partial reads and writes, event loops, keep-alive, back-pressure,
sandboxing — by building one and testing it hard (sanitizers, fuzzing, load
tests) and tracing it from the kernel with eBPF.

## Roadmap

| Milestone | What it adds | Status |
|---|---|---|
| 1. Blocking server | `socket/bind/listen/accept`, read a full request, parse the request line, `200`/`400`/`404`/`405`, lingering close | done |
| 2. Threads | Thread per connection, then a fixed thread pool with a work queue; why threads stop scaling | next |
| 3. Event loops | `poll()`, then `epoll` level- and edge-triggered; non-blocking sockets and a per-connection state machine | planned |
| 4. HTTP done properly | Keep-alive, pipelining, chunked bodies, static files with zero-copy `sendfile` | planned |
| 5. Hardening | Timeouts, slowloris defense, size limits, libFuzzer on the parser, sanitizers in CI | planned |
| 6. io_uring | A completion-based backend next to the readiness-based ones | planned |
| 7. Sandbox | Drop privileges, seccomp syscall filter, systemd socket activation | planned |
| 8. Observability | `/metrics` endpoint, bpftrace scripts that trace the server from the kernel | planned |
| 9. Benchmarks | Throughput and latency for every I/O model vs nginx with `wrk`, graphs and analysis | planned |
| 10. TLS (stretch) | OpenSSL, then kernel TLS (kTLS) combined with `sendfile` | planned |

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
