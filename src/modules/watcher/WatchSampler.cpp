#include "WatchSampler.h"
#include "ValueCodec.h"

QVector<double> WatchSampler::decodeSample(const QList<WatchItem> &items, const WatchPlan &plan,
                                           const QVector<MemoryReply> &replies, QVector<bool> *okOut)
{
    QVector<double> values(items.size(), 0.0);
    QVector<bool> ok(items.size(), false);

    for (int i = 0; i < items.size() && i < plan.itemSlots.size(); ++i) {
        const int reqIndex    = plan.itemSlots.at(i).first;
        const int byteOffset  = plan.itemSlots.at(i).second;
        if (reqIndex < 0 || reqIndex >= replies.size())
            continue;

        const MemoryReply &reply = replies.at(reqIndex);
        if (!reply.ok)
            continue;

        bool decodeOk = false;
        const double v = ValueCodec::decode(reply.data, byteOffset, items.at(i).type, &decodeOk);
        if (decodeOk) {
            values[i] = v;
            ok[i] = true;
        }
    }

    if (okOut) *okOut = ok;
    return values;
}
