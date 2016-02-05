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

static uint8_t
leftmost_bit(uint8_t v) {
  static uint8_t debruijn[8] = {7, 2, 6, 1, 3, 4, 5, 0};
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  return debruijn[(uint8_t)(0x1du * v) >> 5];
}

static int
diff_bit(const unsigned char* key, uint32_t klen,
         const unsigned char* prefix, uint32_t prefixlen) {
  int r = 0;
  unsigned char u1, u2 = 0;

  /* Compare bytes first, until diff byte */ 
  for (; klen-- && prefixlen--; key++, prefix++, r++) {
    u1 = *key;
    u2 = *prefix;
    if (u1 != u2) {
      return (r << 3) + leftmost_bit(~(~u1 ^ u2));
    }
  }

  if (klen == 0 && prefixlen == 0) {
    return -1;  /* Same entry */
  } else if (prefixlen == 0) {
    return r << 3;  /* Prefix is smaller than key */
  } else {
    /* Prefix is larger than key */
    return (r << 3) + leftmost_bit(u2);
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
ensure_capacity(struct radixdb* tp, size_t extra_size) {
  unsigned char* mem;
  uint32_t new_size;
  if ((uint32_t)(tp->dend + extra_size) < tp->dend) {
    return -1;  /* memory overflow */
  } else if ((uint32_t)(tp->dend + extra_size) > 0x80000000ul) {
    new_size = 0xfffffffful;  /* allocate max size */
  } else {
    new_size = 1 << log2ceil(tp->dend + extra_size);
  }
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
find_prefix(const char *a, uint32_t alen, const unsigned char *b, uint32_t blen) {
  uint32_t r = 0;
  unsigned char u1, u2;

  for (; alen-- && blen--; a++, b++, r++) {
    u1 = *(unsigned char*)a;
    u2 = *b;
    if (u1 != u2)
      break;
  }

  return r;
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
  nextmatch = search_largest_prefix_inner(tp, key, klen, nextpos, b1,
      max_length);
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
                const char *key, size_t klen,
                const char *val, size_t vlen) {
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
        (const unsigned char*)key, klen);
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
                   const char *key, size_t klen,
                   const char **val, size_t *vlen) {
  uint32_t pos;

  if (tp->dend > 4) {
    pos = search_node(tp, key, klen);
    if (klen == uint32_unpack(tp->mem + pos + 12)
        && memcmp(tp->mem + pos + 20, key, klen) == 0) {
      *val = (const char*)(tp->mem + pos + 20 + klen);
      *vlen = uint32_unpack(tp->mem + pos + 16);
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}

int radixdb_longest_match(const struct radixdb* tp,
                          const char *key, size_t klen,
                          const char **keymatch, size_t *matchlen,
                          const char **val, size_t *vlen) {
  uint32_t pos;

  if (tp->dend > 4) {
    pos = search_largest_prefix(tp, key, klen);
    if (pos != 0xfffffffful) {
      *keymatch = (const char*)(tp->mem + pos + 20);
      *matchlen = uint32_unpack(tp->mem + pos + 12);
      *val = (const char*)(tp->mem + pos + 20 + *matchlen);
      *vlen = uint32_unpack(tp->mem + pos + 16);
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}

void radixdb_dump2dot(const struct radixdb* tp) {
  uint32_t pos = 4, bit, left, right, klen, vlen;
  printf("digraph G {\n");
  printf("  node [shape=record];\n");
  while (pos < tp->dend) {
    bit = uint32_unpack(tp->mem + pos);
    left = uint32_unpack(tp->mem + pos + 4);
    right = uint32_unpack(tp->mem + pos + 8);
    klen = uint32_unpack(tp->mem + pos + 12);
    vlen = uint32_unpack(tp->mem + pos + 16);
    printf("  n%lu [label=\"{{%lu|%lu|\\\"%.*s\\\"&rarr;\\\"%.*s\\\"}"
           "|{<l>left|<r>right}}\"];\n",
        (unsigned long)pos,
        (unsigned long)(bit>>3), (unsigned long)(bit&7),
        (int)klen, (const char*)(tp->mem + pos + 20),
        (int)vlen, (const char*)(tp->mem + pos + 20 + klen));
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
