#pragma once
#include "SymbolModel.h"

#include <QList>
#include <QString>

// ── NmSymbolParser ────────────────────────────────────────────────────────
// Pure (no QObject, no process) parser: `arm-none-eabi-nm -S --defined-only`
// text output -> QList<Symbol>. Unit-tested against a real ELF's output
// (tests/fixtures/nm_h7.txt) — see docs/variable_watcher_plan.md Bolum 6.1.
class NmSymbolParser
{
public:
    // Accepts both line formats nm actually emits:
    //   "<addr hex> <size hex> <type> <name>"   (4 fields — most lines)
    //   "<addr hex> <type> <name>"              (3 fields — nm gave no size)
    // Unparseable lines (headers, warnings, anything without a valid hex
    // address) are silently skipped, never thrown or logged as errors.
    static QList<Symbol> parse(const QString &nmOutput);
};
