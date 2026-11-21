#ifndef BTREE_CPP_BRIDGE_H
#define BTREE_CPP_BRIDGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BTreeCppHandle BTreeCppHandle;

typedef struct BTreeCppTid {
    uint32_t block;
    uint16_t offset;
} BTreeCppTid;

typedef struct BTreeCppResult {
    BTreeCppTid *tids;
    size_t count;
} BTreeCppResult;

BTreeCppHandle *btree_cpp_create(int order);
void btree_cpp_destroy(BTreeCppHandle *handle);
void btree_cpp_insert_key(BTreeCppHandle *handle, double key, uint32_t block, uint16_t offset);

BTreeCppResult btree_cpp_scan_range(
    BTreeCppHandle *handle,
    int has_min, double min_val, int min_inclusive,
    int has_max, double max_val, int max_inclusive
);

void btree_cpp_free_result(BTreeCppResult *result);

#ifdef __cplusplus
}
#endif

#endif
