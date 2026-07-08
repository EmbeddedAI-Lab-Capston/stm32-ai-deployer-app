#include "SnapshotDiffer.h"

#include <QHash>

namespace {

struct Located
{
    const DecodedPeripheral *peripheral = nullptr;
    const DecodedRegister   *reg = nullptr;
};

QHash<quint64, Located> indexByAddress(const RegisterSnapshot &snap)
{
    QHash<quint64, Located> map;
    for (const DecodedPeripheral &p : snap.peripherals)
        for (const DecodedRegister &r : p.registers)
            map.insert(r.addr, { &p, &r });
    return map;
}

QList<FieldDiff> diffFields(const DecodedRegister &a, const DecodedRegister &b)
{
    QList<FieldDiff> out;
    if (a.status != RegStatus::Ok || b.status != RegStatus::Ok)
        return out;   // no field-level detail when either side wasn't decoded

    QHash<QString, const DecodedField *> bFields;
    for (const DecodedField &f : b.fields)
        bFields.insert(f.name, &f);

    for (const DecodedField &fa : a.fields) {
        const DecodedField *fb = bFields.value(fa.name, nullptr);
        if (!fb || fa.value == fb->value)
            continue;
        FieldDiff fd;
        fd.name        = fa.name;
        fd.bitOffset   = fa.bitOffset;
        fd.bitWidth    = fa.bitWidth;
        fd.description = fa.description;
        fd.valueA      = fa.value;
        fd.valueB      = fb->value;
        fd.enumNameA   = fa.enumName;
        fd.enumNameB   = fb->enumName;
        out.append(fd);
    }
    return out;
}

} // namespace

SnapshotDiff SnapshotDiffer::diff(const RegisterSnapshot &a, const RegisterSnapshot &b) const
{
    SnapshotDiff out;
    out.takenAtA = a.takenAt;
    out.takenAtB = b.takenAt;

    if (!a.valid || !b.valid) {
        out.comparable = false;
        out.incomparableReason = QStringLiteral("Bir veya iki slot da boş");
        return out;
    }
    if (a.svdFile != b.svdFile || a.boardName != b.boardName) {
        out.comparable = false;
        out.incomparableReason = QStringLiteral("Snapshot'lar farklı kart/SVD'den alınmış");
        return out;
    }

    const QHash<quint64, Located> mapA = indexByAddress(a);
    const QHash<quint64, Located> mapB = indexByAddress(b);

    // Only registers present in BOTH (i.e. selected in both snapshots) are
    // comparable; a register only in one side reflects a different peripheral
    // selection, not a real state change, so it is not reported here.
    for (auto it = mapA.constBegin(); it != mapA.constEnd(); ++it) {
        const auto itB = mapB.constFind(it.key());
        if (itB == mapB.constEnd())
            continue;

        const DecodedRegister &ra = *it.value().reg;
        const DecodedRegister &rb = *itB.value().reg;
        if (ra.rawValue == rb.rawValue && ra.status == rb.status)
            continue;

        RegisterDiff rd;
        rd.peripheralName = it.value().peripheral->name;
        rd.registerName   = ra.name;
        rd.addr           = ra.addr;
        rd.statusA        = ra.status;
        rd.statusB        = rb.status;
        rd.rawA           = ra.rawValue;
        rd.rawB           = rb.rawValue;
        rd.changedFields  = diffFields(ra, rb);
        out.changedRegisters.append(rd);
    }

    return out;
}
