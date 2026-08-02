#pragma once

#include <QString>
#include <QtGlobal>

// ── SymbolModel ───────────────────────────────────────────────────────────
// One entry per `arm-none-eabi-nm -S --defined-only <elf>` line (plan
// docs/variable_watcher_plan.md Bolum 6.1).
enum class SymbolKind {
    Bss,        // B/b — RAM, zero-init       -> watchable
    Data,       // D/d — RAM, initialised      -> watchable
    ReadOnly,   // R/r — flash constant        -> watchable (never changes)
    Code,       // T/t — code                  -> not watchable (hidden by default)
    Weak,       // W/w/V/v                     -> depends on what it resolves to
    Absolute,   // A/a — a VALUE, not an address -> can NEVER be added to a watch list
    Common,     // C
    Other
};

struct Symbol
{
    QString    name;
    quint64    address = 0;    // for Absolute: this is a VALUE, not an address
    quint64    size = 0;       // 0 = nm gave no size
    char       nmType = '?';
    SymbolKind kind = SymbolKind::Other;
    bool       hasSize = false;
    bool       addressIsValue = false;   // kind == Absolute
};
