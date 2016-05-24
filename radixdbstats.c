/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <stdio.h>
#include <string.h>

#include "radixdb.h"
#include "radixdb_util.h"

int main(int argc, char **argv) {
  struct radixdb db;
  if (argc != 2) {
    fprintf(stderr, "usage: radixdbdump f\n");
    return 1;
  }
  openfile(argv[1], &db);
  radixdb_stats(&db);
  closefile(&db);
  return 0;
}
