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
    quint32        regionPattern = 0xA5A5A5A5u;   // for RegionScan — fill word to scan against
    double         scale = 1.0;
    double         offset = 0.0;
    QString        unit;
    bool           enabled = true;
    QString        source;        // "elf:<symbol>" | "manual"
    QString        color;         // line colour assigned from Theme
    int            laneIndex = -1; // Faz 6 plot lane; -1 = auto (own lane, in list order)
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

// String <-> enum conversions, shared by VariableWatcher (persistence) and
// Backend (QML marshalling) so both sides agree on one vocabulary.
inline QString watchValueTypeToString(WatchValueType t)
{
    switch (t) {
    case WatchValueType::U8:  return QStringLiteral("u8");
    case WatchValueType::I8:  return QStringLiteral("i8");
    case WatchValueType::U16: return QStringLiteral("u16");
    case WatchValueType::I16: return QStringLiteral("i16");
    case WatchValueType::U32: return QStringLiteral("u32");
    case WatchValueType::I32: return QStringLiteral("i32");
    case WatchValueType::U64: return QStringLiteral("u64");
    case WatchValueType::I64: return QStringLiteral("i64");
    case WatchValueType::F32: return QStringLiteral("f32");
    case WatchValueType::F64: return QStringLiteral("f64");
    }
    return QStringLiteral("u32");
}

inline WatchValueType watchValueTypeFromString(const QString &s)
{
    const QString v = s.trimmed().toLower();
    if (v == QStringLiteral("u8"))  return WatchValueType::U8;
    if (v == QStringLiteral("i8"))  return WatchValueType::I8;
    if (v == QStringLiteral("u16")) return WatchValueType::U16;
    if (v == QStringLiteral("i16")) return WatchValueType::I16;
    if (v == QStringLiteral("i32")) return WatchValueType::I32;
    if (v == QStringLiteral("u64")) return WatchValueType::U64;
    if (v == QStringLiteral("i64")) return WatchValueType::I64;
    if (v == QStringLiteral("f32")) return WatchValueType::F32;
    if (v == QStringLiteral("f64")) return WatchValueType::F64;
    return WatchValueType::U32;   // includes "u32" and any unrecognised value
}

inline QString displayFormatToString(DisplayFormat f)
{
    switch (f) {
    case DisplayFormat::Hex: return QStringLiteral("hex");
    case DisplayFormat::Bin: return QStringLiteral("bin");
    case DisplayFormat::Dec: return QStringLiteral("dec");
    }
    return QStringLiteral("dec");
}

inline DisplayFormat displayFormatFromString(const QString &s)
{
    const QString v = s.trimmed().toLower();
    if (v == QStringLiteral("hex")) return DisplayFormat::Hex;
    if (v == QStringLiteral("bin")) return DisplayFormat::Bin;
    return DisplayFormat::Dec;
}
