#ifndef BOOK_INDEX_H
#define BOOK_INDEX_H
#include <stdint.h>
#include "movegen.h"
// versioned little-endian records, shared by x86_64 and ARM64.
// Both current targets are little endian; never write native pointers.
#define BOOK_MAGIC "CBKIDX1\0"
typedef struct { char magic[8]; uint64_t start_key, count; } BookHeader;
typedef struct { uint64_t key; Move move; uint32_t weight; } BookRecord;
_Static_assert(sizeof(BookHeader)==24 && sizeof(BookRecord)==16, "book layout changed");
#endif
