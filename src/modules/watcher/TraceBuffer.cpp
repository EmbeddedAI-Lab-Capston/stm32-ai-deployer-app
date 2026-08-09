#include "TraceBuffer.h"

#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {
constexpr qint64 kMemCeilingBytes = 64LL * 1024 * 1024;
}

int TraceBuffer::configure(int itemCount, int capacityPerItem)
{
    if (itemCount < 0) itemCount = 0;
    if (capacityPerItem < 1) capacityPerItem = 1;

    const qint64 bytesNeeded = 8LL * (qint64(itemCount) + 1) * capacityPerItem;
    int actualCapacity = capacityPerItem;
    if (bytesNeeded > kMemCeilingBytes) {
        actualCapacity = int(kMemCeilingBytes / (8LL * (qint64(itemCount) + 1)));
        if (actualCapacity < 1) actualCapacity = 1;
    }

    m_capacity = actualCapacity;
    m_times = QVector<double>(m_capacity, 0.0);
    m_series.assign(itemCount, QVector<double>(m_capacity, 0.0));
    m_stats = QVector<WatchStats>(itemCount);
    m_totalCount = 0;
    return m_capacity;
}

void TraceBuffer::append(const WatchSampleBatch &batch)
{
    if (m_capacity <= 0)
        return;

    const int n = batch.times.size();
    for (int k = 0; k < n; ++k) {
        const int slot = slotFor(m_totalCount);
        m_times[slot] = batch.times.at(k);

        for (int it = 0; it < m_series.size(); ++it) {
            const double v = (it < batch.series.size() && k < batch.series.at(it).size())
                                  ? batch.series.at(it).at(k) : 0.0;
            m_series[it][slot] = v;
            m_stats[it].push(v);
        }
        ++m_totalCount;
    }
}

void TraceBuffer::clear()
{
    m_totalCount = 0;
    m_times.fill(0.0);
    for (auto &s : m_series) s.fill(0.0);
    for (auto &s : m_stats) s.reset();
}

quint64 TraceBuffer::sampleCount() const
{
    return qMin(m_totalCount, quint64(qMax(0, m_capacity)));
}

double TraceBuffer::firstTime() const
{
    if (m_totalCount == 0 || m_capacity <= 0) return 0.0;
    const quint64 oldest = (m_totalCount > quint64(m_capacity)) ? (m_totalCount - quint64(m_capacity)) : 0;
    return m_times.at(slotFor(oldest));
}

double TraceBuffer::lastTime() const
{
    if (m_totalCount == 0 || m_capacity <= 0) return 0.0;
    return m_times.at(slotFor(m_totalCount - 1));
}

QVector<PlotColumn> TraceBuffer::decimate(int item, double t0, double t1, int columns) const
{
    QVector<PlotColumn> out(qMax(0, columns));
    if (columns <= 0 || item < 0 || item >= m_series.size() || m_capacity <= 0
        || m_totalCount == 0 || t1 <= t0)
        return out;

    for (int c = 0; c < columns; ++c)
        out[c].t = t0 + (t1 - t0) * (double(c) + 0.5) / double(columns);

    const quint64 oldest = (m_totalCount > quint64(m_capacity)) ? (m_totalCount - quint64(m_capacity)) : 0;
    const quint64 available = m_totalCount - oldest;
    const double bucketWidth = (t1 - t0) / double(columns);

    for (quint64 k = 0; k < available; ++k) {
        const quint64 ordinal = oldest + k;
        const int slot = slotFor(ordinal);
        const double t = m_times.at(slot);
        if (t < t0 || t > t1)
            continue;

        int col = int((t - t0) / bucketWidth);
        if (col >= columns) col = columns - 1;
        if (col < 0) col = 0;

        const double v = m_series.at(item).at(slot);
        PlotColumn &pc = out[col];
        if (!pc.hasData) {
            pc.vmin = pc.vmax = pc.vlast = v;
            pc.hasData = true;
        } else {
            if (v < pc.vmin) pc.vmin = v;
            if (v > pc.vmax) pc.vmax = v;
            pc.vlast = v;   // iterating oldest -> newest, so last write wins
        }
    }

    return out;
}

QVector<RawSample> TraceBuffer::rawWindow(int item, double t0, double t1) const
{
    QVector<RawSample> out;
    if (item < 0 || item >= m_series.size() || m_capacity <= 0 || m_totalCount == 0 || t1 < t0)
        return out;

    const quint64 oldest = (m_totalCount > quint64(m_capacity)) ? (m_totalCount - quint64(m_capacity)) : 0;
    const quint64 available = m_totalCount - oldest;

    for (quint64 k = 0; k < available; ++k) {
        const int slot = slotFor(oldest + k);
        const double t = m_times.at(slot);
        if (t < t0 || t > t1)
            continue;
        out.append({t, m_series.at(item).at(slot)});
    }
    return out;
}

double TraceBuffer::valueAt(int item, double t) const
{
    if (item < 0 || item >= m_series.size() || m_capacity <= 0 || m_totalCount == 0)
        return std::numeric_limits<double>::quiet_NaN();

    const quint64 oldest = (m_totalCount > quint64(m_capacity)) ? (m_totalCount - quint64(m_capacity)) : 0;
    const quint64 available = m_totalCount - oldest;
    if (available == 0)
        return std::numeric_limits<double>::quiet_NaN();

    // First ordinal whose time is >= t (lower_bound over the ordinal range,
    // times are monotonic non-decreasing in insertion order).
    quint64 lo = oldest, hi = oldest + available - 1;
    while (lo < hi) {
        const quint64 mid = lo + (hi - lo) / 2;
        if (m_times.at(slotFor(mid)) < t) lo = mid + 1;
        else hi = mid;
    }

    quint64 best = lo;
    if (lo > oldest) {
        const double tPrev = m_times.at(slotFor(lo - 1));
        const double tCur  = m_times.at(slotFor(lo));
        if (std::abs(tPrev - t) <= std::abs(tCur - t))
            best = lo - 1;
    }
    return m_series.at(item).at(slotFor(best));
}

const WatchStats &TraceBuffer::stats(int item) const
{
    static const WatchStats empty;
    if (item < 0 || item >= m_stats.size())
        return empty;
    return m_stats.at(item);
}
