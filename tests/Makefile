CC = cc
CFLAGS = -std=c99 -O0 -g -Wall
CDEFS = -D_FILE_OFFSET_BITS=64
LD = $(CC)

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS = $(CFLAGS) -pthread -lm -lrt
endif
ifeq ($(UNAME),Darwin)
LDFLAGS = $(CFLAGS) -lm
endif

check_insert: check_insert.o
	$(LD) $(LDFLAGS) -o $@ check_insert.o ../libradixdb.a -lcheck

.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<
