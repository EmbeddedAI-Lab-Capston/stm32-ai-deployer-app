#pragma once
#include "RegisterReadModel.h"
#include "SvdModel.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

// ── ReadPlanBuilder ────────────────────────────────────────────────────────
// Pure logic (no QObject, no CLI): turns a peripheral selection + SvdDevice into
// safe, side-effect-free read ranges, and decodes RCC clock-gating. Unit-tested
// against the shipped SVDs. See docs/register_inspector_plan.md Bolum 3.2/3.5.
class ReadPlanBuilder
{
public:
    // A register whose *read* has a side effect not caught by SVD readAction.
    // NOTE: matched by exact name (+ "FIFO" substring), deliberately NOT the
    // plan's raw "contains DR" — that would wrongly skip GPIO ODR/IDR, which are
    // safe and useful. Data registers that pop FIFOs (USART DR/RDR, SPI DR,
    // I2C RX/TXDR) are exact-named and caught here.
    static bool isBlacklistedDataRegister(const QString &registerName);

    // Build a read plan for the given peripheral names. If `clockEnabled` is
    // non-null, peripherals absent from it are skipped as ClockOff (RCC-gating).
    ReadPlan build(const SvdDevice &dev,
                   const QStringList &peripheralNames,
                   const QSet<QString> *clockEnabled = nullptr) const;

    // A plan that reads only RCC (the first, gating read of a snapshot).
    ReadPlan buildRccPlan(const SvdDevice &dev) const;

    // Decode which peripherals have their clock enabled from RCC register
    // values. Maps RCC enable-register fields ("GPIOAEN" -> "GPIOA") by stripping
    // the "EN" suffix and matching a real peripheral. Low-power enable registers
    // (xxLPENR) are ignored. `rccOverrides` (from boards.json) can correct
    // field-name -> peripheral-name mismatches.
    QSet<QString> clockEnabledPeripherals(
        const SvdDevice &dev,
        const QHash<quint64, quint32> &values,
        const QHash<QString, QString> &rccOverrides = QHash<QString, QString>()) const;

    // Peripherals that HAVE an RCC enable bit (regardless of its value). Only
    // these can be gated off; always-on peripherals (RCC, PWR, FLASH...) have no
    // enable bit and are never skipped as ClockOff.
    QSet<QString> rccGateablePeripherals(
        const SvdDevice &dev,
        const QHash<QString, QString> &rccOverrides = QHash<QString, QString>()) const;

private:
    // Append safe ranges for one (clock-on) peripheral, collecting skips.
    void buildPeripheral(const SvdPeripheral &p, ReadPlan &plan) const;

    // Shared walk over RCC enable registers: calls fn(peripheralName, isSet,
    // hasValue) for every "xxEN" field that maps to a real peripheral.
    template <typename Fn>
    void forEachRccEnableBit(const SvdDevice &dev,
                             const QHash<quint64, quint32> &values,
                             const QHash<QString, QString> &rccOverrides,
                             Fn &&fn) const;
};

template <typename Fn>
void ReadPlanBuilder::forEachRccEnableBit(
    const SvdDevice &dev,
    const QHash<quint64, quint32> &values,
    const QHash<QString, QString> &rccOverrides,
    Fn &&fn) const
{
    const SvdPeripheral *rcc = dev.findPeripheral(QStringLiteral("RCC"));
    if (!rcc)
        return;

    for (const SvdRegister &reg : rcc->registers) {
        const QString rn = reg.name.toUpper();
        // Clock-enable registers end in "ENR"; skip low-power variants (xxLPENR).
        if (!rn.endsWith(QStringLiteral("ENR")) || rn.contains(QStringLiteral("LP")))
            continue;

        const quint64 addr    = rcc->baseAddress + reg.addressOffset;
        const bool    hasValue = values.contains(addr);
        const quint32 regVal   = values.value(addr, 0);

        for (const SvdField &f : reg.fields) {
            if (!f.name.endsWith(QStringLiteral("EN")) || f.name.size() <= 2)
                continue;
            const quint32 mask = (f.bitWidth >= 32) ? 0xFFFFFFFFu
                                                    : ((1u << f.bitWidth) - 1u);
            const bool isSet = hasValue && (((regVal >> f.bitOffset) & mask) != 0);

            QString periph = f.name;
            periph.chop(2);   // "GPIOAEN" -> "GPIOA"
            periph = rccOverrides.value(f.name, periph);

            if (const SvdPeripheral *pp = dev.findPeripheral(periph))
                fn(pp->name, isSet, hasValue);
        }
    }
}
