#!/usr/bin/env bash
# End-to-end tests: start the real server and talk to it over TCP.
set -euo pipefail

BIN=${1:-./epoll-http}
PORT=${PORT:-18080}
failures=0

"$BIN" "$PORT" >/dev/null &
server=$!
trap 'kill $server 2>/dev/null || true' EXIT

# Wait until the server accepts connections.
for _ in $(seq 50); do
    if curl -s -o /dev/null "http://127.0.0.1:$PORT/"; then break; fi
    sleep 0.1
done

expect_status() { # description, expected status, then the raw request
    local desc=$1 want=$2 got
    got=$(printf '%b' "$3" | nc -q 1 127.0.0.1 "$PORT" | head -1 | cut -d' ' -f2)
    if [[ "$got" == "$want" ]]; then
        echo "ok   $desc"
    else
        echo "FAIL $desc: expected $want, got '${got}'"
        failures=$((failures + 1))
    fi
}

expect_status "GET / returns 200"          200 'GET / HTTP/1.1\r\nHost: x\r\n\r\n'
expect_status "unknown path returns 404"   404 'GET /missing HTTP/1.1\r\nHost: x\r\n\r\n'
expect_status "POST returns 405"           405 'POST / HTTP/1.1\r\nHost: x\r\n\r\n'
expect_status "garbage returns 400"        400 'not http at all\r\n\r\n'
expect_status "HTTP/2.0 returns 400"       400 'GET / HTTP/2.0\r\n\r\n'

# A request split across several TCP writes must still be read as one.
got=$( (printf 'GET / HT'; sleep 0.2; printf 'TP/1.1\r\nHo'; sleep 0.2; printf 'st: x\r\n\r\n') \
       | nc -q 1 127.0.0.1 "$PORT" | head -1 | cut -d' ' -f2)
if [[ "$got" == 200 ]]; then echo "ok   request split across writes"; else
    echo "FAIL request split across writes: got '$got'"; failures=$((failures + 1)); fi

# Headers larger than the 8 KiB buffer are rejected instead of overflowing it.
big=$(head -c 9000 /dev/zero | tr '\0' 'a')
expect_status "oversized headers return 400" 400 "GET / HTTP/1.1\r\nX-Big: $big\r\n\r\n"

# A client that disconnects without reading the response must not kill the server.
printf 'GET / HTTP/1.1\r\n\r\n' | nc -q 0 127.0.0.1 "$PORT" >/dev/null || true
expect_status "server survives an early disconnect" 200 'GET / HTTP/1.1\r\n\r\n'

body=$(curl -s "http://127.0.0.1:$PORT/")
if [[ "$body" == "hello from epoll-http" ]]; then echo "ok   response body"; else
    echo "FAIL response body: '$body'"; failures=$((failures + 1)); fi

if ((failures)); then echo "$failures integration test(s) failed"; exit 1; fi
echo "all integration tests passed"
