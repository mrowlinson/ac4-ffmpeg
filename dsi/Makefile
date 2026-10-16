# dsi: standalone AC-4 DSI (dac4 payload) serializer, pure C.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

# Configured ffmpeg build tree for the DSI-11 PutBitContext suite.
# LAVCBASE holds config.h + built libavutil; LAVCSRC is the ffmpeg
# source tree (in these scratch builds: <LAVCBASE>/src symlink).
# Override both for an in-tree-configured ffmpeg (LAVCSRC=<tree>).
LAVCBASE ?= /Volumes/NVME/ac4-ffmpeg-ws/build/ff-C
LAVCSRC ?= $(LAVCBASE)/src
# -isystem (not -I): lavc's own headers carry -Wextra noise; system
# headers stay warning-silent so -Werror still gates OUR code.
LAVCINC := -isystem $(LAVCBASE) -isystem $(LAVCSRC)
LAVCLIBS := $(LAVCBASE)/libavutil/libavutil.a -lm -lz -lpthread

# dsi_test intentionally covers the deprecated string shim (DSI-12):
# silence only that warning, -Werror still gates everything else.
TESTCFLAGS := -Wno-deprecated-declarations

HDRS := ac4dsi.h ac4dsi_priv.h dsi_goldens.h

all: build/dsi_test build/dsi_putbits_test

build/dsi_test: ac4dsi.c dsi_test.c $(HDRS)
	mkdir -p build
	$(CC) $(CFLAGS) $(TESTCFLAGS) ac4dsi.c dsi_test.c -o build/dsi_test

build/dsi_putbits_test: ac4dsi.c ac4dsi_putbits.c ac4dsi_putbits.h \
		dsi_putbits_test.c $(HDRS)
	mkdir -p build
	$(CC) $(CFLAGS) $(LAVCINC) ac4dsi.c ac4dsi_putbits.c \
		dsi_putbits_test.c $(LAVCLIBS) -o build/dsi_putbits_test

test: build/dsi_test build/dsi_putbits_test
	./build/dsi_test
	./build/dsi_putbits_test

# -Werror gate: same suites, warnings are failures.
test-werror: build/dsi_test_werror build/dsi_putbits_test_werror
	./build/dsi_test_werror
	./build/dsi_putbits_test_werror

build/dsi_test_werror: ac4dsi.c dsi_test.c $(HDRS)
	mkdir -p build
	$(CC) $(CFLAGS) -Werror $(TESTCFLAGS) ac4dsi.c dsi_test.c \
		-o build/dsi_test_werror

build/dsi_putbits_test_werror: ac4dsi.c ac4dsi_putbits.c \
		ac4dsi_putbits.h dsi_putbits_test.c $(HDRS)
	mkdir -p build
	$(CC) $(CFLAGS) -Werror $(LAVCINC) ac4dsi.c ac4dsi_putbits.c \
		dsi_putbits_test.c $(LAVCLIBS) -o build/dsi_putbits_test_werror

# Sanitizer gate: ASan+UBSan builds of the same suites.
test-asan: build/dsi_test_asan build/dsi_putbits_test_asan
	./build/dsi_test_asan
	./build/dsi_putbits_test_asan

build/dsi_test_asan: ac4dsi.c dsi_test.c $(HDRS)
	mkdir -p build
	$(CC) $(CFLAGS) -Werror $(TESTCFLAGS) -g -fsanitize=address,undefined \
		ac4dsi.c dsi_test.c -o build/dsi_test_asan

build/dsi_putbits_test_asan: ac4dsi.c ac4dsi_putbits.c \
		ac4dsi_putbits.h dsi_putbits_test.c $(HDRS)
	mkdir -p build
	$(CC) $(CFLAGS) -Werror $(LAVCINC) -g -fsanitize=address,undefined \
		ac4dsi.c ac4dsi_putbits.c dsi_putbits_test.c $(LAVCLIBS) \
		-o build/dsi_putbits_test_asan

clean:
	rm -rf build

.PHONY: all test test-werror test-asan clean
