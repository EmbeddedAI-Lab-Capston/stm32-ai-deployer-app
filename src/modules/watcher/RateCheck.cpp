#include "RateCheck.h"

RateCheckResult computeRateCheck(const RateCheckInput &in)
{
    RateCheckResult out;

    if (!in.haveCountSamples) {
        out.detail = QStringLiteral("infer_count penceresinde yeterli örnek yok");
        return out;
    }
    const double deltaT = in.lastT - in.firstT;
    const double deltaCount = in.lastCount - in.firstCount;
    if (deltaT <= 0.0) {
        out.detail = QStringLiteral("pencere süresi sıfır veya negatif");
        return out;
    }
    if (deltaCount <= 0.0) {
        out.detail = QStringLiteral("pencerede inference sayacı artmadı");
        return out;
    }
    if (!in.infUsKnown || in.reportedInfUs <= 0.0) {
        out.detail = QStringLiteral("firmware'in bildirdiği inf_us değeri yok veya geçersiz");
        return out;
    }

    out.observedHz       = deltaCount / deltaT;
    out.reportedInfUs    = in.reportedInfUs;
    out.theoreticalMaxHz = 1.0e6 / in.reportedInfUs;

    // Small tolerance for measurement/timing jitter — this is a "does the
    // claim hold up" gate, not a bit-exact equality check.
    out.consistent = out.observedHz <= out.theoreticalMaxHz * 1.0005;
    out.ok = true;
    out.detail = out.consistent
        ? QStringLiteral("%1 Hz gözlendi · beyan %2 µs ile tutarlı")
              .arg(out.observedHz, 0, 'f', 1).arg(out.reportedInfUs, 0, 'f', 0)
        : QStringLiteral("%1 Hz gözlendi · beyan %2 µs ile TUTARSIZ (teorik üst sınır %3 Hz)")
              .arg(out.observedHz, 0, 'f', 1).arg(out.reportedInfUs, 0, 'f', 0)
              .arg(out.theoreticalMaxHz, 0, 'f', 1);
    return out;
}
