#include "btree_cpp_bridge.h"
#include "BTree.h"
#include "../types.h"

#include <cstdlib>
#include <cstring>
#include <vector>

struct BTreeCppHandle {
    BTree<double, long> tree;
    std::vector<std::vector<BTreeCppTid>> buckets;

    explicit BTreeCppHandle(TX order) : tree(order, true) {}
};

namespace {

struct RangeCollectCtx {
    BTreeCppHandle *handle;
    TX has_min;
    double min_val;
    TX min_inclusive;
    TX has_max;
    double max_val;
    TX max_inclusive;
    std::vector<BTreeCppTid> out;
};

void collect_if_in_range(BTree<double, long>::ObjectInfo &info, TX /*level*/, void *pExtra) {
    auto *ctx = static_cast<RangeCollectCtx *>(pExtra);
    double k = info.key;

    if (ctx->has_min) {
        if (ctx->min_inclusive ? (k < ctx->min_val) : (k <= ctx->min_val))
            return;
    }
    if (ctx->has_max) {
        if (ctx->max_inclusive ? (k > ctx->max_val) : (k >= ctx->max_val))
            return;
    }

    TX bucket_id = static_cast<TX>(info.ObjID);
    if (bucket_id >= 0 && static_cast<size_t>(bucket_id) < ctx->handle->buckets.size()) {
        const auto &tids = ctx->handle->buckets[bucket_id];
        ctx->out.insert(ctx->out.end(), tids.begin(), tids.end());
    }
}

} // namespace

extern "C" {

BTreeCppHandle *btree_cpp_create(TX order) {
    return new BTreeCppHandle(order > 0 ? order : DEFAULT_BTREE_ORDER);
}

void btree_cpp_destroy(BTreeCppHandle *handle) {
    delete handle;
}

void btree_cpp_insert_key(BTreeCppHandle *handle, double key, uint32_t block, uint16_t offset) {
    if (!handle) return;

    BTreeCppTid tid{block, offset};
    long existing_bucket = handle->tree.Search(key);
    if (existing_bucket >= 0 && static_cast<size_t>(existing_bucket) < handle->buckets.size()) {
        handle->buckets[existing_bucket].push_back(tid);
        return;
    }

    long new_bucket = static_cast<long>(handle->buckets.size());
    handle->buckets.push_back({tid});
    handle->tree.Insert(key, static_cast<TX>(new_bucket));
}

BTreeCppResult btree_cpp_scan_range(
    BTreeCppHandle *handle,
    TX has_min, double min_val, TX min_inclusive,
    TX has_max, double max_val, TX max_inclusive
) {
    BTreeCppResult res{nullptr, 0};
    if (!handle) return res;

    // Caso puntual exacto: min == max inclusivo -> usar BTree::Search directamente
    if (has_min && has_max && min_inclusive && max_inclusive && min_val == max_val) {
        long bucket_id = handle->tree.Search(min_val);
        if (bucket_id >= 0 && static_cast<size_t>(bucket_id) < handle->buckets.size()) {
            const auto &tids = handle->buckets[bucket_id];
            res.count = tids.size();
            if (res.count > 0) {
                res.tids = static_cast<BTreeCppTid *>(std::malloc(res.count * sizeof(BTreeCppTid)));
                std::memcpy(res.tids, tids.data(), res.count * sizeof(BTreeCppTid));
            }
        }
        return res;
    }

    RangeCollectCtx ctx{handle, has_min, min_val, min_inclusive, has_max, max_val, max_inclusive, {}};
    handle->tree.ForEach(
        reinterpret_cast<BTree<double, long>::lpfnForEach2>(&collect_if_in_range),
        &ctx
    );

    res.count = ctx.out.size();
    if (res.count > 0) {
        res.tids = static_cast<BTreeCppTid *>(std::malloc(res.count * sizeof(BTreeCppTid)));
        std::memcpy(res.tids, ctx.out.data(), res.count * sizeof(BTreeCppTid));
    }
    return res;
}

void btree_cpp_free_result(BTreeCppResult *result) {
    if (result && result->tids) {
        std::free(result->tids);
        result->tids = nullptr;
        result->count = 0;
    }
}

} // extern "C"
