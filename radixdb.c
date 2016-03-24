/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "radixdb.h"

static uint32_t
uint32_unpack(const unsigned char *buf) {
  uint32_t n = buf[3];
  n <<= 8; n |= buf[2];
  n <<= 8; n |= buf[1];
  n <<= 8; n |= buf[0];
  return n;
}

static int
get_bit(uint32_t b, const char *key, uint32_t klen) {
  size_t index = b >> 3;
  if (index >= klen)
    return 0;
  return key[index] & (1 << (7 - (b & 7)));
}

static uint32_t
prefix(const char* x, uint32_t n, const char* key, uint32_t m) {
  uint32_t k;
  for (k = 0; k < n; k++)
    if (k == m || x[k] != key[k])
      return k;
  return n;
}

void radixdb_free(struct radixdb* tp) {
  free(tp->mem);
}

int radixdb_check(struct radixdb* tp) {
  uint32_t pos = 4, bit, left, right, klen, vlen;
  while (pos < tp->dend) {
    bit = uint32_unpack(tp->mem + pos);
    left = uint32_unpack(tp->mem + pos + 4);
    right = uint32_unpack(tp->mem + pos + 8);
    klen = uint32_unpack(tp->mem + pos + 12);
    vlen = uint32_unpack(tp->mem + pos + 16);
    if ((bit > 0 && ((bit - 1) >> 3) >= klen)
        || left >= tp->dend
        || right >= tp->dend
        || pos + 4 + 8 + 8 + klen + vlen > tp->dend) {
      errno = EINVAL;
      return -1;
    }
    pos += 4 + 8 + 8 + klen + vlen;
  }
  return 0;
}

int radixdb_lookup(const struct radixdb* tp,
                   const char *key, size_t klen,
                   const char **val, size_t *vlen) {
  if (tp->dend > 4 && klen > 0) {
    uint32_t pos, nextpos, b0, b1;
    pos = uint32_unpack(tp->mem);
    b0 = uint32_unpack(tp->mem + pos);
    nextpos = uint32_unpack(tp->mem + pos + (get_bit(b0, key, klen) ? 8 : 4));
    for (;;) {
      if (uint32_unpack(tp->mem + pos + 12) == klen
          && memcmp(tp->mem + pos + 20, key, klen) == 0) {
        *val = (const char*)(tp->mem + pos + 20 + klen);
        *vlen = uint32_unpack(tp->mem + pos + 16);
        return 0;
      }
      b1 = uint32_unpack(tp->mem + nextpos);
      if (b1 <= b0 || b1 > (klen << 3))
        break;
      pos = nextpos;
      nextpos = uint32_unpack(tp->mem + pos + (get_bit(b1, key, klen) ? 8 : 4));
      b0 = b1;
    }
  }

  errno = ENOENT;
  return -1;
}

int radixdb_longest_match(const struct radixdb* tp,
                          const char *key, size_t klen,
                          const char **keymatch, size_t *matchlen,
                          const char **val, size_t *vlen) {
  if (tp->dend > 4 && klen > 0) {
    uint32_t pos, nextpos, b0, b1, k, n_klen, best = 0;
    pos = uint32_unpack(tp->mem);
    b0 = uint32_unpack(tp->mem + pos);
    nextpos = uint32_unpack(tp->mem + pos + (get_bit(b0, key, klen) ? 8 : 4));
    for (;;) {
      n_klen = uint32_unpack(tp->mem + pos + 12);
      if (n_klen <= klen) {
        k = prefix((const char*)(tp->mem + pos + 20), n_klen, key, klen);
        if (k == n_klen && k > best) {
          *keymatch = (const char*)(tp->mem + pos + 20);
          *matchlen = best = uint32_unpack(tp->mem + pos + 12);
          *val = (const char*)(tp->mem + pos + 20 + *matchlen);
          *vlen = uint32_unpack(tp->mem + pos + 16);
        }
      }
      b1 = uint32_unpack(tp->mem + nextpos);
      if (b1 <= b0 || b1 > (klen << 3))
        break;
      pos = nextpos;
      nextpos = uint32_unpack(tp->mem + pos + (get_bit(b1, key, klen) ? 8 : 4));
      b0 = b1;
    }
    if (best > 0)
      return 0;
  }

  errno = ENOENT;
  return -1;
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

int radixdb_iter_next(const struct radixdb* tp,
                      uint32_t *iterator,
                      const char **key, size_t *klen,
                      const char **val, size_t *vlen) {
  if (*iterator < tp->dend) {
    *klen = uint32_unpack(tp->mem + *iterator + 12);
    *vlen = uint32_unpack(tp->mem + *iterator + 16);
    *key = (const char*)(tp->mem + *iterator + 20);
    *val = (const char*)(tp->mem + *iterator + 20 + *klen);
    *iterator += 4 + 8 + 8 + *klen + *vlen;
    return 0;
  }
  errno = ENOENT;
  return -1;
}
