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
      if (uint32_unpack(tp->mem + pos + 12) == klen) {
#if 0
        printf("cmp(\"%.*s\",\"%.*s\")\n", (int)klen, tp->mem + pos + 20, (int)klen, key);
#endif
        if (memcmp(tp->mem + pos + 20, key, klen) == 0) {
          *val = (const char*)(tp->mem + pos + 20 + klen);
          *vlen = uint32_unpack(tp->mem + pos + 16);
          return 0;
        }
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
    uint32_t pos, nextpos, b0, b1, n_klen, best = 0;
    pos = uint32_unpack(tp->mem);
    b0 = uint32_unpack(tp->mem + pos);
    nextpos = uint32_unpack(tp->mem + pos + (get_bit(b0, key, klen) ? 8 : 4));
    for (;;) {
      n_klen = uint32_unpack(tp->mem + pos + 12);
      if (n_klen > 0 && n_klen <= klen && n_klen > best) {
#if 0
        printf("cmp(\"%.*s\",\"%.*s\")", (int)n_klen, tp->mem + pos + 20, (int)klen, key);
#endif
        if (memcmp(key, tp->mem + pos + 20, n_klen) == 0) {
#if 0
          printf(" -> best is \"%.*s\"", (int)n_klen, tp->mem + pos + 20);
#endif
          *keymatch = (const char*)(tp->mem + pos + 20);
          *matchlen = best = uint32_unpack(tp->mem + pos + 12);
          *val = (const char*)(tp->mem + pos + 20 + *matchlen);
          *vlen = uint32_unpack(tp->mem + pos + 16);
        }
#if 0
        printf("\n");
#endif
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
