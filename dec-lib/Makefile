# dec-lib: streaming AC-4 decoder lib (H1) + parity shim.
# Vendored DSP modules are verbatim from dolby-ac4-cleanroom (read-only
# source) except acpl.c/h (state threaded) and dec_lib.c (ctx conversion
# + streaming split). ffmpeg tree + plain repo stay untouched.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -ffp-contract=off \
          -fno-builtin-sin -fno-builtin-cos
SRC = src/dec_lib.c src/entropy.c src/imdct.c src/huffman_tables.c \
      src/sfb_offsets.c src/snf_tables.c src/aspx_tables.c \
      src/aspx_noise.c src/acpl.c src/qmf.c src/qwin_tables.c
OBJ = $(SRC:src/%.c=build/%.o)
LIBOBJ = $(SRC:src/%.c=build/lib_%.o)

all: build/libac4d.a build/shim

build/libac4d.a: $(LIBOBJ)
	ar rcs $@ $(LIBOBJ)

build/lib_%.o: src/%.c src/ac4d.h | build
	$(CC) $(CFLAGS) -DAC4D_LIB_ONLY -c $< -o $@

build/shim: $(OBJ) | build
	$(CC) $(CFLAGS) $(OBJ) -o $@ -lm

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -f build/*.o build/libac4d.a build/shim

.PHONY: all clean
