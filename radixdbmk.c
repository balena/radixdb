/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "radixdb.h"

static int getnum(FILE *f, uint32_t *np, char *nextchar) {
  int c, i = 0;
  uint32_t n = 0;
  while ((c = getc(f)) >= '0' && c <= '9') {
    c -= '0';
    if (0xffffffff / 10 - c < n) {
      errno = EBADMSG;
      return -1;
    }
    n = n * 10 + c;
    ++i;
  }
  *np = n;
  *nextchar = c;
  return i;
}

static void badinput(int line) {
  fprintf(stderr, "bad input (line %d)\n", line);
  exit(1);
}

int main(int argc, char **argv) {
  char c;
  char* buf = NULL;
  struct radixdb db;
  struct radixdb_make db_make;
  int line = 1;
  uint32_t i;

  radixdb_make_start(&db_make);
  while((c = getc(stdin)) == '+') {
    uint32_t klen, vlen = 0;
    if (getnum(stdin, &klen, &c) < 0 || c != ','
        || getnum(stdin, &vlen, &c) < 0 || c != ':'
        || 0xffffffff - klen < vlen)
      badinput(line);
    buf = (char*) realloc(buf, klen + vlen);
    if (fread(buf, 1, klen, stdin) != klen
        || getc(stdin) != '-' || getc(stdin) != '>'
        || fread(buf + klen, 1, vlen, stdin) != vlen
        || getc(stdin) != '\n')
      badinput(line);
    if (radixdb_make_add(&db_make, buf, klen, buf + klen, vlen) != 0) {
      fprintf(stderr, "ignored duplicate (line %d)\n", line);
    }
    line += 1;
    /* Handle cases where the key or the value contain newlines */
    for (i = 0; i < klen + vlen; i++) {
      if (buf[i] == '\n')
        line++;
    }
  }
  radixdb_make_finish(&db_make, &db);
  fwrite(db.mem, db.dend, 1, stdout);
  radixdb_free(&db);

  free(buf);
  return 0;
}
