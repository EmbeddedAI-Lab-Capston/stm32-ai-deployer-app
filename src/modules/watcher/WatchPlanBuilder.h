#pragma once
#include "WatchModel.h"
#include "modules/debug/DebugLinkTypes.h"

#include <QList>
#include <QPair>
#include <QVector>

// ── WatchPlanBuilder ──────────────────────────────────────────────────────
// Pure logic (no QObject, no link): turns a WatchItem selection into a
// minimal set of MemoryRequests (plan docs/variable_watcher_plan.md Bolum 7.2).
// Faz A's cost model is `t ~= 0.31ms + size/550KB/s` — fixed per-round-trip
// cost dominates, so merging nearby reads into one block matters far more
// than the extra bytes it costs.
struct WatchPlan
{
    QVector<MemoryRequest> requests;

    // Parallel to the `items` list passed to build()/buildRegionScans() —
    // itemSlots[i] describes where items[i]'s bytes land: {requestIndex,
    // byteOffset}. {-1,-1} means "not sampled by this plan" (disabled, or
    // the wrong WatchItemKind for this builder call).
    QVector<QPair<int, int>> itemSlots;

    int roundTripsPerSample() const { return requests.size(); }
};

class WatchPlanBuilder
{
public:
    // Merge rule: a gap <= kMergeGapBytes between two ranges' end/start is
    // bridged into one read; the resulting block never exceeds maxReadBytes
    // (DebugLink::maxReadBytes(), passed in — never hardcoded here).
    static constexpr quint32 kMergeGapBytes = 256;

    // Scalar items only (RegionScan items are skipped — see buildRegionScans).
    static WatchPlan build(const QList<WatchItem> &items, quint32 maxReadBytes);

    // RegionScan items only (stack watermark etc.). A region larger than
    // maxReadBytes is split into sequential same-item chunks.
    //
    // NOTE: the plan's original intent (see WatchPlan's own history) was for
    // this to run on a separate, low-rate poll alongside the main scalar
    // plan — no such secondary poll exists (the DHCSR health-check timer in
    // DebugLinkWorker is the only precedent, and it is a hardcoded
    // architecture-level single register, not a generic per-item mechanism).
    // Building that is significant additional scheduling work, so
    // `VariableWatcher::rebuildPlan()` instead merges this straight into the
    // main plan via `merge()` — RegionScan items are read at the FULL
    // target sample rate, not throttled. Correct, but not free: a region
    // scan adds its own MemoryRequest(s) to every single sample's round
    // trip (WatchPlan's own cost model: ~0.31ms + size/550KB/s per request),
    // so a large region can meaningfully cap the achievable sample rate.
    // Revisit with real low-rate scheduling if that cost matters in practice.
    static WatchPlan buildRegionScans(const QList<WatchItem> &items, quint32 maxReadBytes);

    // Combines a scalar plan and a region-scan plan (built separately, over
    // the SAME items list) into one: requests are concatenated and the
    // region plan's itemSlots are remapped by the scalar plan's request
    // count. Safe because a given item index is filled by at most one of
    // the two source plans (an item is either Scalar or RegionScan, never
    // both), so there is nothing to arbitrate between them.
    static WatchPlan merge(const WatchPlan &scalarPlan, const WatchPlan &regionPlan);
};
