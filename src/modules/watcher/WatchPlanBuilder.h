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

    // RegionScan items only (stack watermark etc.) — a separate, low-rate
    // plan; these never enter the main sampling plan. A region larger than
    // maxReadBytes is split into sequential same-item chunks.
    static WatchPlan buildRegionScans(const QList<WatchItem> &items, quint32 maxReadBytes);
};
