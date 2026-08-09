#pragma once
#include "WatchModel.h"
#include "modules/debug/DebugLinkTypes.h"

#include <QVector>

// ── TraceBuffer ───────────────────────────────────────────────────────────
// Pure (no QObject): per-item ring buffer + min/max-envelope decimation for
// the plot, plus WatchStats accumulation (plan docs/variable_watcher_plan.md
// Bolum 7.3). WatchStats is independent of the ring — eviction never touches
// it, so session min/max/mean/stddev stay correct even after old samples
// fall out of the window.
struct PlotColumn
{
    double t = 0.0;
    double vmin = 0.0;
    double vmax = 0.0;
    double vlast = 0.0;
    bool   hasData = false;
};

// Undecimated (t, value) pair — for the rule engine (Faz 8), which needs
// actual samples (for z-score/linear-regression) rather than decimate()'s
// per-pixel-column min/max envelope.
struct RawSample
{
    double t = 0.0;
    double v = 0.0;
};

class TraceBuffer
{
public:
    // Default ~120000 samples/item (40s @ 3kHz). Total memory is capped at
    // 64MB (8 bytes/sample * (itemCount+1) for the shared time column);
    // if the requested capacity would exceed that, it is silently reduced —
    // returns the ACTUAL capacity used so the caller can tell the user
    // ("ring arabelleği N s'ye düşürüldü") rather than truncate silently.
    int configure(int itemCount, int capacityPerItem = 120000);

    void append(const WatchSampleBatch &batch);
    void clear();

    int    itemCount() const { return m_series.size(); }
    int    capacityPerItem() const { return m_capacity; }
    quint64 sampleCount() const;        // samples currently in the ring (<= capacity)
    quint64 totalSamplesEver() const { return m_totalCount; }
    double firstTime() const;
    double lastTime() const;

    QVector<PlotColumn> decimate(int item, double t0, double t1, int columns) const;
    const WatchStats &stats(int item) const;

    // Raw (undecimated) samples in [t0, t1] — Faz 8 rule engine. Cost is
    // O(samples in range), fine for the few-second windows rules use; NOT
    // meant for the full-buffer/plot-frame case (use decimate() there).
    QVector<RawSample> rawWindow(int item, double t0, double t1) const;

    // Cursor read (plan Bolum 9.4 watchValuesAt): value of the sample whose
    // timestamp is closest to t. NaN if the item/buffer is empty or out of
    // range. Binary search over insertion order — times are monotonic.
    double valueAt(int item, double t) const;

private:
    int slotFor(quint64 ordinal) const { return int(ordinal % quint64(m_capacity)); }

    int m_capacity = 0;
    QVector<double>           m_times;    // circular, shared across items
    QVector<QVector<double>>  m_series;   // series[item][slot], circular
    QVector<WatchStats>       m_stats;    // independent of the ring
    quint64 m_totalCount = 0;             // total samples ever appended
};
