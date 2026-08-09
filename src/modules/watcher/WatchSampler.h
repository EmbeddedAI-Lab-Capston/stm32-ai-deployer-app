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
};
