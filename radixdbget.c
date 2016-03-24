/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <stdio.h>
#include <string.h>

#include "radixdb.h"
#include "radixdb_util.h"

int main(int argc, char **argv) {
  struct radixdb db;
  const char *val;
  size_t vlen;
  int result = 0;

  if (argc != 3) {
    fprintf(stderr, "usage: radixdbget f key\n");
    return 1;
  }

  openfile(argv[1], &db);

  if (radixdb_lookup(&db, argv[2], strlen(argv[2]), &val, &vlen) == 0) {
    printf("%.*s", (int)vlen, val);
  } else {
    result = 2;
  }

  closefile(&db);

  return result;
}
