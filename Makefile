CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Wshadow -Werror

SRC := src/main.c src/http.c

epoll-http: $(SRC) src/http.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f epoll-http tests/test_http

.PHONY: clean
