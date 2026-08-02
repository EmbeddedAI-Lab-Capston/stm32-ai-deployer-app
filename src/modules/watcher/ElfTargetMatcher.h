#pragma once
#include "SymbolModel.h"

#include <QList>
#include <QString>

// ── ElfTargetMatcher ──────────────────────────────────────────────────────
// The worst failure mode this whole feature has: an ELF that was never
// flashed to the connected board makes every symbol address wrong, and the
// tool would SILENTLY show completely fabricated values — the graph moves,
// numbers change, nothing ever errors (plan
// docs/variable_watcher_plan.md Bolum 6.2, 15). This class is the guard
// against that: it compares the target's live vector table (read via VTOR,
// a Cortex-M architectural constant, not board-specific) against the loaded
// ELF's `_estack`/`Reset_Handler` symbols.
//
// Policy is a visible warning, not a hard block (see Bolum 6.2's rationale —
// legitimate firmware that relocates the vector table to RAM, e.g. N6 LRUN,
// would otherwise be false-blocked). The caller (VariableWatcher/UI) decides
// what to do with Mismatch; this class only reports the facts.
enum class ElfMatchResult { Unknown, Match, Mismatch };

struct ElfMatchReport
{
    ElfMatchResult result = ElfMatchResult::Unknown;
    quint64 vtor            = 0;   // the VTOR value that was read
    quint32 targetInitialSp = 0;   // vector table [0], read from the live target
    quint32 targetResetVec  = 0;   // vector table [1], read from the live target
    quint64 elfEstack       = 0;   // ELF's _estack symbol address
    quint64 elfResetHandler = 0;   // ELF's Reset_Handler symbol address
    bool    spMatches    = false;
    bool    resetMatches = false;
    QString detail;                // human-readable explanation for the UI
};

class ElfTargetMatcher
{
public:
    // `firstEightBytes` is the 8 bytes read from address `vtor` (target's
    // active vector table base): [0..3] = initial SP, [4..7] = reset vector.
    // Pure/static — no I/O, fully unit-testable.
    static ElfMatchReport evaluate(quint64 vtor, const QByteArray &firstEightBytes,
                                    const QList<Symbol> &symbols);
};
