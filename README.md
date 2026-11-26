# epoll-http

A small HTTP/1.1 server in C for Linux, built step by step: from a blocking
one-client-at-a-time server to a non-blocking `epoll` event loop, measured
against nginx along the way.

The goal is to understand what a production web server does underneath —
sockets, partial reads and writes, event loops, keep-alive, back-pressure —
by building one and testing it hard (sanitizers, fuzzing, load tests).

## Roadmap

| Milestone | What it adds | Status |
|---|---|---|
| 1. Blocking server | `socket/bind/listen/accept`, read a full request, parse the request line, `200`/`400`/`404`/`405` | in progress |
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
