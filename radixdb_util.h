/* This file is a part of radixdb package by G. B. Versiani, guibv@yahoo.com.
 * Public domain.
 */

#ifndef RADIXDB_UTIL_H
#define RADIXDB_UTIL_H

#include "radixdb.h"

#ifdef __cplusplus
extern "C" {
#endif

void openfile(const char* filename, struct radixdb* db);
void closefile(struct radixdb* db);

void radixdb_dump2dot(const struct radixdb* tp);

void radixdb_dump(const struct radixdb* tp);

void radixdb_stats(const struct radixdb* tp);


#ifdef __cplusplus
}
#endif

#endif
