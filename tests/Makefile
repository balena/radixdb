CC = cc
CFLAGS = -std=c99 -O0 -g -Wall
CDEFS = -D_FILE_OFFSET_BITS=64
LD = $(CC)
LDFLAGS = $(CFLAGS)

check_insert: check_insert.o
	$(LD) $(LDFLAGS) -o $@ -pthread check_insert.o ../libradixdb.a -lcheck -lm -lrt

.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<
