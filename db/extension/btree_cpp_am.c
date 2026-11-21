#include "postgres.h"

#include "access/amapi.h"
#include "access/genam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/stratnum.h"
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

#include "btree_cpp_bridge.h"

PG_MODULE_MAGIC;

typedef struct BTreeCppRegistryEntry {
    Oid index_oid;
    BTreeCppHandle *handle;
    struct BTreeCppRegistryEntry *next;
} BTreeCppRegistryEntry;

static BTreeCppRegistryEntry *registry_head = NULL;

static BTreeCppHandle *registry_get(Oid index_oid) {
    for (BTreeCppRegistryEntry *e = registry_head; e != NULL; e = e->next) {
        if (e->index_oid == index_oid)
            return e->handle;
    }
    return NULL;
}

static void registry_set(Oid index_oid, BTreeCppHandle *handle) {
    for (BTreeCppRegistryEntry *e = registry_head; e != NULL; e = e->next) {
        if (e->index_oid == index_oid) {
            if (e->handle)
                btree_cpp_destroy(e->handle);
            e->handle = handle;
            return;
        }
    }
    BTreeCppRegistryEntry *e = (BTreeCppRegistryEntry *) malloc(sizeof(BTreeCppRegistryEntry));
    e->index_oid = index_oid;
    e->handle = handle;
    e->next = registry_head;
    registry_head = e;
}

static double datum_to_double(Oid typid, Datum val) {
    if (typid == INT4OID)
        return (double) DatumGetInt32(val);
    if (typid == FLOAT8OID)
        return DatumGetFloat8(val);
    ereport(ERROR,
            (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
             errmsg("btree_cpp only supports int4 and float8 keys")));
    return 0.0;
}

typedef struct BTreeCppBuildState {
    BTreeCppHandle *handle;
    Oid key_typid;
    double indtuples;
} BTreeCppBuildState;

static void btree_cpp_build_callback(Relation index,
                                     ItemPointer tid,
                                     Datum *values,
                                     bool *isnull,
                                     bool tupleIsAlive,
                                     void *state) {
    BTreeCppBuildState *bstate = (BTreeCppBuildState *) state;
    if (isnull[0])
        return;

    double key = datum_to_double(bstate->key_typid, values[0]);
    BlockNumber blk = ItemPointerGetBlockNumber(tid);
    OffsetNumber off = ItemPointerGetOffsetNumber(tid);
    btree_cpp_insert_key(bstate->handle, key, blk, off);
    bstate->indtuples += 1.0;
}

static BTreeCppHandle *ensure_loaded(Relation heap, Relation index) {
    Oid index_oid = RelationGetRelid(index);
    BTreeCppHandle *handle = registry_get(index_oid);
    if (handle != NULL)
        return handle;

    handle = btree_cpp_create(3);
    BTreeCppBuildState bstate;
    bstate.handle = handle;
    bstate.key_typid = TupleDescAttr(RelationGetDescr(index), 0)->atttypid;
    bstate.indtuples = 0.0;

    IndexInfo *indexInfo = BuildIndexInfo(index);
    table_index_build_scan(heap, index, indexInfo, true, false,
                           btree_cpp_build_callback, &bstate, NULL);

    registry_set(index_oid, handle);
    return handle;
}

static IndexBuildResult *btree_cpp_build(Relation heap, Relation index, IndexInfo *indexInfo) {
    BTreeCppHandle *handle = btree_cpp_create(3);
    BTreeCppBuildState bstate;
    bstate.handle = handle;
    bstate.key_typid = TupleDescAttr(RelationGetDescr(index), 0)->atttypid;
    bstate.indtuples = 0.0;

    double reltuples = table_index_build_scan(heap, index, indexInfo, true, true,
                                              btree_cpp_build_callback, &bstate, NULL);

    registry_set(RelationGetRelid(index), handle);

    IndexBuildResult *result = (IndexBuildResult *) palloc(sizeof(IndexBuildResult));
    result->heap_tuples = reltuples;
    result->index_tuples = bstate.indtuples;
    return result;
}

static void btree_cpp_buildempty(Relation index) {
    Oid index_oid = RelationGetRelid(index);
    if (registry_get(index_oid) == NULL)
        registry_set(index_oid, btree_cpp_create(3));
}

static bool btree_cpp_insert(Relation index,
                             Datum *values,
                             bool *isnull,
                             ItemPointer ht_ctid,
                             Relation heapRel,
                             IndexUniqueCheck checkUnique,
                             bool indexUnchanged,
                             IndexInfo *indexInfo) {
    if (isnull[0])
        return false;

    BTreeCppHandle *handle = ensure_loaded(heapRel, index);
    Oid key_typid = TupleDescAttr(RelationGetDescr(index), 0)->atttypid;
    double key = datum_to_double(key_typid, values[0]);
    btree_cpp_insert_key(handle, key,
                         ItemPointerGetBlockNumber(ht_ctid),
                         ItemPointerGetOffsetNumber(ht_ctid));
    return true;
}

static IndexBulkDeleteResult *btree_cpp_bulkdelete(IndexVacuumInfo *info,
                                                   IndexBulkDeleteResult *stats,
                                                   IndexBulkDeleteCallback callback,
                                                   void *callback_state) {
    if (stats == NULL)
        stats = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
    return stats;
}

static IndexBulkDeleteResult *btree_cpp_vacuumcleanup(IndexVacuumInfo *info,
                                                      IndexBulkDeleteResult *stats) {
    if (stats == NULL)
        stats = (IndexBulkDeleteResult *) palloc0(sizeof(IndexBulkDeleteResult));
    return stats;
}

static void btree_cpp_costestimate(PlannerInfo *root,
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

static bool btree_cpp_validate(Oid opclassoid) {
    return true;
}

typedef struct BTreeCppScanOpaque {
    BTreeCppResult result;
    size_t pos;
    bool executed;
} BTreeCppScanOpaque;

static IndexScanDesc btree_cpp_beginscan(Relation index, int nkeys, int norderbys) {
    IndexScanDesc scan = RelationGetIndexScan(index, nkeys, norderbys);
    BTreeCppScanOpaque *so = (BTreeCppScanOpaque *) palloc0(sizeof(BTreeCppScanOpaque));
    scan->opaque = so;
    return scan;
}

static void btree_cpp_rescan(IndexScanDesc scan, ScanKey scankey, int nscankeys,
                             ScanKey orderbys, int norderbys) {
    BTreeCppScanOpaque *so = (BTreeCppScanOpaque *) scan->opaque;
    btree_cpp_free_result(&so->result);
    so->pos = 0;
    so->executed = false;

    if (scankey && scan->numberOfKeys > 0)
        memmove(scan->keyData, scankey, scan->numberOfKeys * sizeof(ScanKeyData));
}

static bool btree_cpp_gettuple(IndexScanDesc scan, ScanDirection dir) {
    BTreeCppScanOpaque *so = (BTreeCppScanOpaque *) scan->opaque;

    if (!so->executed) {
        BTreeCppHandle *handle = ensure_loaded(scan->heapRelation, scan->indexRelation);
        Oid key_typid = TupleDescAttr(RelationGetDescr(scan->indexRelation), 0)->atttypid;

        int has_min = 0, min_inc = 0;
        int has_max = 0, max_inc = 0;
        double min_val = 0.0, max_val = 0.0;
        bool impossible = false;

        for (int i = 0; i < scan->numberOfKeys; i++) {
            ScanKey sk = &scan->keyData[i];
            if (sk->sk_flags & SK_ISNULL) {
                impossible = true;
                break;
            }
            double val = datum_to_double(key_typid, sk->sk_argument);

            switch (sk->sk_strategy) {
                case BTLessStrategyNumber:
                    if (!has_max || val < max_val || (val == max_val && max_inc)) {
                        has_max = 1; max_val = val; max_inc = 0;
                    }
                    break;
                case BTLessEqualStrategyNumber:
                    if (!has_max || val < max_val) {
                        has_max = 1; max_val = val; max_inc = 1;
                    }
                    break;
                case BTEqualStrategyNumber:
                    if (!has_min || val > min_val || (val == min_val && !min_inc)) {
                        has_min = 1; min_val = val; min_inc = 1;
                    }
                    if (!has_max || val < max_val || (val == max_val && !max_inc)) {
                        has_max = 1; max_val = val; max_inc = 1;
                    }
                    break;
                case BTGreaterEqualStrategyNumber:
                    if (!has_min || val > min_val) {
                        has_min = 1; min_val = val; min_inc = 1;
                    }
                    break;
                case BTGreaterStrategyNumber:
                    if (!has_min || val > min_val || (val == min_val && min_inc)) {
                        has_min = 1; min_val = val; min_inc = 0;
                    }
                    break;
                default:
                    break;
            }
        }

        if (!impossible) {
            so->result = btree_cpp_scan_range(handle, has_min, min_val, min_inc,
                                              has_max, max_val, max_inc);
        }
        so->pos = 0;
        so->executed = true;
    }

    if (so->pos >= so->result.count)
        return false;

    BTreeCppTid tid = so->result.tids[so->pos++];
    ItemPointerSet(&scan->xs_heaptid, tid.block, tid.offset);
    scan->xs_recheck = false;
    return true;
}

static void btree_cpp_endscan(IndexScanDesc scan) {
    BTreeCppScanOpaque *so = (BTreeCppScanOpaque *) scan->opaque;
    if (so) {
        btree_cpp_free_result(&so->result);
        pfree(so);
        scan->opaque = NULL;
    }
}

PG_FUNCTION_INFO_V1(btree_cpp_handler);

Datum btree_cpp_handler(PG_FUNCTION_ARGS) {
    IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

    amroutine->amstrategies = 5;
    amroutine->amsupport = 1;
    amroutine->amoptsprocnum = 0;
    amroutine->amcanorder = true;
    amroutine->amcanorderbyop = false;
    amroutine->amcanbackward = false;
    amroutine->amcanunique = false;
    amroutine->amcanmulticol = false;
    amroutine->amoptionalkey = true;
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

    amroutine->ambuild = btree_cpp_build;
    amroutine->ambuildempty = btree_cpp_buildempty;
    amroutine->aminsert = btree_cpp_insert;
    amroutine->ambulkdelete = btree_cpp_bulkdelete;
    amroutine->amvacuumcleanup = btree_cpp_vacuumcleanup;
    amroutine->amcanreturn = NULL;
    amroutine->amcostestimate = btree_cpp_costestimate;
    amroutine->amoptions = NULL;
    amroutine->amproperty = NULL;
    amroutine->ambuildphasename = NULL;
    amroutine->amvalidate = btree_cpp_validate;
    amroutine->amadjustmembers = NULL;
    amroutine->ambeginscan = btree_cpp_beginscan;
    amroutine->amrescan = btree_cpp_rescan;
    amroutine->amgettuple = btree_cpp_gettuple;
    amroutine->amgetbitmap = NULL;
    amroutine->amendscan = btree_cpp_endscan;
    amroutine->ammarkpos = NULL;
    amroutine->amrestrpos = NULL;
    amroutine->amestimateparallelscan = NULL;
    amroutine->aminitparallelscan = NULL;
    amroutine->amparallelrescan = NULL;

    PG_RETURN_POINTER(amroutine);
}
