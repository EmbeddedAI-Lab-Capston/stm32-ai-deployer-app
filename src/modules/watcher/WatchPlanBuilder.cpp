#include "WatchPlanBuilder.h"
#include "ValueCodec.h"

#include <QtGlobal>

#include <algorithm>

namespace {
struct Range {
    int     itemIndex;
    quint64 start;          // extended range start (covers guard words too, if any)
    quint32 len;            // extended range length
    quint64 itemAddr;       // the item's own value address (for itemSlots offset)
    quint64 guardBeginAddr; // 0 if unguarded
    quint64 guardEndAddr;   // 0 if unguarded
};
}

WatchPlan WatchPlanBuilder::build(const QList<WatchItem> &items, quint32 maxReadBytes)
{
    WatchPlan plan;
    plan.itemSlots  = QVector<QPair<int, int>>(items.size(), qMakePair(-1, -1));
    plan.guardSlots = QVector<std::array<int, 3>>(items.size(), std::array<int, 3>{-1, -1, -1});
    if (maxReadBytes == 0) maxReadBytes = 4096;

    QVector<Range> ranges;
    ranges.reserve(items.size());
    for (int i = 0; i < items.size(); ++i) {
        const WatchItem &it = items.at(i);
        if (!it.enabled || it.kind != WatchItemKind::Scalar)
            continue;
        const int sz = ValueCodec::byteSize(it.type);
        if (sz <= 0)
            continue;

        const bool guarded = it.guardBeginAddr != 0 && it.guardEndAddr != 0;
        quint64 rangeStart = it.address;
        quint64 rangeEnd   = it.address + quint64(sz);
        if (guarded) {
            rangeStart = qMin(rangeStart, it.guardBeginAddr);
            rangeEnd   = qMax(rangeEnd, it.guardEndAddr + 4);
        }
        ranges.append({ i, rangeStart, quint32(rangeEnd - rangeStart), it.address,
                         guarded ? it.guardBeginAddr : 0, guarded ? it.guardEndAddr : 0 });
    }

    std::sort(ranges.begin(), ranges.end(),
              [](const Range &a, const Range &b) { return a.start < b.start; });

    int idx = 0;
    while (idx < ranges.size()) {
        quint64 blockStart = ranges.at(idx).start;
        quint64 blockEnd   = ranges.at(idx).start + ranges.at(idx).len;
        QVector<int> members{ idx };

        int j = idx + 1;
        while (j < ranges.size()) {
            const Range &next = ranges.at(j);
            const quint64 gap = (next.start >= blockEnd) ? (next.start - blockEnd) : 0;
            const quint64 candidateEnd = qMax(blockEnd, next.start + next.len);
            const quint64 candidateLen = candidateEnd - blockStart;
            if (gap > kMergeGapBytes || candidateLen > maxReadBytes)
                break;
            blockEnd = candidateEnd;
            members.append(j);
            ++j;
        }

        const int reqIndex = plan.requests.size();
        MemoryRequest req;
        req.id   = quint32(reqIndex);
        req.addr = blockStart;
        req.len  = quint32(blockEnd - blockStart);
        plan.requests.append(req);

        for (int m : members) {
            const Range &r = ranges.at(m);
            plan.itemSlots[r.itemIndex] = qMakePair(reqIndex, int(r.itemAddr - blockStart));
            if (r.guardBeginAddr != 0) {
                plan.guardSlots[r.itemIndex] = { reqIndex,
                                                  int(r.guardBeginAddr - blockStart),
                                                  int(r.guardEndAddr - blockStart) };
            }
        }

        idx = j;
    }

    return plan;
}

WatchPlan WatchPlanBuilder::buildRegionScans(const QList<WatchItem> &items, quint32 maxReadBytes)
{
    WatchPlan plan;
    plan.itemSlots  = QVector<QPair<int, int>>(items.size(), qMakePair(-1, -1));
    plan.guardSlots = QVector<std::array<int, 3>>(items.size(), std::array<int, 3>{-1, -1, -1});
    if (maxReadBytes == 0) maxReadBytes = 4096;

    for (int i = 0; i < items.size(); ++i) {
        const WatchItem &it = items.at(i);
        if (!it.enabled || it.kind != WatchItemKind::RegionScan || it.regionBytes == 0)
            continue;

        quint32 offset = 0;
        bool first = true;
        while (offset < it.regionBytes) {
            const quint32 chunk = qMin(maxReadBytes, it.regionBytes - offset);
            const int reqIndex = plan.requests.size();
            MemoryRequest req;
            req.id   = quint32(reqIndex);
            req.addr = it.address + offset;
            req.len  = chunk;
            plan.requests.append(req);

            if (first) {
                plan.itemSlots[i] = qMakePair(reqIndex, 0);
                first = false;
            }
            offset += chunk;
        }
    }

    return plan;
}

WatchPlan WatchPlanBuilder::merge(const WatchPlan &scalarPlan, const WatchPlan &regionPlan)
{
    WatchPlan out;
    out.requests = scalarPlan.requests;
    out.requests += regionPlan.requests;

    out.itemSlots  = scalarPlan.itemSlots;
    out.guardSlots = scalarPlan.guardSlots;   // RegionScan items never carry guards
    const int offset = scalarPlan.requests.size();
    for (int i = 0; i < out.itemSlots.size() && i < regionPlan.itemSlots.size(); ++i) {
        const QPair<int, int> &slot = regionPlan.itemSlots.at(i);
        if (slot.first >= 0)
            out.itemSlots[i] = qMakePair(slot.first + offset, slot.second);
    }
    return out;
}
