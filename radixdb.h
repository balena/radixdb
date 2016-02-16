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
  uint32_t size;       /* memory size */
  uint32_t dend;       /* end of data ptr */
};

int radixdb_init(struct radixdb* tp);
void radixdb_free(struct radixdb* tp);

/* Check if the RadixDB structure is valid (i.e. if the loaded database is not
 * corrupted). This is a O(n) operation.
 */
int radixdb_check(struct radixdb* tp);

int radixdb_add(struct radixdb* tp,
                const char *key, size_t klen,
                const char *val, size_t vlen);

int radixdb_lookup(const struct radixdb* tp,
                   const char *key, size_t klen,
                   const char **val, size_t *vlen);

int radixdb_longest_match(const struct radixdb* tp,
                          const char *key, size_t klen,
                          const char **keymatch, size_t *matchlen,
                          const char **val, size_t *vlen);

void radixdb_dump2dot(const struct radixdb* tp);

void radixdb_dump(const struct radixdb* tp);

/* Simple sata iterator. Returns key-values in the order they were added. The
 * iterator parameter must be initialized with 4.
 */
int radixdb_iter_next(const struct radixdb* tp,
                      uint32_t *iterator,
                      const char **key, size_t *klen,
                      const char **val, size_t *vlen);

#ifdef __cplusplus
}
#endif

#endif
