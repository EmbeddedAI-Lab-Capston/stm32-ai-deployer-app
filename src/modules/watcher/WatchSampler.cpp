#include "WatchSampler.h"
#include "ValueCodec.h"

QVector<double> WatchSampler::decodeSample(const QList<WatchItem> &items, const WatchPlan &plan,
                                           const QVector<MemoryReply> &replies, QVector<bool> *okOut)
{
    QVector<double> values(items.size(), 0.0);
    QVector<bool> ok(items.size(), false);

    for (int i = 0; i < items.size() && i < plan.itemSlots.size(); ++i) {
        const WatchItem &item = items.at(i);
        const int reqIndex    = plan.itemSlots.at(i).first;
        if (reqIndex < 0 || reqIndex >= replies.size())
            continue;

        if (item.kind == WatchItemKind::RegionScan) {
            bool scanOk = false;
            const double v = decodeRegionScan(item, reqIndex, replies, &scanOk);
            if (scanOk) {
                values[i] = v;
                ok[i] = true;
            }
            continue;
        }

        const int byteOffset  = plan.itemSlots.at(i).second;
        const MemoryReply &reply = replies.at(reqIndex);
        if (!reply.ok)
            continue;

        // Seqlock gate: if the two guard words differ, the host caught the
        // firmware mid-write. Do NOT fabricate a value for this sample —
        // leave ok[i] false, same contract as any other failed read.
        if (i < plan.guardSlots.size() && plan.guardSlots.at(i)[0] >= 0) {
            const auto &g = plan.guardSlots.at(i);
            if (g[0] >= replies.size())
                continue;
            const MemoryReply &guardReply = replies.at(g[0]);
            if (!guardReply.ok)
                continue;
            bool beginOk = false, endOk = false;
            const double beginVal = ValueCodec::decode(guardReply.data, g[1], WatchValueType::U32, &beginOk);
            const double endVal   = ValueCodec::decode(guardReply.data, g[2], WatchValueType::U32, &endOk);
            if (!beginOk || !endOk || beginVal != endVal)
                continue;
        }

        bool decodeOk = false;
        const double v = ValueCodec::decode(reply.data, byteOffset, item.type, &decodeOk);
        if (decodeOk) {
            values[i] = v;
            ok[i] = true;
        }
    }

    if (okOut) *okOut = ok;
    return values;
}

double WatchSampler::decodeRegionScan(const WatchItem &item, int firstReqIndex,
                                      const QVector<MemoryReply> &replies, bool *ok)
{
    *ok = false;
    if (item.regionBytes == 0)
        return 0.0;

    quint64 expectedAddr = item.address;
    quint32 bytesSeen    = 0;
    quint32 mismatchAt   = item.regionBytes;   // default: fully matched end-to-end
    bool    foundMismatch = false;

    for (int k = firstReqIndex; k < replies.size() && bytesSeen < item.regionBytes; ++k) {
        const MemoryReply &reply = replies.at(k);
        if (reply.addr != expectedAddr)
            break;      // gap/reorder — not (or no longer) part of this item's chunk run
        if (!reply.ok)
            return 0.0;  // a required chunk failed: whole scan unusable this sample

        const int wordCount = reply.data.size() / 4;
        for (int w = 0; w < wordCount && bytesSeen < item.regionBytes; ++w, bytesSeen += 4) {
            bool wordOk = false;
            const quint32 word = quint32(ValueCodec::decode(reply.data, w * 4, WatchValueType::U32, &wordOk));
            if (!wordOk)
                return 0.0;
            if (word != item.regionPattern) {
                mismatchAt    = bytesSeen;
                foundMismatch = true;
                break;
            }
        }
        if (foundMismatch)
            break;
        expectedAddr = reply.addr + quint64(reply.data.size());
    }

    *ok = true;
    return double(mismatchAt);
}
