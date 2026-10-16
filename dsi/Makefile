# dsi: standalone AC-4 DSI (dac4 payload) serializer, pure C.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

all: build/dsi_test

build/dsi_test: ac4dsi.c ac4dsi.h dsi_test.c
	mkdir -p build
	$(CC) $(CFLAGS) ac4dsi.c dsi_test.c -o build/dsi_test

test: build/dsi_test
	./build/dsi_test

clean:
	rm -rf build

.PHONY: all test clean
