#pragma once
#include "WatchModel.h"

#include <QByteArray>
#include <QString>

// ── ValueCodec ────────────────────────────────────────────────────────────
// Pure byte <-> number codec + display formatting (plan
// docs/variable_watcher_plan.md Bolum 6.1). Little-endian only — Cortex-M is
// architecturally little-endian, this is not board-specific.
class ValueCodec
{
public:
    static int byteSize(WatchValueType t);

    // Decodes `byteSize(t)` bytes starting at `offset` in `buf`. *ok is false
    // (and the return value 0.0) if the range doesn't fit in `buf`.
    static double decode(const QByteArray &buf, int offset, WatchValueType t, bool *ok);

    // Applies scale/offset/format/unit. Hex/Bin show the raw bit pattern
    // (scale/offset don't apply — they're for human-readable decimal
    // conversion, meaningless for a bitmask view). Dec shows 3 decimals for
    // float types or anything with a non-identity scale/offset (a physical
    // quantity conversion), otherwise a plain integer.
    static QString format(double raw, const WatchItem &item);
};
