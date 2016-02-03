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

struct radixdb {
  unsigned char* mem;  /* used memory */
  uint32_t size;       /* memory size */
  uint32_t dend;       /* end of data ptr */
};

int radixdb_init(struct radixdb* tp);
void radixdb_free(struct radixdb* tp);

int radixdb_add(struct radixdb* tp,
                const char *key, uint32_t klen,
                const char *val, uint32_t vlen);

int radixdb_lookup(const struct radixdb* tp,
                   const char *key, uint32_t klen,
                   const char **val, uint32_t *vlen);

int radixdb_longest_match(const struct radixdb* tp,
                          const char *key, uint32_t klen,
                          const char **keymatch, uint32_t *matchlen,
                          const char **val, uint32_t *vlen);

void radixdb_dump2dot(const struct radixdb* tp);

void radixdb_dump(const struct radixdb* tp);

#ifdef __cplusplus
}
#endif

#endif
