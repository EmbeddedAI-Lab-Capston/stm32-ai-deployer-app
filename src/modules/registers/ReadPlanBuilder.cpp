#include "ReadPlanBuilder.h"

#include <QRegularExpression>
#include <algorithm>

bool ReadPlanBuilder::isBlacklistedDataRegister(const QString &registerName)
{
    static const QSet<QString> kDataRegs = {
        QStringLiteral("DR"), QStringLiteral("RDR"), QStringLiteral("TDR"),
        QStringLiteral("TXDR"), QStringLiteral("RXDR"), QStringLiteral("DATAR")
    };
    const QString n = registerName.toUpper();
    return kDataRegs.contains(n) || n.contains(QStringLiteral("FIFO"));
}

ReadPlan ReadPlanBuilder::build(const SvdDevice &dev,
                                const QStringList &peripheralNames,
                                const QSet<QString> *clockEnabled) const
{
    ReadPlan plan;
    plan.label = QStringLiteral("peripherals");

    // Only peripherals that actually have an RCC enable bit can be gated off;
    // always-on peripherals (RCC, PWR, FLASH...) are read regardless.
    QSet<QString> gateable;
    if (clockEnabled)
        gateable = rccGateablePeripherals(dev);

    for (const QString &name : peripheralNames) {
        const SvdPeripheral *p = dev.findPeripheral(name);
        if (!p)
            continue;   // selection may name a peripheral absent from this device

        if (clockEnabled && gateable.contains(p->name) && !clockEnabled->contains(p->name)) {
            // Clock off: don't read (avoids garbage/fault), surface as skipped.
            plan.skipped.append({ p->name, QString(), p->baseAddress, SkipReason::ClockOff });
            continue;
        }
        buildPeripheral(*p, plan);
    }
    return plan;
}

ReadPlan ReadPlanBuilder::buildRccPlan(const SvdDevice &dev) const
{
    ReadPlan plan;
    plan.label = QStringLiteral("RCC");
    if (const SvdPeripheral *rcc = dev.findPeripheral(QStringLiteral("RCC")))
        buildPeripheral(*rcc, plan);
    return plan;
}

void ReadPlanBuilder::buildPeripheral(const SvdPeripheral &p, ReadPlan &plan) const
{
    // Sort registers by address so we can merge readable neighbours and split at
    // side-effect registers.
    QList<const SvdRegister *> regs;
    regs.reserve(p.registers.size());
    for (const SvdRegister &r : p.registers)
        regs.append(&r);
    std::sort(regs.begin(), regs.end(), [](const SvdRegister *a, const SvdRegister *b) {
        return a->addressOffset < b->addressOffset;
    });

    bool    rangeOpen = false;
    quint64 rangeStart = 0;
    quint64 rangeEnd   = 0;   // exclusive (absolute address)

    auto closeRange = [&]() {
        if (!rangeOpen)
            return;
        ReadPlanItem item;
        item.peripheralName = p.name;
        item.startAddr = rangeStart;
        // Round up to a multiple of 4 (we read 32-bit words).
        item.byteCount = static_cast<quint32>(((rangeEnd - rangeStart) + 3) & ~quint64(3));
        plan.items.append(item);
        rangeOpen = false;
    };

    for (const SvdRegister *r : regs) {
        const quint64 addr  = p.baseAddress + r->addressOffset;
        const quint64 bytes = std::max(1, r->sizeBits / 8);

        const bool skip = r->hasReadSideEffect() || isBlacklistedDataRegister(r->name);
        if (skip) {
            closeRange();   // break the range around this register
            SkipReason reason = (r->access == QStringLiteral("write-only"))
                                    ? SkipReason::WriteOnly
                                    : SkipReason::SideEffect;
            plan.skipped.append({ p.name, r->name, addr, reason });
            continue;
        }

        if (!rangeOpen) {
            rangeOpen  = true;
            rangeStart = addr;
            rangeEnd   = addr + bytes;
        } else {
            rangeEnd = std::max(rangeEnd, addr + bytes);
        }
    }
    closeRange();
}

QSet<QString> ReadPlanBuilder::clockEnabledPeripherals(
    const SvdDevice &dev,
    const QHash<quint64, quint32> &values,
    const QHash<QString, QString> &rccOverrides) const
{
    QSet<QString> enabled;
    forEachRccEnableBit(dev, values, rccOverrides,
                        [&](const QString &periph, bool isSet, bool /*hasValue*/) {
                            if (isSet)
                                enabled.insert(periph);
                        });
    return enabled;
}

QSet<QString> ReadPlanBuilder::rccGateablePeripherals(
    const SvdDevice &dev,
    const QHash<QString, QString> &rccOverrides) const
{
    QSet<QString> gateable;
    forEachRccEnableBit(dev, QHash<quint64, quint32>(), rccOverrides,
                        [&](const QString &periph, bool /*isSet*/, bool /*hasValue*/) {
                            gateable.insert(periph);
                        });
    return gateable;
}
