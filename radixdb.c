/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "radixdb.h"

#define pos(t, p) ((p) - (tp)->mem)
#define empty(t) ((t)->dend == 4)
#define set_root(t, pos) pack((t)->mem, pos)
#define set_bit(x, i) pack(x, i)
#define set_leftpos(x, i) pack((x) + 4, i)
#define set_rightpos(x, i) pack((x) + 8, i)
#define set_keylen(x, i) pack((x) + 12, i)
#define set_datalen(x, i) pack((x) + 16, i)
#define root(t) unpack((t)->mem)
#define bit(x) unpack(x)
#define leftpos(tp, x) ((tp)->mem + unpack((x) + 4))
#define rightpos(tp, x) ((tp)->mem + unpack((x) + 8))
#define keylen(x) unpack((x) + 12)
#define datalen(x) unpack((x) + 16)
#define keystart(x) ((x) + 20)
#define datastart(x, klen) ((x) + 20 + (klen))
#define nodesize(klen, vlen) (4 + 8 + 8 + (klen) + (vlen))

static void
pack(unsigned char* buf, uint32_t i) {
  buf[0] = i & 255; i >>= 8;
  buf[1] = i & 255; i >>= 8;
  buf[2] = i & 255;
  buf[3] = i >> 8;
}

static uint32_t
unpack(const unsigned char* buf) {
  uint32_t n = buf[3];
  n <<= 8; n |= buf[2];
  n <<= 8; n |= buf[1];
  n <<= 8; n |= buf[0];
  return n;
}

static int
get_bit(uint32_t b, const char* key, uint32_t klen) {
  int index = b >> 3;
  if (index >= klen)
    return 0;
  return key[index] & (1 << (7 - (b % 8)));
}

static void
printbits(const char *b, uint32_t blen) {
  uint32_t i, j;
  for (i = 0; i < blen; i++) {
    for (j = 0; j < 8; j++) {
      putchar(get_bit(j, b + i, blen) ? '1' : '0');
    }
    putchar(' ');
  }
}

static int
diff_bit(const char* a, uint32_t alen,
         const char* b, uint32_t blen) {
  unsigned char c1, c2, mask;
  uint32_t i, j, todo = alen > blen ? alen : blen;

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

static int
is_leaf(const unsigned char *prev, const unsigned char *p) {
  if (prev == NULL)
    return 0;
  return bit(p) <= bit(prev);
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
  uint32_t new_size = 1 << log2ceil(tp->dend + extra_size);
  if (new_size > tp->size) {
    unsigned char* mem = (unsigned char*) realloc(tp->mem, new_size);
    if (!mem)
      return -1;
    tp->size = new_size;
    tp->mem = mem;
  }
  return 0;
}

static uint32_t
insert_between(struct radixdb* tp,
    unsigned char *p, unsigned char *n, int diff, unsigned char *prev) {
  if (is_leaf(prev, p) || diff < (int)bit(p)) {
    if (get_bit(diff, keystart(n), keylen(n)))
      set_leftpos(n, pos(tp, p));
    else
      set_rightpos(n, pos(tp, p));
    return pos(tp, n);
  }
  if (get_bit(bit(p), keystart(n), keylen(n)))
    set_rightpos(p, insert_between(tp, rightpos(tp, p), n, diff, p));
  else
    set_leftpos(p, insert_between(tp, leftpos(tp, p), n, diff, p));
  return pos(tp, p);
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
  unsigned char *n, *p;
  uint32_t b0;
  int diff;

  if (ensure_capacity(tp, nodesize(klen, vlen)) == -1)
    return -1;

  /* copy the key and value to the new node */
  n = tp->mem + tp->dend;
  set_bit(n, (klen << 3) - 1);
  set_leftpos(n, tp->dend);
  set_rightpos(n, tp->dend);
  set_keylen(n, klen);
  set_datalen(n, vlen);
  memcpy(keystart(n), key, klen);
  memcpy(datastart(n, klen), val, vlen);

  if (empty(tp)) {
    /* first node inserted */
    set_root(tp, tp->dend);
  } else {
    /* find insert position */
    p = tp->mem + root(tp);
    do {
      b0 = bit(p);
      if (get_bit(bit(p), key, klen))
        p = rightpos(tp, p);
      else
        p = leftpos(tp, p);
    } while (bit(p) > b0);
    diff = diff_bit(
        keystart(p), keylen(p), key, klen);
    if (diff == -1) {
      errno = EEXIST;
      return -1;  /* entry already exists */
    }
    set_bit(n, diff);
    set_root(tp, insert_between(tp, tp->mem + root(tp), n, diff, NULL));
  }

  tp->dend += nodesize(klen, vlen);
  return 0;
}

int radixdb_lookup(const struct radixdb* tp,
                   const char *key, uint32_t klen,
                   const char **val, uint32_t *vlen) {
  unsigned char *p;
  uint32_t b0;

  if (!empty(tp)) {
    p = tp->mem + root(tp);
    do {
      b0 = bit(p);
      if (get_bit(bit(p), key, klen))
        p = rightpos(tp, p);
      else
        p = leftpos(tp, p);
    } while (bit(p) > b0);
 
    if (klen == keylen(p) && memcmp(keystart(p), key, klen) == 0) {
      *val = datastart(p, klen);
      *vlen = datalen(p);
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}

int radixdb_longest_match(const struct radixdb* tp,
                          const char *key, uint32_t klen,
                          const char **val, uint32_t *vlen) {

}

void radixdb_dump(const struct radixdb* tp) {
  unsigned char *p = tp->mem + root(tp);
  printf("digraph G {\n");
  while (pos(tp, p) < tp->dend) {
    printf("  n%ld -> n%ld [label=\"left\"];\n"
           "  n%ld -> n%ld [label=\"right\"];\n",
        pos(tp, p), pos(tp, leftpos(tp, p)),
        pos(tp, p), pos(tp, rightpos(tp, p)));
    printf("  n%ld [label=\"%u,", pos(tp, p), bit(p));
    printbits((char*)keystart(p), keylen(p));
    printf("\"];\n");
    p += nodesize(keylen(p), datalen(p));
  }
  printf("}\n");
}
