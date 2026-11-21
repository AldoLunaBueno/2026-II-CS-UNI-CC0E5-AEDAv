#include "hash_cpp_bridge.h"
#include "containers/hash_table.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct HashCppHandle {
    HashTable<HashTableTraits<std::string, size_t>> table;
    std::vector<std::vector<HashCppTid>> buckets;
};

extern "C" {

HashCppHandle *hash_cpp_create(void) {
    return new HashCppHandle();
}

void hash_cpp_destroy(HashCppHandle *handle) {
    delete handle;
}

void hash_cpp_insert_key(HashCppHandle *handle, const char *key, uint32_t block, uint16_t offset) {
    if (!handle || !key) return;

    std::string k(key);
    HashCppTid tid{block, offset};

    if (handle->table.contains(k)) {
        size_t bucket_id = handle->table[k];
        handle->buckets[bucket_id].push_back(tid);
        return;
    }

    size_t new_bucket = handle->buckets.size();
    handle->buckets.push_back({tid});
    handle->table.push_back(HashEntry<std::string, size_t>(k, new_bucket));
}

HashCppResult hash_cpp_search(HashCppHandle *handle, const char *key) {
    HashCppResult res{nullptr, 0};
    if (!handle || !key) return res;

    std::string k(key);
    if (!handle->table.contains(k))
        return res;

    size_t bucket_id = handle->table[k];
    if (bucket_id < handle->buckets.size()) {
        const auto &tids = handle->buckets[bucket_id];
        res.count = tids.size();
        if (res.count > 0) {
            res.tids = static_cast<HashCppTid *>(std::malloc(res.count * sizeof(HashCppTid)));
            std::memcpy(res.tids, tids.data(), res.count * sizeof(HashCppTid));
        }
    }
    return res;
}

void hash_cpp_free_result(HashCppResult *result) {
    if (result && result->tids) {
        std::free(result->tids);
        result->tids = nullptr;
        result->count = 0;
    }
}

} // extern "C"
