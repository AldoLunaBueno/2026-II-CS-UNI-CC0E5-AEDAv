#ifndef HASH_CPP_BRIDGE_H
#define HASH_CPP_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HashCppHandle HashCppHandle;

typedef struct HashCppTid {
    uint32_t block;
    uint16_t offset;
} HashCppTid;

typedef struct HashCppResult {
    HashCppTid *tids;
    size_t count;
} HashCppResult;

HashCppHandle *hash_cpp_create(void);
void hash_cpp_destroy(HashCppHandle *handle);
void hash_cpp_insert_key(HashCppHandle *handle, const char *key, uint32_t block, uint16_t offset);
HashCppResult hash_cpp_search(HashCppHandle *handle, const char *key);
void hash_cpp_free_result(HashCppResult *result);

#ifdef __cplusplus
}
#endif

#endif
