#include "postgres.h"

#include "access/amapi.h"
#include "access/genam.h"
#include "access/hash.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/table.h"
#include "access/tableam.h"
#include "catalog/index.h"
#include "catalog/pg_type.h"
#include "commands/vacuum.h"
#include "fmgr.h"
#include "nodes/execnodes.h"
#include "optimizer/cost.h"
#include "storage/itemptr.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"

#include <stdio.h>

#include "hash_cpp_bridge.h"

PG_MODULE_MAGIC;

typedef struct HashCppRegistryEntry {
    Oid index_oid;
    HashCppHandle *handle;
    struct HashCppRegistryEntry *next;
} HashCppRegistryEntry;

static HashCppRegistryEntry *registry_head = NULL;

static HashCppHandle *registry_get(Oid index_oid) {
    for (HashCppRegistryEntry *e = registry_head; e != NULL; e = e->next) {
        if (e->index_oid == index_oid)
            return e->handle;
    }
    return NULL;
}

static void registry_set(Oid index_oid, HashCppHandle *handle) {
    for (HashCppRegistryEntry *e = registry_head; e != NULL; e = e->next) {
        if (e->index_oid == index_oid) {
            if (e->handle)
                hash_cpp_destroy(e->handle);
            e->handle = handle;
            return;
        }
    }
    HashCppRegistryEntry *e = (HashCppRegistryEntry *) malloc(sizeof(HashCppRegistryEntry));
    e->index_oid = index_oid;
    e->handle = handle;
    e->next = registry_head;
    registry_head = e;
}

static char *datum_to_key_cstring(Oid typid, Datum val, char *buf, size_t buf_sz) {
    if (typid == TEXTOID || typid == VARCHAROID || typid == BPCHAROID)
        return text_to_cstring(DatumGetTextPP(val));
    if (typid == INT4OID) {
        snprintf(buf, buf_sz, "%d", DatumGetInt32(val));
        return buf;
    }
    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("hash_cpp only supports text, varchar, and int4 keys")));
    return NULL;
}

typedef struct HashCppBuildState {
    HashCppHandle *handle;
    Oid key_typid;
    double indtuples;
} HashCppBuildState;

static void hash_cpp_build_callback(Relation index,
                                    ItemPointer tid,
                                    Datum *values,
                                    bool *isnull,
                                    bool tupleIsAlive,
                                    void *state) {
    HashCppBuildState *bstate = (HashCppBuildState *) state;
    if (isnull[0])
        return;

    char buf[32];
    char *key = datum_to_key_cstring(bstate->key_typid, values[0], buf, sizeof(buf));
    hash_cpp_insert_key(bstate->handle, key,
                        ItemPointerGetBlockNumber(tid),
                        ItemPointerGetOffsetNumber(tid));
    if (key != buf)
        pfree(key);
    bstate->indtuples += 1.0;
}

static HashCppHandle *ensure_loaded(Relation heap, Relation index) {
    Oid index_oid = RelationGetRelid(index);
    HashCppHandle *handle = registry_get(index_oid);
    if (handle != NULL)
        return handle;

    handle = hash_cpp_create();
    HashCppBuildState bstate;
    bstate.handle = handle;
    bstate.key_typid = TupleDescAttr(RelationGetDescr(index), 0)->atttypid;
    bstate.indtuples = 0.0;

    IndexInfo *indexInfo = BuildIndexInfo(index);
    table_index_build_scan(heap, index, indexInfo, true, false,
                           hash_cpp_build_callback, &bstate, NULL);

    registry_set(index_oid, handle);
    return handle;
}

static IndexBuildResult *hash_cpp_build(Relation heap, Relation index, IndexInfo *indexInfo) {
    HashCppHandle *handle = hash_cpp_create();
    HashCppBuildState bstate;
    bstate.handle = handle;
    bstate.key_typid = TupleDescAttr(RelationGetDescr(index), 0)->atttypid;
    bstate.indtuples = 0.0;

    double reltuples = table_index_build_scan(heap, index, indexInfo, true, true,
                                              hash_cpp_build_callback, &bstate, NULL);

    registry_set(RelationGetRelid(index), handle);

    IndexBuildResult *result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
    result->heap_tuples = reltuples;
    result->index_tuples = bstate.indtuples;
    return result;
}

static void hash_cpp_buildempty(Relation index) {
    Oid index_oid = RelationGetRelid(index);
    if (registry_get(index_oid) == NULL)
        registry_set(index_oid, hash_cpp_create());
}

static bool hash_cpp_insert(Relation index,
                            Datum *values,
                            bool *isnull,
                            ItemPointer ht_ctid,
                            Relation heapRel,
                            IndexUniqueCheck checkUnique,
                            bool indexUnchanged,
                            IndexInfo *indexInfo) {
    if (isnull[0])
        return false;

    HashCppHandle *handle = ensure_loaded(heapRel, index);
    Oid key_typid = TupleDescAttr(RelationGetDescr(index), 0)->atttypid;
    char buf[32];
    char *key = datum_to_key_cstring(key_typid, values[0], buf, sizeof(buf));
    hash_cpp_insert_key(handle, key,
                        ItemPointerGetBlockNumber(ht_ctid),
                        ItemPointerGetOffsetNumber(ht_ctid));
    if (key != buf)
        pfree(key);
    return true;
}

static IndexBulkDeleteResult *hash_cpp_bulkdelete(IndexVacuumInfo *info,
                                                  IndexBulkDeleteResult *stats,
                                                  IndexBulkDeleteCallback callback,
                                                  void *callback_state) {
    if (stats == NULL)
        stats = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
    return stats;
}

static IndexBulkDeleteResult *hash_cpp_vacuumcleanup(IndexVacuumInfo *info,
                                                     IndexBulkDeleteResult *stats) {
    if (stats == NULL)
        stats = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
    return stats;
}

static void hash_cpp_costestimate(PlannerInfo *root,
                                  IndexPath *path,
                                  double loop_count,
                                  Cost *indexStartupCost,
                                  Cost *indexTotalCost,
                                  Selectivity *indexSelectivity,
                                  double *indexCorrelation,
                                  double *indexPages) {
    GenericCosts costs = {0};
    genericcostestimate(root, path, loop_count, &costs);
    *indexStartupCost = costs.indexStartupCost;
    *indexTotalCost = costs.indexTotalCost;
    *indexSelectivity = costs.indexSelectivity;
    *indexCorrelation = costs.indexCorrelation;
    *indexPages = costs.numIndexPages;
}

static bool hash_cpp_validate(Oid opclassoid) {
    return true;
}

typedef struct HashCppScanOpaque {
    HashCppResult result;
    size_t pos;
    bool executed;
} HashCppScanOpaque;

static IndexScanDesc hash_cpp_beginscan(Relation index, int nkeys, int norderbys) {
    IndexScanDesc scan = RelationGetIndexScan(index, nkeys, norderbys);
    HashCppScanOpaque *so = (HashCppScanOpaque *) palloc0(sizeof(HashCppScanOpaque));
    scan->opaque = so;
    return scan;
}

static void hash_cpp_rescan(IndexScanDesc scan, ScanKey scankey, int nscankeys,
                            ScanKey orderbys, int norderbys) {
    HashCppScanOpaque *so = (HashCppScanOpaque *) scan->opaque;
    hash_cpp_free_result(&so->result);
    so->pos = 0;
    so->executed = false;

    if (scankey && scan->numberOfKeys > 0)
        memmove(scan->keyData, scankey, scan->numberOfKeys * sizeof(ScanKeyData));
}

static bool hash_cpp_gettuple(IndexScanDesc scan, ScanDirection dir) {
    HashCppScanOpaque *so = (HashCppScanOpaque *) scan->opaque;

    if (!so->executed) {
        if (scan->numberOfKeys == 1 && !(scan->keyData[0].sk_flags & SK_ISNULL) &&
            scan->keyData[0].sk_strategy == HTEqualStrategyNumber) {
            HashCppHandle *handle = ensure_loaded(scan->heapRelation, scan->indexRelation);
            Oid key_typid = TupleDescAttr(RelationGetDescr(scan->indexRelation), 0)->atttypid;
            char buf[32];
            char *key = datum_to_key_cstring(key_typid, scan->keyData[0].sk_argument, buf, sizeof(buf));
            so->result = hash_cpp_search(handle, key);
            if (key != buf)
                pfree(key);
        }
        so->pos = 0;
        so->executed = true;
    }

    if (so->pos >= so->result.count)
        return false;

    HashCppTid tid = so->result.tids[so->pos++];
    ItemPointerSet(&scan->xs_heaptid, tid.block, tid.offset);
    scan->xs_recheck = false;
    return true;
}

static void hash_cpp_endscan(IndexScanDesc scan) {
    HashCppScanOpaque *so = (HashCppScanOpaque *) scan->opaque;
    if (so) {
        hash_cpp_free_result(&so->result);
        pfree(so);
        scan->opaque = NULL;
    }
}

PG_FUNCTION_INFO_V1(hash_cpp_handler);

Datum hash_cpp_handler(PG_FUNCTION_ARGS) {
    IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

    amroutine->amstrategies = HTMaxStrategyNumber;
    amroutine->amsupport = 1;
    amroutine->amoptsprocnum = 0;
    amroutine->amcanorder = false;
    amroutine->amcanorderbyop = false;
    amroutine->amcanbackward = false;
    amroutine->amcanunique = false;
    amroutine->amcanmulticol = false;
    amroutine->amoptionalkey = false;
    amroutine->amsearcharray = false;
    amroutine->amsearchnulls = false;
    amroutine->amstorage = false;
    amroutine->amclusterable = false;
    amroutine->ampredlocks = false;
    amroutine->amcanparallel = false;
    amroutine->amcanbuildparallel = false;
    amroutine->amcaninclude = false;
    amroutine->amusemaintenanceworkmem = false;
    amroutine->amsummarizing = false;
    amroutine->amparallelvacuumoptions = VACUUM_OPTION_NO_PARALLEL;
    amroutine->amkeytype = InvalidOid;

    amroutine->ambuild = hash_cpp_build;
    amroutine->ambuildempty = hash_cpp_buildempty;
    amroutine->aminsert = hash_cpp_insert;
    amroutine->ambulkdelete = hash_cpp_bulkdelete;
    amroutine->amvacuumcleanup = hash_cpp_vacuumcleanup;
    amroutine->amcanreturn = NULL;
    amroutine->amcostestimate = hash_cpp_costestimate;
    amroutine->amoptions = NULL;
    amroutine->amproperty = NULL;
    amroutine->ambuildphasename = NULL;
    amroutine->amvalidate = hash_cpp_validate;
    amroutine->amadjustmembers = NULL;
    amroutine->ambeginscan = hash_cpp_beginscan;
    amroutine->amrescan = hash_cpp_rescan;
    amroutine->amgettuple = hash_cpp_gettuple;
    amroutine->amgetbitmap = NULL;
    amroutine->amendscan = hash_cpp_endscan;
    amroutine->ammarkpos = NULL;
    amroutine->amrestrpos = NULL;
    amroutine->amestimateparallelscan = NULL;
    amroutine->aminitparallelscan = NULL;
    amroutine->amparallelrescan = NULL;

    PG_RETURN_POINTER(amroutine);
}
