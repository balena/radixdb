/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <stdio.h>
#include <string.h>

#include "radixdb.h"
#include "radixdb_util.h"

int main(int argc, char **argv) {
  struct radixdb db;
  const char *val, *key;
  size_t vlen, klen;
  int result = 0;

  if (argc != 3) {
    fprintf(stderr, "usage: radixdbmatch f key\n");
    return 1;
  }

  openfile(argv[1], &db);

  if (radixdb_longest_match(&db, argv[2], strlen(argv[2]),
      &key, &klen, &val, &vlen) == 0) {
    printf("+%lu,%lu:%.*s->%.*s\n",
        (unsigned long)klen, (unsigned long)vlen,
        (int)klen, key,
        (int)vlen, val);
  } else {
    result = 2;
  }

  closefile(&db);

  return result;
}
