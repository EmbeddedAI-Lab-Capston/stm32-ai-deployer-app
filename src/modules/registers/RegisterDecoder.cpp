#include "RegisterDecoder.h"
#include "ReadPlanBuilder.h"   // isBlacklistedDataRegister (single source of truth)

QList<DecodedPeripheral> RegisterDecoder::decode(
    const SvdDevice &dev,
    const QStringList &selectedPeripherals,
    const QHash<quint64, quint32> &values,
    const QSet<QString> &clockEnabled,
    const QSet<QString> &gateable) const
{
    QList<DecodedPeripheral> out;

    for (const QString &name : selectedPeripherals) {
        const SvdPeripheral *p = dev.findPeripheral(name);
        if (!p)
            continue;

        DecodedPeripheral dp;
        dp.name        = p->name;
        dp.groupName   = p->groupName;
        dp.description = p->description;
        dp.baseAddress = p->baseAddress;

        // Clock status: only meaningful for gateable peripherals; always-on ones
        // (RCC, PWR, FLASH...) are Unknown and read regardless.
        if (gateable.contains(p->name))
            dp.clock = clockEnabled.contains(p->name) ? ClockStatus::On : ClockStatus::Off;
        else
            dp.clock = ClockStatus::Unknown;

        for (const SvdRegister &r : p->registers)
            dp.registers.append(decodeRegister(*p, r, values, dp.clock));

        out.append(dp);
    }
    return out;
}

DecodedRegister RegisterDecoder::decodeRegister(const SvdPeripheral &p,
                                                const SvdRegister &r,
                                                const QHash<quint64, quint32> &values,
                                                ClockStatus clock) const
{
    DecodedRegister dr;
    dr.name        = r.name;
    dr.displayName = r.displayName.isEmpty() ? r.name : r.displayName;
    dr.addr        = p.baseAddress + r.addressOffset;
    dr.resetValue  = r.resetValue;
    dr.resetMask   = r.resetMask;
    dr.description = r.description;

    // Status precedence: clock-off > deliberately skipped > unreadable > ok.
    if (clock == ClockStatus::Off) {
        dr.status = RegStatus::ClockOff;
        return dr;   // no value to decode
    }
    if (r.access == QStringLiteral("write-only")) {
        dr.status = RegStatus::SkippedWriteOnly;
        return dr;
    }
    if (r.hasReadSideEffect() || ReadPlanBuilder::isBlacklistedDataRegister(r.name)) {
        dr.status = RegStatus::SkippedSideEffect;
        return dr;
    }
    if (!values.contains(dr.addr)) {
        dr.status = RegStatus::Unreadable;
        return dr;
    }

    dr.status   = RegStatus::Ok;
    dr.rawValue = values.value(dr.addr);
    dr.changedFromReset =
        (dr.rawValue & dr.resetMask) != (dr.resetValue & dr.resetMask);

    for (const SvdField &f : r.fields) {
        DecodedField df;
        df.name        = f.name;
        df.bitOffset   = f.bitOffset;
        df.bitWidth    = f.bitWidth;
        df.description = f.description;
        const quint32 mask = (f.bitWidth >= 32) ? 0xFFFFFFFFu
                                                : ((1u << f.bitWidth) - 1u);
        df.value      = (dr.rawValue >> f.bitOffset) & mask;
        df.resetValue = (dr.resetValue >> f.bitOffset) & mask;
        df.changed    = df.value != df.resetValue;

        // Symbolic name for the current value (skip catch-all default entries).
        for (const SvdEnumValue &ev : f.enums) {
            if (!ev.isDefault && ev.value == df.value) {
                df.enumName = ev.name;
                break;
            }
        }
        dr.fields.append(df);
    }
    return dr;
}
