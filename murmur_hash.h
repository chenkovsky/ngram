#ifndef __MURMUR_HASH_H__
#define __MURMUR_HASH_H__ 1
#include <stdint.h>
#include <stddef.h>
uint64_t MurmurHash64( const void * key, size_t len, uint64_t seed);
#endif

