/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include "radixdb_util.h"

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

static int __fd;

void openfile(const char* filename, struct radixdb* db) {
  struct stat st;
#ifdef _WIN32
  static HANDLE hFile, hMapping;
#endif

  /* open and get file size */
  __fd = open(filename, O_RDONLY);
  if (__fd < 0 || fstat(__fd, &st) < 0) {
    fprintf(stderr, "unable to open `%s'\n", filename);
    exit(1);
  }

  /* trivial sanity check: at least root should be here */
  if (st.st_size < 4 || st.st_size >= 0xfffffffful) {
    fprintf(stderr, "invalid file\n");
    exit(1);
  }

  db->dend = st.st_size;
  /* memory-map file */
#ifdef _WIN32
  hFile = (HANDLE) _get_osfhandle(__fd);
  if (hFile == (HANDLE) -1) {
    fprintf(stderr, "_get_osfhandle error\n");
    exit(1);
  }
  hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!hMapping) {
    fprintf(stderr, "CreateFileMapping error\n");
    exit(1);
  }
  db->mem = (unsigned char *)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
  CloseHandle(hMapping);
  if (!mem) {
    fprintf(stderr, "MapViewOfFile error\n");
    exit(1);
  }
#else
  db->mem = (unsigned char*)mmap(NULL, db->dend, PROT_READ, MAP_SHARED, __fd, 0);
  if (db->mem == MAP_FAILED) {
    fprintf(stderr, "mmap error");
    exit(1);
  }
#endif /* _WIN32 */
}

void closefile(struct radixdb* db) {
#ifdef _WIN32
  UnmapViewOfFile((void*) db->mem);
#else
  munmap((void*)db->mem, db->dend);
#endif /* _WIN32 */

  close(__fd);
}
