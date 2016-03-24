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

static int
bit(uint32_t b, const char *key, uint32_t klen) {
  size_t index = b >> 3;
  if (index >= klen)
    return 0;
  return key[index] & (1 << (7 - (b & 7)));
}

static struct radixdb_node*
insert_between(struct radixdb_node *h, struct radixdb_node *n,
    int d, struct radixdb_node *p) {
  if ((h->b >= d) || (h->b <= p->b)) {
    n->b = d;
    n->left = bit(d, n->key, n->klen) ? h : n;
    n->right = bit(d, n->key, n->klen) ? n : h;
    return n;
  }

  if (bit(h->b, n->key, n->klen))
    h->right = insert_between(h->right, n, d, h);
  else
    h->left = insert_between(h->left, n, d, h);
  return h;
}

int radixdb_make_start(struct radixdb_make* tpm) {
  memset(tpm, 0, sizeof(*tpm));
  tpm->head.left = tpm->head.right = &tpm->head;
  return 0;
}

int radixdb_make_add(struct radixdb_make* tpm,
                     const char *key, size_t klen,
                     const char *val, size_t vlen) {
  size_t i;
  struct radixdb_node *t, *n, *head;

  n = (struct radixdb_node*) malloc(sizeof(*n));
  n->klen = klen;
  n->vlen = vlen;
  n->key = (char*) malloc(klen);
  memcpy(n->key, key, klen);
  if (vlen) {
    n->val = (char*) malloc(vlen);
    memcpy(n->val, val, vlen);
  } else {
    n->val = NULL;
  }
  tpm->dend += sizeof(*n) + klen + vlen;

  t = head = &tpm->head;
  do {
    i = t->b;
    t = bit(t->b, key, klen) ? t->right : t->left;
  } while (i < t->b);

  if (klen == t->klen && memcmp(key, t->key, klen) == 0) {
    errno = EEXIST;
    return -1;  /* entry already exists */
  }

  i = 0;
  while (i < (klen << 3)) {
    if (bit(i, key, klen) != bit(i, t->key, t->klen))
      break;
    i++;
  } 

  if (bit(head->b, key, klen))
    head->right = insert_between(head->right, n, i, head);
  else
    head->left = insert_between(head->left, n, i, head);
  return 0;
}

static size_t
memcount(struct radixdb_node *t, int b) {
  size_t size;
  if (t->b <= b)
    return 0;
  size = 4 + 8 + 8 + t->klen + t->vlen;
  size += memcount(t->left, t->b);
  size += memcount(t->right, t->b);
  return size;
}

static uint32_t
to_db(struct radixdb_node *t, int b, struct radixdb* db) {
  uint32_t n;

  if (t->b <= b)
    return 4;

  n = db->dend;
  uint32_pack(db->mem + n, t->b);
  uint32_pack(db->mem + n + 12, t->klen);
  uint32_pack(db->mem + n + 16, t->vlen);
  memcpy(db->mem + n + 20, t->key, t->klen);
  if (t->vlen > 0)
    memcpy(db->mem + n + 20 + t->klen, t->val, t->vlen);
  db->dend += 4 + 8 + 8 + t->klen + t->vlen;

  uint32_pack(db->mem + n + 4, to_db(t->left, t->b, db));
  uint32_pack(db->mem + n + 8, to_db(t->right, t->b, db));
  return n;
}

static void
free_nodes(struct radixdb_node *t, int b) {
  if (t->b <= b)
    return;

  free_nodes(t->left, t->b);
  free_nodes(t->right, t->b);

  free(t->key);
  free(t->val);
  free(t);
}

int radixdb_make_finish(struct radixdb_make* tpm, struct radixdb* tp) {
  uint32_t n;
  size_t memsize;

  memsize = memcount(&tpm->head, -1);
  tp->mem = (unsigned char*) malloc(4 + memsize);
  if (!tp->mem)
    return -1;

  tp->dend = 4;
  uint32_pack(tp->mem, 4);  /* point to root */

  n = tp->dend;
  uint32_pack(tp->mem + n, 0);
  uint32_pack(tp->mem + n + 12, 0);
  uint32_pack(tp->mem + n + 16, 0);
  tp->dend += 4 + 8 + 8;

  uint32_pack(tp->mem + n + 4, to_db(tpm->head.left, 0, tp));
  uint32_pack(tp->mem + n + 8, to_db(tpm->head.right, 0, tp));

  free_nodes(tpm->head.left, 0);
  free_nodes(tpm->head.right, 0);
  return 0;
}
