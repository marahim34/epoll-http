CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Wshadow -Werror

SRC := src/main.c src/http.c

epoll-http: $(SRC) src/http.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

tests/test_http: tests/test_http.c src/http.c src/http.h
	$(CC) $(CFLAGS) -o $@ tests/test_http.c src/http.c $(LDFLAGS)

test: epoll-http tests/test_http
	./tests/test_http
	./tests/integration.sh ./epoll-http

# Build and test with AddressSanitizer and UndefinedBehaviorSanitizer.
sanitize: clean
	$(MAKE) test CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
	             LDFLAGS="-fsanitize=address,undefined"

clean:
	rm -f epoll-http tests/test_http

.PHONY: clean test sanitize
