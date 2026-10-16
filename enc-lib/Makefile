# enc-lib: AC-4 encoder library (opaque-ctx streaming API, v1 buffered).
# Float discipline (H9): -ffp-contract=off, -fno-builtin-sin/cos/pow.
# Losing any of these silently breaks CLI bit-parity.
CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -ffp-contract=off \
	-fno-builtin-sin -fno-builtin-cos -fno-builtin-pow

SRCDIR := src
CORE_SRCS := $(SRCDIR)/enc_core_enc.c $(SRCDIR)/enc_core_long51.c \
	$(SRCDIR)/enc_core_fit51.c $(SRCDIR)/bits.c $(SRCDIR)/asf.c \
	$(SRCDIR)/mdct.c $(SRCDIR)/loudness.c $(SRCDIR)/enc_mc3.c \
	$(SRCDIR)/enc_jshort.c $(SRCDIR)/enc_tail59.c \
	$(SRCDIR)/enc_aspx.c $(SRCDIR)/enc_acpl.c \
	$(SRCDIR)/huffman_tables.c $(SRCDIR)/aspx_tables.c \
	$(SRCDIR)/sfb_offsets.c $(SRCDIR)/pycompat.c
LIB_SRCS := $(SRCDIR)/ac4enc_lib.c $(CORE_SRCS)

all: libac4enc.a

libac4enc.a: $(LIB_SRCS) $(SRCDIR)/ac4enc_lib.h $(SRCDIR)/enc_core.h
	$(CC) $(CFLAGS) -c $(SRCDIR)/ac4enc_lib.c -o ac4enc_lib.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_core_enc.c -o enc_core_enc.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_core_long51.c -o enc_core_long51.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_core_fit51.c -o enc_core_fit51.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/bits.c -o bits.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/asf.c -o asf.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/mdct.c -o mdct.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/loudness.c -o loudness.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_mc3.c -o enc_mc3.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_jshort.c -o enc_jshort.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_tail59.c -o enc_tail59.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_aspx.c -o enc_aspx.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/enc_acpl.c -o enc_acpl.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/huffman_tables.c -o huffman_tables.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/aspx_tables.c -o aspx_tables.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/sfb_offsets.c -o sfb_offsets.o
	$(CC) $(CFLAGS) -c $(SRCDIR)/pycompat.c -o pycompat.o
	ar rcs $@ *.o
	rm -f *.o

# Harness + unit probes build into OUTDIR (default: scratch, never repo).
OUTDIR ?= /Volumes/NVME/ac4-ffmpeg-ws/scratch/ff-enc-librefactor

harness: libac4enc.a
	$(CC) $(CFLAGS) -o $(OUTDIR)/encharness tests/harness.c \
		libac4enc.a -lm

unit: libac4enc.a
	$(CC) $(CFLAGS) -o $(OUTDIR)/unit tests/unit.c \
		libac4enc.a -lm

clean:
	rm -f libac4enc.a *.o

.PHONY: all harness unit clean
