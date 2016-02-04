CC = cc
CFLAGS = -std=c99 -O0 -g -Wall
CDEFS = -D_FILE_OFFSET_BITS=64
LD = $(CC)

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS = -pthread -lm -lrt
endif
ifeq ($(UNAME),Darwin)
LDFLAGS = -lm
endif

check_insert: check_insert.o
	$(LD) $(CFLAGS) -o $@ check_insert.o ../libradixdb.a -lcheck $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<
