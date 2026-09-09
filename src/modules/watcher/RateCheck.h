#pragma once
#include <QString>

// ── RateCheck ─────────────────────────────────────────────────────────────
// Pure (no QObject/Backend dependency): independently sanity-checks the
// firmware's self-reported inf_us against the host's OWN clock (plan
// docs/memory_telemetry_plan.md Bolum 5). What this can and cannot prove,
// honestly:
//   - CAN verify: inference RATE (inferences/sec), from watching
//     infer_count increase over host-measured wall time.
//   - CANNOT directly verify: the microsecond duration of a single
//     inference (the firmware's own DWT-cycle measurement is opaque to an
//     external observer).
//   - CAN still catch a real lie: observed rate must never exceed the
//     THEORETICAL upper bound implied by the claimed duration
//     (1e6 / reportedInfUs inferences/sec). Exceeding it is a provable
//     contradiction. Staying under it is merely "consistent", never
//     "verified" — callers must say "tutarlı", never "doğrulandı".
struct RateCheckInput
{
    // infer_count read at the start/end of the observation window (raw
    // counter values, NOT deltas — the caller doesn't need to track state).
    bool   haveCountSamples = false;
    double firstT     = 0.0;   // seconds
    double lastT      = 0.0;
    double firstCount = 0.0;
    double lastCount  = 0.0;

    // Firmware's self-reported last inference duration, in MICROSECONDS
    // (raw inf_us — not scaled/ms-converted).
    bool   infUsKnown    = false;
    double reportedInfUs = 0.0;
};

struct RateCheckResult
{
    bool    ok = false;
    QString detail;

    double observedHz       = 0.0;
    double reportedInfUs    = 0.0;
    double theoreticalMaxHz = 0.0;
    bool   consistent       = false;
};

RateCheckResult computeRateCheck(const RateCheckInput &in);
