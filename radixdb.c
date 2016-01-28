/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "radixdb.h"

#define set_bit(x, i) pack(x, i)
#define set_leftpos(x, i) pack((x) + 4, i)
#define set_rightpos(x, i) pack((x) + 8, i)
#define set_keylen(x, i) pack((x) + 12, i)
#define set_datalen(x, i) pack((x) + 16, i)
#define bit(x) unpack(x)
#define leftpos(x) unpack((x) + 4)
#define rightpos(x) unpack((x) + 8)
#define keylen(x) unpack((x) + 12)
#define datalen(x) unpack((x) + 16)
#define keystart(x) ((x) + 20)
#define datastart(x, klen) ((x) + 20 + (klen))
#define nodesize(klen, vlen) (4 + 8 + 8 + (klen) + (vlen))

static void pack(unsigned char* buf, uint32_t i) {
  buf[0] = i & 255; i >>= 8;
  buf[1] = i & 255; i >>= 8;
  buf[2] = i & 255;
  buf[3] = i >> 8;
}

static uint32_t unpack(const unsigned char* buf) {
  uint32_t n = buf[3];
  n <<= 8; n |= buf[2];
  n <<= 8; n |= buf[1];
  n <<= 8; n |= buf[0];
  return n;
}

static int get_bit(uint32_t b, const char* key, uint32_t klen) {
  int index = b >> 3;
  if (index >= klen)
    return 0;
  return key[index] & (1 << (7 - (b % 8)));
}

static void printbits(const char *b, uint32_t blen) {
  uint32_t i, j;
  for (i = 0; i < blen; i++) {
    for (j = 0; j < 8; j++) {
      putchar(get_bit(j, b + i, blen) ? '1' : '0');
    }
    putchar(' ');
  }
}

static int diff_bit(const char* a, uint32_t alen,
                    const char* b, uint32_t blen) {
  char c1, c2, mask;
  uint32_t i, j, todo = alen > blen ? alen : blen;

  for (i = 0; i < todo; i++) {
    c1 = (i < alen) ? a[i] : 0;
    c2 = (i < blen) ? b[i] : 0;
    if (c1 != c2) {
      mask = ~(~c1 ^ c2);
      break;
    }
  }

  if (i == alen && alen == blen)
    return -1;  /* same entry */

  for (j = 0; j < 8; j++) {
    if (mask & (1 << (7 - (j % 8))))
      break;
  }

  return (i << 3) + j;
}

int is_leaf(struct radixdb* tp, uint32_t prevpos, uint32_t pos) {
  if (prevpos == 0xffffffff)
    return 0;
  return bit(tp->mem + pos) <= bit(tp->mem + prevpos);
}

static uint32_t log2ceil(uint32_t i) {
  uint32_t l = 0;
  if (i & (i - 1)) { l += 1; }
  if (i >> 16) { l += 16; i >>= 16; }
  if (i >> 8)  { l += 8; i >>= 8; }
  if (i >> 4)  { l += 4; i >>= 4; }
  if (i >> 2)  { l += 2; i >>= 2; }
  if (i >> 1)  { l += 1; }
  return l;
}

static int ensure_capacity(struct radixdb* tp, uint32_t extra_size) {
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

int radixdb_init(struct radixdb* tp) {
  tp->mem = NULL;
  tp->size = 0ul;
  tp->dend = 4;
  if (ensure_capacity(tp, 2000) == -1)
    return -1;
  *(uint32_t*)tp->mem = 0xfffffffful;  /* empty root */
  return 0;
}

void radixdb_free(struct radixdb* tp) {
  free(tp->mem);
}

static uint32_t insert_between(struct radixdb* tp,
    uint32_t pos, unsigned char* n, int diff, uint32_t prevpos) {
  if (is_leaf(tp, prevpos, pos)
      || diff < (int)bit(tp->mem + pos)) {
    if (get_bit(diff, keystart(n), keylen(n))) {
      set_leftpos(n, pos);
    } else {
      set_rightpos(n, pos);
    }
    set_bit(n, diff);
    return n - tp->mem;
  }
  if (get_bit(bit(tp->mem + pos), keystart(n), keylen(n))) {
    set_rightpos(tp->mem + pos,
        insert_between(tp, rightpos(tp->mem + pos), n, diff, pos));
  } else {
    set_leftpos(tp->mem + pos,
        insert_between(tp, leftpos(tp->mem + pos), n, diff, pos));
  }
  return pos;
}

int radixdb_add(struct radixdb* tp,
                const char *key, uint32_t klen,
                const char *val, uint32_t vlen) {
  unsigned char* n;
  uint32_t b0, pos;
  int diff;

  if (ensure_capacity(tp, nodesize(klen, vlen)) == -1)
    return -1;

  n = tp->mem + tp->dend;

  /* copy the key and value to the new node */
  set_bit(n, (klen << 3) - 1);
  set_leftpos(n, tp->dend);
  set_rightpos(n, tp->dend);
  set_keylen(n, klen);
  set_datalen(n, vlen);
  memcpy(keystart(n), key, klen);
  memcpy(datastart(n, klen), val, vlen);
  tp->dend += nodesize(klen, vlen);

  if (*(uint32_t*)tp->mem == 0xfffffffful) {
    /* first node inserted */
    *(uint32_t*)tp->mem = 4;
    return 0;
  }

  pos = *(uint32_t*)tp->mem;
  do {
    b0 = bit(tp->mem + pos);
    if (get_bit(bit(tp->mem + pos), key, klen))
      pos = rightpos(tp->mem + pos);
    else
      pos = leftpos(tp->mem + pos);
  } while (bit(tp->mem + pos) > b0);

  diff = diff_bit(
      keystart(tp->mem + pos), keylen(tp->mem + pos), key, klen);
  if (diff == -1) {
    errno = EEXIST;
    return -1;  /* entry already exists */
  }

  *(uint32_t*)tp->mem = insert_between(tp, *(uint32_t*)tp->mem,
      n, diff, 0xffffffff);
  return 0;
}

int radixdb_lookup(const struct radixdb* tp,
                   const char *key, uint32_t klen,
                   const char **val, uint32_t *vlen) {
}

int radixdb_best_match(const struct radixdb* tp,
                       const char *key, uint32_t klen,
                       const char **val, uint32_t *vlen) {
}

void radixdb_dump(const struct radixdb* tp) {
  uint32_t pos = 4;
  printf("digraph G {\n");
  if (pos != 0xfffffffful) {
    while (pos < tp->dend) {
      printf("  n%u -> n%u [label=\"left\"];\n"
             "  n%u -> n%u [label=\"right\"];\n",
          pos, leftpos(tp->mem + pos),
          pos, rightpos(tp->mem + pos));
      printf("  n%u [label=\"%u,", pos, bit(tp->mem + pos));
      printbits((char*)keystart(tp->mem + pos), keylen(tp->mem + pos));
      printf("\"];\n");
      pos += nodesize(keylen(tp->mem + pos), datalen(tp->mem + pos));
    }
  }
  printf("}\n");
}
