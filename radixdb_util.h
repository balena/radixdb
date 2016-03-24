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

#ifdef __cplusplus
}
#endif

#endif
