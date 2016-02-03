/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "radixdb.h"

static void
uint32_pack(unsigned char *buf, uint32_t i) {
  buf[0] = i & 255; i >>= 8;
  buf[1] = i & 255; i >>= 8;
  buf[2] = i & 255;
  buf[3] = i >> 8;
}

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
  int index = b >> 3;
  if (index >= klen)
    return 0;
  return key[index] & (1 << (7 - (b & 7)));
}

static void
printbits(const char *b, uint32_t blen) {
  uint32_t i, j;
  for (i = 0; i < blen; i++) {
    for (j = 0; j < 8; j++) {
      putchar(get_bit(j, b + i, blen) ? '1' : '0');
    }
    if (i + 1 < blen)
      putchar(' ');
  }
}

static int
diff_bit(const char* a, uint32_t alen,
         const char* b, uint32_t blen) {
  unsigned char c1, c2, mask;
  uint32_t i, todo = alen > blen ? alen : blen;

  /* Consider padding zeroes after the end of input strings. */ 
  for (i = 0; i < todo; i++) {
    c1 = (i < alen) ? (unsigned char)a[i] : 0;
    c2 = (i < blen) ? (unsigned char)b[i] : 0;
    if (c1 != c2)
      break;
  }

  if (i == alen && alen == blen)
    return -1;  /* same entry */

  i <<= 3;
  mask = ~(~c1 ^ c2);

  /* Binary search on the diff bit. Three comparisons for any mask. */
  if (mask & 0xf0) {
    if (mask & 0xc0) {
      return (mask & 0x80) ? i : i + 1;
    } else {
      return (mask & 0x20) ? i + 2 : i + 3;
    }
  } else {
    if (mask & 0x0c) {
      return (mask & 0x08) ? i + 4 : i + 5;
    } else {
      return (mask & 0x02) ? i + 6 : i + 7;
    }
  }
}

static uint32_t
log2ceil(uint32_t i) {
  uint32_t l = 0;
  if (i & (i - 1)) { l += 1; }
  if (i >> 16) { l += 16; i >>= 16; }
  if (i >> 8)  { l += 8; i >>= 8; }
  if (i >> 4)  { l += 4; i >>= 4; }
  if (i >> 2)  { l += 2; i >>= 2; }
  if (i >> 1)  { l += 1; }
  return l;
}

static int
ensure_capacity(struct radixdb* tp, uint32_t extra_size) {
  unsigned char* mem;
  uint32_t new_size = 1 << log2ceil(tp->dend + extra_size);
  if (new_size > tp->size) {
    mem = (unsigned char*) realloc(tp->mem, new_size);
    if (!mem)
      return -1;
    tp->size = new_size;
    tp->mem = mem;
  }
  return 0;
}

static uint32_t
insert_between_inner(struct radixdb* tp, const char *key, uint32_t klen,
    uint32_t pos, uint32_t n, uint32_t b0, int diff) {
  uint32_t b1 = uint32_unpack(tp->mem + pos);
  unsigned char *tmp;
  if (b1 <= b0 || diff < (int)b1) {
    uint32_pack(tp->mem + n + (get_bit(diff, key, klen) ? 4 : 8), pos);
    return n;
  }
  tmp = tp->mem + pos + (get_bit(b1, key, klen) ? 8 : 4);
  n = insert_between_inner(tp, key, klen, uint32_unpack(tmp), n, b1, diff);
  uint32_pack(tmp, n);
  return pos;
}

static uint32_t
insert_between(struct radixdb* tp, const char *key, uint32_t klen,
    uint32_t n, int diff) {
  uint32_t pos = uint32_unpack(tp->mem);
  uint32_t b0 = uint32_unpack(tp->mem + pos);
  unsigned char *tmp;
  if (diff < (int)b0) {
    uint32_pack(tp->mem + n + (get_bit(diff, key, klen) ? 4 : 8), pos);
    return n;
  }
  tmp = tp->mem + pos + (get_bit(b0, key, klen) ? 8 : 4);
  n = insert_between_inner(tp, key, klen, uint32_unpack(tmp), n, b0, diff);
  uint32_pack(tmp, n);
  return pos;
}

static uint32_t
search_node(const struct radixdb *tp, const char *key, uint32_t klen) {
  uint32_t pos, b0, b1;
  pos = uint32_unpack(tp->mem);
  b0 = uint32_unpack(tp->mem + pos);
  pos = uint32_unpack(tp->mem + pos + (get_bit(b0, key, klen) ? 8 : 4));
  for (;;) {
    b1 = uint32_unpack(tp->mem + pos);
    if (b1 <= b0)
      break;
    pos = uint32_unpack(tp->mem + pos + (get_bit(b1, key, klen) ? 8 : 4));
    b0 = b1;
  }
  return pos;
}

static uint32_t
find_prefix(const char *a, uint32_t alen, const char *b, uint32_t blen) {
  unsigned char c1, c2;
  uint32_t i, todo = alen < blen ? alen : blen;

  for (i = 0; i < todo; i++) {
    c1 = (unsigned char)a[i];
    c2 = (unsigned char)b[i];
    if (c1 != c2)
      break;
  }

#if 0
  printf("prefix(%.*s, %.*s) -> %.*s\n",
      (int)alen, a, (int)blen, b,
      (int)i, b);
#endif
  return i;
}

static uint32_t
search_largest_prefix_inner(const struct radixdb *tp,
    const char *key, uint32_t klen, uint32_t pos, uint32_t b0,
    uint32_t *max_length) {
  uint32_t nextpos, poskeylen = uint32_unpack(tp->mem + pos + 12);
  uint32_t nextmatch, b1 = uint32_unpack(tp->mem + pos);
  if (b1 <= b0) {
    *max_length = find_prefix(key, klen, tp->mem + pos + 20, poskeylen);
    return (*max_length == poskeylen) ? pos : 0xfffffffful;
  }
  nextpos = uint32_unpack(tp->mem + pos + (get_bit(b1, key, klen) ? 8 : 4));
  nextmatch = search_largest_prefix_inner(tp, key, klen, nextpos, b1, max_length);
  if (nextmatch == 0xfffffffful
      && poskeylen <= *max_length) {
    *max_length = find_prefix(key, *max_length, tp->mem + pos + 20, poskeylen);
    if (*max_length == poskeylen)
      return pos;
  }
  return nextmatch;
}

static uint32_t
search_largest_prefix(const struct radixdb *tp,
    const char *key, uint32_t klen) {
  uint32_t max_length;
  uint32_t pos = uint32_unpack(tp->mem);
  uint32_t b0 = uint32_unpack(tp->mem + pos);
  pos = uint32_unpack(tp->mem + pos + (get_bit(b0, key, klen) ? 8 : 4));
  return search_largest_prefix_inner(tp, key, klen, pos, b0, &max_length);
}

int radixdb_init(struct radixdb* tp) {
  tp->mem = NULL;
  tp->size = 0ul;
  tp->dend = 4;
  if (ensure_capacity(tp, 2000) == -1)
    return -1;
  return 0;
}

void radixdb_free(struct radixdb* tp) {
  free(tp->mem);
}

int radixdb_add(struct radixdb* tp,
                const char *key, uint32_t klen,
                const char *val, uint32_t vlen) {
  int diff;
  uint32_t n, pos, nodesize;

  nodesize = 4 + 8 + 8 + klen + vlen;
  if (ensure_capacity(tp, nodesize) == -1)
    return -1;

  /* copy the key and value to the new node */
  n = tp->dend;
  uint32_pack(tp->mem + n, klen << 3);
  uint32_pack(tp->mem + n + 4, tp->dend);
  uint32_pack(tp->mem + n + 8, tp->dend);
  uint32_pack(tp->mem + n + 12, klen);
  uint32_pack(tp->mem + n + 16, vlen);
  memcpy(tp->mem + n + 20, key, klen);
  memcpy(tp->mem + n + 20 + klen, val, vlen);

  if (tp->dend == 4) {
    /* first node inserted */
    uint32_pack(tp->mem, tp->dend);
  } else {
    /* find insert position */
    pos = search_node(tp, key, klen);
    diff = diff_bit(tp->mem + pos + 20, uint32_unpack(tp->mem + pos + 12),
        key, klen);
    if (diff == -1) {
      errno = EEXIST;
      return -1;  /* entry already exists */
    }
    uint32_pack(tp->mem + n, (uint32_t)diff);
    uint32_pack(tp->mem, insert_between(tp, key, klen, n, diff));
  }

  tp->dend += nodesize;
  return 0;
}

int radixdb_lookup(const struct radixdb* tp,
                   const char *key, uint32_t klen,
                   const char **val, uint32_t *vlen) {
  uint32_t pos;

  if (tp->dend > 4) {
    pos = search_node(tp, key, klen);
    if (klen == uint32_unpack(tp->mem + pos + 12)
        && memcmp(tp->mem + pos + 20, key, klen) == 0) {
      *val = tp->mem + pos + 20 + klen;
      *vlen = uint32_unpack(tp->mem + pos + 16);
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}

int radixdb_longest_match(const struct radixdb* tp,
                          const char *key, uint32_t klen,
                          const char **keymatch, uint32_t *matchlen,
                          const char **val, uint32_t *vlen) {
  uint32_t pos;

  if (tp->dend > 4) {
    pos = search_largest_prefix(tp, key, klen);
    if (pos != 0xfffffffful) {
      *keymatch = tp->mem + pos + 20;
      *matchlen = uint32_unpack(tp->mem + pos + 12);
      *val = tp->mem + pos + 20 + *matchlen;
      *vlen = uint32_unpack(tp->mem + pos + 16);
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}

void radixdb_dump(const struct radixdb* tp) {
  uint32_t pos = 4, bit, left, right, klen, vlen;
  printf("digraph G {\n");
  while (pos < tp->dend) {
    bit = uint32_unpack(tp->mem + pos);
    left = uint32_unpack(tp->mem + pos + 4);
    right = uint32_unpack(tp->mem + pos + 8);
    klen = uint32_unpack(tp->mem + pos + 12);
    vlen = uint32_unpack(tp->mem + pos + 16);
    printf("  n%lu -> n%lu [label=\"left\"];\n"
           "  n%lu -> n%lu [label=\"right\"];\n",
        (unsigned long)pos, (unsigned long)left,
        (unsigned long)pos, (unsigned long)right);
    printf("  n%lu [label=\"%lu,", (unsigned long)pos,
        (unsigned long)bit);
    printbits((char*)(tp->mem + pos + 20), klen);
    printf("\"];\n");
    pos += 4 + 8 + 8 + klen + vlen;
  }
  printf("}\n");
}
