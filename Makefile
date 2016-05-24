#! /usr/bin/make -rf
# Makefile: make file for radixdb package
#
# This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
# Public domain.

VERSION = 0.27

prefix=/usr/local
exec_prefix=$(prefix)
bindir=$(exec_prefix)/bin
libdir=$(exec_prefix)/lib
syslibdir=$(libdir)
sysconfdir=/etc
includedir=$(prefix)/include
mandir=$(prefix)/man
DESTDIR=

CC = cc
CFLAGS = -std=c99 -O0 -g -Wall
CDEFS = -D_FILE_OFFSET_BITS=64
LD = $(CC)
LDFLAGS = $(CFLAGS)

AR = ar
ARFLAGS = rv
RANLIB = ranlib

LIBBASE = libradixdb
LIB = $(LIBBASE).a
USELIB = $(LIB)
INSTALLPROG = radixdb

CP = cp

LIB_SRCS = radixdb.c radixdb_make.c

DISTFILES = Makefile radixdb.h $(LIB_SRCS)
all: static
static: staticlib radixdbmk radixdbget radixdbmatch radixdbdump radixdb2dot radixdbstats
staticlib: $(LIB)

LIB_OBJS = $(LIB_SRCS:.c=.o)

$(LIB): $(LIB_OBJS)
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)
	-$(RANLIB) $@

radixdbmk: radixdbmk.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ $^

radixdbget: radixdbget.o radixdb_util.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ $^

radixdbmatch: radixdbmatch.o radixdb_util.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ $^

radixdbdump: radixdbdump.o radixdb_util.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ $^

radixdb2dot: radixdb2dot.o radixdb_util.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ $^

radixdbstats: radixdbstats.o radixdb_util.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ $^

.SUFFIXES:
.SUFFIXES: .c .o .lo

.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<

radixdb.o: radixdb.h
radixdbmk.o: radixdb.h
radixdbget.o: radixdb.h
radixdbmatch.o: radixdb.h
radixdbdump.o: radixdb.h
radixdb2dot.o: radixdb.h
radixdbstats.o: radixdb.h
$(LIB_OBJS): radixdb.h

clean:
	-rm -f *.o core *~ $(LIBBASE)[._][aps]* radixdbmk radixdbget radixdbmatch radixdbdump radixdb2dot radixdbstats
	$(MAKE) -C tests clean

DNAME = radixdb-$(VERSION)
dist: $(DNAME).tar.gz
$(DNAME).tar.gz: $(DISTFILES)
	mkdir $(DNAME)
	ln $(DISTFILES) $(DNAME)/
	tar cfz $@ $(DNAME)
	rm -fr $(DNAME)

tests: $(LIB)
	$(MAKE) -C tests

.PHONY: all clean realclean dist
.PHONY: tests
.PHONY: static staticlib
