# dsi: standalone AC-4 DSI (dac4 payload) serializer, pure C.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

all: build/dsi_test

build/dsi_test: ac4dsi.c ac4dsi.h dsi_test.c
	mkdir -p build
	$(CC) $(CFLAGS) ac4dsi.c dsi_test.c -o build/dsi_test

test: build/dsi_test
	./build/dsi_test

# -Werror gate: same suite, warnings are failures.
test-werror: build/dsi_test_werror
	./build/dsi_test_werror

build/dsi_test_werror: ac4dsi.c ac4dsi.h dsi_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Werror ac4dsi.c dsi_test.c -o build/dsi_test_werror

# Sanitizer gate: ASan+UBSan build of the same suite.
test-asan: build/dsi_test_asan
	./build/dsi_test_asan

build/dsi_test_asan: ac4dsi.c ac4dsi.h dsi_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -Werror -g -fsanitize=address,undefined \
		ac4dsi.c dsi_test.c -o build/dsi_test_asan

clean:
	rm -rf build

.PHONY: all test test-werror test-asan clean
