/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#ifndef RADIXDB_H
#define RADIXDB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RADIXDB_MAX_KEY_LENGTH 0x0ffffffful

struct radixdb {
  unsigned char* mem;  /* used memory */
  uint32_t dend;       /* end of data ptr */
};

void radixdb_free(struct radixdb* tp);

/* Check if the RadixDB structure is valid (i.e. if the loaded database is not
 * corrupted). This is a O(n) operation.
 */
int radixdb_check(struct radixdb* tp);

int radixdb_lookup(const struct radixdb* tp,
                   const char *key, size_t klen,
                   const char **val, size_t *vlen);

int radixdb_longest_match(const struct radixdb* tp,
                          const char *key, size_t klen,
                          const char **keymatch, size_t *matchlen,
                          const char **val, size_t *vlen);

/* Simple data iterator. Returns key-values in the order they were added. The
 * iterator parameter must be initialized with 4.
 */
int radixdb_iter_next(const struct radixdb* tp,
                      uint32_t *iterator,
                      const char **key, size_t *klen,
                      const char **val, size_t *vlen);

/* radixdb_make */

struct radixdb_node {
  int b;
  char *key, *val;
  size_t klen, vlen;
  struct radixdb_node *left, *right;
};

struct radixdb_make {
  struct radixdb_node head;
  unsigned char* mem;  /* used memory */
  uint32_t size;       /* memory size */
  uint32_t dend;       /* end of data ptr */
};

int radixdb_make_start(struct radixdb_make* tpm);
int radixdb_make_add(struct radixdb_make* tpm,
                     const char *key, size_t klen,
                     const char *val, size_t vlen);
int radixdb_make_finish(struct radixdb_make *tpm, struct radixdb* tp);

#ifdef __cplusplus
}
#endif

#endif
