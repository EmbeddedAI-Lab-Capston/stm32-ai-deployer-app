#pragma once
#include <QHash>
#include <QList>
#include <QString>
#include <QtGlobal>

// ── Read-side data model ───────────────────────────────────────────────────
// Runtime structures for taking a register snapshot (see
// docs/register_inspector_plan.md Bolum 4.2). ReadPlanBuilder produces a
// ReadPlan (what to read) plus a skip list (what was deliberately left out and
// why); RegisterReader executes the plan and fills a RegisterReadResult.

// Why a register/range was not read.
enum class SkipReason
{
    SideEffect,   // SVD readAction, write-only, or data-register name blacklist
    WriteOnly,    // access = write-only
    ClockOff      // peripheral clock disabled (RCC-gated out)
};

// One contiguous, side-effect-free range to read in a single -r32.
struct ReadPlanItem
{
    QString peripheralName;   // for error attribution / UI grouping
    quint64 startAddr = 0;
    quint32 byteCount = 0;    // multiple of 4
};

// A register the builder chose to skip (surfaced in the UI, never silently lost).
struct SkippedRegister
{
    QString    peripheralName;
    QString    registerName;
    quint64    addr = 0;
    SkipReason reason = SkipReason::SideEffect;
};

// The output of ReadPlanBuilder: safe ranges to read + what was skipped.
struct ReadPlan
{
    QString                 label;      // e.g. "RCC" or "peripherals"
    QList<ReadPlanItem>     items;
    QList<SkippedRegister>  skipped;

    quint32 totalBytes() const
    {
        quint32 t = 0;
        for (const ReadPlanItem &i : items) t += i.byteCount;
        return t;
    }
};

// A block-level read failure (a peripheral range the CLI could not read).
struct RegisterReadError
{
    QString peripheralName;
    quint64 startAddr = 0;
    QString message;
};

// The raw result of executing a ReadPlan: address -> 32-bit value, plus errors.
struct RegisterReadResult
{
    QHash<quint64, quint32> values;   // addr -> value, address-anchored
    QList<RegisterReadError> errors;  // ranges that failed (snapshot still valid)
    bool     processOk = false;       // CLI process launched & exited
    int      exitCode  = -1;

    bool hasValue(quint64 addr) const { return values.contains(addr); }
};
