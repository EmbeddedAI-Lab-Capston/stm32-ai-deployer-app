#pragma once

#include <QString>
#include <QtGlobal>

#include <cmath>

// ── WatchModel ────────────────────────────────────────────────────────────
// Runtime data model for a watched item (plan docs/variable_watcher_plan.md
// Bolum 6.1). No process/socket dependency — pure data + WatchStats' online
// (Welford) accumulation logic.

enum class WatchValueType { U8, I8, U16, I16, U32, I32, U64, I64, F32, F64 };
enum class DisplayFormat  { Dec, Hex, Bin };
enum class WatchItemKind  { Scalar, RegionScan };   // RegionScan = stack watermark

struct WatchItem
{
    QString        id;            // persistent, QUuid::createUuid().toString(Id128)
    QString        label;
    QString        role;          // preset role ("heapEnd","stackWatermark",...) or empty
    quint64        address = 0;
    WatchItemKind  kind = WatchItemKind::Scalar;
    WatchValueType type = WatchValueType::U32;
    DisplayFormat  format = DisplayFormat::Dec;
    quint32        regionBytes = 0;   // for RegionScan
    double         scale = 1.0;
    double         offset = 0.0;
    QString        unit;
    bool           enabled = true;
    QString        source;        // "elf:<symbol>" | "manual"
    QString        color;         // line colour assigned from Theme
};

// Independent of the ring buffer, accumulated online (Welford) — session
// min/max/mean/stddev stay correct even after old samples are evicted.
struct WatchStats
{
    quint64 count = 0;
    double  min = 0, max = 0, last = 0, mean = 0, m2 = 0;

    double variance() const { return count > 0 ? m2 / double(count) : 0.0; }
    double stddev() const { return std::sqrt(variance()); }

    void push(double v)
    {
        last = v;
        if (count == 0) {
            min = max = v;
        } else {
            if (v < min) min = v;
            if (v > max) max = v;
        }
        ++count;
        const double delta = v - mean;
        mean += delta / double(count);
        const double delta2 = v - mean;
        m2 += delta * delta2;
    }

    void reset()
    {
        count = 0;
        min = max = last = mean = m2 = 0;
    }
};
