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

static uint32_t
uint32_unpack(const unsigned char *buf) {
  uint32_t n = buf[3];
  n <<= 8; n |= buf[2];
  n <<= 8; n |= buf[1];
  n <<= 8; n |= buf[0];
  return n;
}

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

static void
dot_escape(const char* s, uint32_t len) {
  while (len > 0) {
    switch (*s) {
      case '{': case '}': case '(': case ')': case '<': case '>': case '&':
        fputc('\\', stdout); fputc(*s, stdout);
        break;
      default:
        fputc(*s, stdout);
        break;
    }
    s++;
    len--;
  }
}

void radixdb_dump2dot(const struct radixdb* tp) {
  uint32_t pos = 4, bit, left, right, klen, vlen;
  printf("digraph G {\n");
  printf("  node [shape=record];\n");
  printf("  root -> n%lu;\n",
      (unsigned long)uint32_unpack(tp->mem));
  while (pos < tp->dend) {
    bit = uint32_unpack(tp->mem + pos);
    left = uint32_unpack(tp->mem + pos + 4);
    right = uint32_unpack(tp->mem + pos + 8);
    klen = uint32_unpack(tp->mem + pos + 12);
    vlen = uint32_unpack(tp->mem + pos + 16);
    printf("  n%lu [label=\"{{%lu|%lu|\\\"",
        (unsigned long)pos,
        (unsigned long)(bit>>3), (unsigned long)(bit&7));
    dot_escape((const char*)(tp->mem + pos + 20), klen);
    printf("\\\"&rarr;\\\"");
    dot_escape((const char*)(tp->mem + pos + 20 + klen), vlen);
    printf("\\\"}|{<l>left|<r>right}}\"];\n");
    printf("  n%lu:l -> n%lu;\n"
           "  n%lu:r -> n%lu;\n",
           (unsigned long)pos, (unsigned long)left,
           (unsigned long)pos, (unsigned long)right);
    pos += 4 + 8 + 8 + klen + vlen;
  }
  printf("}\n");
}

void radixdb_dump(const struct radixdb* tp) {
  uint32_t pos = 4, klen, vlen;
  while (pos < tp->dend) {
    klen = uint32_unpack(tp->mem + pos + 12);
    vlen = uint32_unpack(tp->mem + pos + 16);
    printf("+%lu,%lu:%.*s->%.*s\n",
        (unsigned long)klen, (unsigned long)vlen,
        (int)klen, tp->mem + pos + 20,
        (int)vlen, tp->mem + pos + 20 + klen);
    pos += 4 + 8 + 8 + klen + vlen;
  }
  printf("\n");
}

struct radixdb_stats {
  uint32_t cnt;
  uint32_t kmin, kmax, ktot;
  uint32_t vmin, vmax, vtot;
  uint32_t dmax, bmax;
};

static void
collectstats(const struct radixdb* tp, uint32_t pos, int32_t b0,
    int d, struct radixdb_stats* stats) {
  int32_t b1;
  uint32_t klen, vlen;

  b1 = (int32_t)uint32_unpack(tp->mem + pos);
  if (b1 <= b0)
    return;

  klen = uint32_unpack(tp->mem + pos + 12);
  vlen = uint32_unpack(tp->mem + pos + 16);

  stats->cnt++;

  if (klen < stats->kmin)
    stats->kmin = klen;
  if (klen > stats->kmax)
    stats->kmax = klen;
  stats->ktot += klen;

  if (vlen < stats->vmin)
    stats->vmin = vlen;
  if (vlen > stats->vmax)
    stats->vmax = vlen;
  stats->vtot += vlen;

  if (d > stats->dmax)
    stats->dmax = d;
  if (b1 > stats->bmax)
    stats->bmax = b1;

  collectstats(tp, uint32_unpack(tp->mem + pos + 4), b1, d+1, stats);
  collectstats(tp, uint32_unpack(tp->mem + pos + 8), b1, d+1, stats);
}

void radixdb_stats(const struct radixdb* tp) {
  struct radixdb_stats st;

  memset(&st, 0, sizeof(st));
  st.kmin = 0xfffffffful;
  st.vmin = 0xfffffffful;

  collectstats(tp, uint32_unpack(tp->mem + 4 + 4), 0, 0, &st);
  collectstats(tp, uint32_unpack(tp->mem + 4 + 8), 0, 0, &st);

  printf("number of records: %u\n", st.cnt);
  printf("key min/avg/max length: %u/%u/%u\n", st.kmin,
         st.cnt ? (st.ktot + st.cnt / 2) / st.cnt : 0, st.kmax);
  printf("val min/avg/max length: %u/%u/%u\n", st.vmin,
         st.cnt ? (st.vtot + st.cnt / 2) / st.cnt : 0, st.vmax);
  printf("max depth: %u\n", st.dmax);
  printf("max bit depth: %u\n", st.bmax);
}
