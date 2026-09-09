#pragma once
#include "WatchModel.h"
#include "WatchPlanBuilder.h"
#include "modules/debug/DebugLinkTypes.h"

#include <QList>
#include <QVector>

// ── WatchSampler ──────────────────────────────────────────────────────────
// Pure function: one sample's raw MemoryReply list -> one decoded double per
// WatchItem (plan docs/variable_watcher_plan.md Bolum 7.1). Conceptually
// "worker-side logic" per the plan, but implemented as a stateless function
// so it needs no thread affinity of its own — VariableWatcher (main thread)
// calls it directly on each DebugLink::rawSamplesReady.
class WatchSampler
{
public:
    // `replies` must be in the same order as `plan.requests` (DebugLink's
    // contract: "replies arrive in order"). Items with no slot in `plan`
    // (disabled/RegionScan/failed chunk) come back as value 0.0, ok=false.
    //
    // CALLERS MUST PASS okOut AND HONOUR IT. The 0.0 for a failed read is not
    // a reading — it is a placeholder, and it is indistinguishable from a
    // genuine zero. VariableWatcher originally passed nullptr here, so failed
    // reads entered the ring buffer and WatchStats as real samples; a rule
    // like "stack headroom < 512 B" would then fire on nothing at all. See
    // docs/variable_watcher_review.md K-4 and tests/TestWatchSampler.cpp.
    static QVector<double> decodeSample(const QList<WatchItem> &items, const WatchPlan &plan,
                                        const QVector<MemoryReply> &replies,
                                        QVector<bool> *okOut = nullptr);

private:
    // A RegionScan item's bytes may span several MemoryRequests
    // (WatchPlanBuilder::buildRegionScans() splits a region wider than
    // maxReadBytes into sequential chunks). Walks forward from `firstReqIndex`
    // through `replies` for as long as each reply's address is exactly
    // contiguous with the previous one's end (no gap, no reordering) and
    // fewer than `item.regionBytes` bytes have been consumed, scanning 32-bit
    // words for the first one that doesn't match `item.regionPattern`. Returns
    // the byte offset of that first mismatch (i.e. how much of the region from
    // its low-address start is still untouched — the stack "headroom" a
    // stackWatermark item reports), or `item.regionBytes` if every word
    // scanned still matches the pattern. *ok is false if a needed chunk is
    // missing/failed — the whole scan is unusable for this sample, same
    // "caller must honour ok, 0.0 is not a reading" contract as scalar items.
    static double decodeRegionScan(const WatchItem &item, int firstReqIndex,
                                   const QVector<MemoryReply> &replies, bool *ok);
};
