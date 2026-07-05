#pragma once
#include "RegisterSnapshot.h"
#include "SvdModel.h"

#include <QHash>
#include <QSet>
#include <QStringList>

// ── RegisterDecoder ────────────────────────────────────────────────────────
// Pure: merges SVD definitions with raw read values into a decoded tree
// (plan Bolum 3.6). Every selected register appears with a status — read &
// decoded, skipped (side-effect/write-only), clock-off, or unreadable — so
// nothing is silently lost. Field decode includes enum names and reset-diff.
class RegisterDecoder
{
public:
    // Decode the selected peripherals. `values` is the combined RCC + block read
    // map. `clockEnabled` / `gateable` come from ReadPlanBuilder's RCC decode.
    QList<DecodedPeripheral> decode(const SvdDevice &dev,
                                    const QStringList &selectedPeripherals,
                                    const QHash<quint64, quint32> &values,
                                    const QSet<QString> &clockEnabled,
                                    const QSet<QString> &gateable) const;

private:
    DecodedRegister decodeRegister(const SvdPeripheral &p, const SvdRegister &r,
                                   const QHash<quint64, quint32> &values,
                                   ClockStatus clock) const;
};
