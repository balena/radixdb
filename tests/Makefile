CC = cc
CFLAGS = -std=c99 -O0 -g -Wall
CDEFS = -D_FILE_OFFSET_BITS=64
LD = $(CC)

LIBBASE = libradixdb
LIB = $(LIBBASE).a

UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS = -pthread -lm -lrt
endif
ifeq ($(UNAME),Darwin)
LDFLAGS = -lm
endif

all: check_insert run_all_tests

check_insert: check_insert.o ../$(LIB)
	$(LD) $(CFLAGS) -o $@ check_insert.o ../$(LIB) -lcheck $(LDFLAGS)

run_all_tests:
	./check_insert

clean:
	rm -f *.o check_insert

.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<
