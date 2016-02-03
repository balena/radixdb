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
#ifdef _WIN32
# include <windows.h>
#else
# include <sys/mman.h>
# ifndef MAP_FAILED
#  define MAP_FAILED ((void*)-1)
# endif
#endif
#include <sys/stat.h>

#include "radixdb.h"

int main(int argc, char **argv) {
  struct stat st;
  struct radixdb db;
  int fd;
  const char *val;
  uint32_t vlen;
#ifdef _WIN32
  HANDLE hFile, hMapping;
#endif

  if (argc != 3) {
    printf("usage: radixdbget f key");
    return 1;
  }

  /* open and get file size */
  fd = open(argv[1], O_RDONLY);
  if (fd < 0 || fstat(fd, &st) < 0) {
    fprintf(stderr, "unable to open `%s'\n", argv[1]);
    return 1;
  }

  /* trivial sanity check: at least root should be here */
  if (st.st_size < 4 || st.st_size >= 0xfffffffful) {
    fprintf(stderr, "invalid file\n");
    return 1;
  }

  db.dend = st.st_size;
  /* memory-map file */
#ifdef _WIN32
  hFile = (HANDLE) _get_osfhandle(fd);
  if (hFile == (HANDLE) -1) {
    fprintf(stderr, "_get_osfhandle error\n");
    return 1;
  }
  hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!hMapping) {
    fprintf(stderr, "CreateFileMapping error\n");
    return 1;
  }
  db.mem = (unsigned char *)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
  CloseHandle(hMapping);
  if (!mem) {
    fprintf(stderr, "MapViewOfFile error\n");
    return 1;
  }
#else
  db.mem = (unsigned char*)mmap(NULL, db.dend, PROT_READ, MAP_SHARED, fd, 0);
  if (db.mem == MAP_FAILED) {
    fprintf(stderr, "mmap error");
    return 1;
  }
#endif /* _WIN32 */

  db.size = 0;  /* file is read-only */

  if (radixdb_longest_match(&db, argv[2], strlen(argv[2]), &val, &vlen) == 0) {
    printf("Found: %.*s\n", (int)vlen, val);
  } else {
    printf("Not found\n");
  }

#ifdef _WIN32
  UnmapViewOfFile((void*) db.mem);
#else
  munmap((void*)db.mem, db.dend);
#endif /* _WIN32 */

  close(fd);
  return 0;
}
