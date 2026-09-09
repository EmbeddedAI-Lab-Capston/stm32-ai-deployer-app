#pragma once
#include <QString>
#include <QtGlobal>

// ── RamBudget ─────────────────────────────────────────────────────────────
// Pure (no QObject/Backend dependency): turns a handful of already-resolved
// addresses/values into a RAM usage breakdown (plan docs/memory_telemetry_plan.md
// Bolum 4). Deliberately kept separate from Backend so the arithmetic itself
// is unit-testable without a VariableWatcher/DebugLink/ELF in play.
//
// CRITICAL DISTINCTION this module exists to get right: a linker symbol's
// ADDRESS (_end, _estack) is meaningful; the memory CONTENT at that address
// is not — reading it back is garbage. Callers must pass symbol ADDRESSES
// for staticEndAddr/ramTopAddr/stackBaseAddr, and live watched VALUES for
// heapTopAddr/stackHeadroomBytes.
struct RamBudgetInput
{
    quint64 ramTotalBytes = 0;   // board's total RAM (BoardPresets), bytes

    bool    ramTopKnown = false;
    quint64 ramTopAddr  = 0;     // address of _estack (top of RAM / initial SP)

    bool    staticEndKnown = false;
    quint64 staticEndAddr  = 0;  // address of _end (end of .data+.bss)

    // __sbrk_heap_end's live VALUE (a RAM address) — false if that item
    // isn't being watched or has no value yet. NOT the same as "heap end
    // symbol found" — this needs an actual live read.
    bool    heapKnown   = false;
    quint64 heapTopAddr = 0;

    // stackWatermark RegionScan item: its own address (the resolved _sstack/
    // "_estack-_Min_Stack_Size" start) plus its live decoded value (bytes
    // from that start still holding the paint pattern = unused headroom).
    bool    stackKnown         = false;
    quint64 stackBaseAddr      = 0;
    quint64 stackHeadroomBytes = 0;
};

struct RamBudgetResult
{
    bool    ok = false;
    QString warning;   // set whenever something is off — even if ok, e.g. a collision

    quint64 ramTotal = 0;
    quint64 ramBase  = 0;   // ramTop - ramTotal
    quint64 ramTop   = 0;

    quint64 staticEnd = 0;
    quint64 heapTop   = 0;   // 0 if heap isn't in use / not watched
    quint64 stackDip  = 0;   // stackBaseAddr + stackHeadroomBytes: deepest point ever used

    // Signed: the gap between (max of static/heap end) and the stack's
    // deepest point. NEGATIVE means the stack has already grown past static
    // or heap memory — a real collision, not a rounding artefact.
    qint64  freeBytes = 0;

    quint64 staticUsed = 0;
    quint64 heapUsed   = 0;
    quint64 stackUsed  = 0;

    double  usedPct = 0.0;   // 100 * (staticUsed+heapUsed+stackUsed) / ramTotal
};

RamBudgetResult computeRamBudget(const RamBudgetInput &in);
