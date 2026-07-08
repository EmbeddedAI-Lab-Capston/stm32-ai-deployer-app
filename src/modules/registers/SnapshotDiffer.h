#pragma once
#include "RegisterDiff.h"

// ── SnapshotDiffer ─────────────────────────────────────────────────────────
// Pure logic (no QObject): computes a field-level SnapshotDiff between two
// RegisterSnapshot trees. Registers are matched by address; only registers
// with a real difference (raw value or read status) are reported, and within
// those, only the fields that actually changed. See plan Bolum 1a.
class SnapshotDiffer
{
public:
    SnapshotDiff diff(const RegisterSnapshot &a, const RegisterSnapshot &b) const;
};
