#! /usr/bin/make -rf
# Makefile: make file for radixdb package
#
# This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
# Public domain.

VERSION = 0.25

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
CFLAGS = -O0 -g
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

LIB_SRCS = radixdb.c

DISTFILES = Makefile radixdb.h $(LIB_SRCS)
all: static
static: staticlib radixdbmk radixdbget radixdbmatch
staticlib: $(LIB)

LIB_OBJS = $(LIB_SRCS:.c=.o)

$(LIB): $(LIB_OBJS)
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)
	-$(RANLIB) $@

radixdbmk: radixdbmk.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ radixdbmk.o $(USELIB)

radixdbget: radixdbget.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ radixdbget.o $(USELIB)

radixdbmatch: radixdbmatch.o $(USELIB)
	$(LD) $(LDFLAGS) -o $@ radixdbmatch.o $(USELIB)

.SUFFIXES:
.SUFFIXES: .c .o .lo

.c.o:
	$(CC) $(CFLAGS) $(CDEFS) -c $<

radixdb.o: radixdb.h
radixdbmk.o: radixdb.h
radixdbget.o: radixdb.h
radixdbmatch.o: radixdb.h
$(LIB_OBJS): radixdb.h

clean:
	-rm -f *.o core *~ tests.out tests-shared.ok
realclean distclean:
	-rm -f *.o core *~ $(LIBBASE)[._][aps]* radixdbmk radixdbget radixdbmatch

DNAME = radixdb-$(VERSION)
dist: $(DNAME).tar.gz
$(DNAME).tar.gz: $(DISTFILES)
	mkdir $(DNAME)
	ln $(DISTFILES) $(DNAME)/
	tar cfz $@ $(DNAME)
	rm -fr $(DNAME)

.PHONY: all clean realclean dist
.PHONY: static staticlib
