#pragma once
#include <QString>
#include <QList>
#include <QtGlobal>

// ── SVD data model ─────────────────────────────────────────────────────────
// Plain, QObject-free structs mirroring the CMSIS-SVD hierarchy (see
// docs/register_inspector_plan.md Bolum 4.1). SvdParser fills these; the model
// itself carries no parsing/decoding logic. derivedFrom and (future) dim are
// resolved by the parser at load time, so consumers see a self-contained tree.

// A single symbolic value of a field (SVD <enumeratedValue>).
struct SvdEnumValue
{
    QString name;         // e.g. "MEMORY_TO_PERIPH" (F4 SVDs have no enums)
    QString description;
    quint64 value = 0;
    bool    isDefault = false;   // SVD <isDefault/> catch-all entry
};

// A bit-field within a register (SVD <field>).
struct SvdField
{
    QString name;         // "EN"
    QString description;
    int     bitOffset = 0;
    int     bitWidth  = 1;
    QString access;       // empty = inherit from register
    QString readAction;   // clear/set/modify/modifyExternal (field-level; N6 uses this)
    QList<SvdEnumValue> enums;
};

// A single register (SVD <register>). readAction marks side-effect-on-read.
struct SvdRegister
{
    QString name;             // "S0CR"
    QString displayName;
    QString description;
    quint64 addressOffset = 0;    // relative to peripheral baseAddress
    int     sizeBits      = 32;
    QString access;               // read-write / read-only / write-only
    QString readAction;           // clear/set/modify/modifyExternal (empty = none)
    quint32 resetValue    = 0;
    quint32 resetMask     = 0xFFFFFFFF;
    QList<SvdField> fields;

    // Side-effect on read: register-level readAction, write-only access, OR any
    // field marks a readAction (N6 SVDs put readAction at field level).
    bool hasReadSideEffect() const
    {
        if (!readAction.isEmpty() || access == QStringLiteral("write-only"))
            return true;
        for (const SvdField &f : fields)
            if (!f.readAction.isEmpty())
                return true;
        return false;
    }
};

// A contiguous register region of a peripheral (SVD <addressBlock>).
struct SvdAddressBlock
{
    quint64 offset = 0;   // relative to baseAddress
    quint64 size   = 0;   // bytes
    QString usage;        // "registers" / "reserved" / ...
};

// A peripheral instance (SVD <peripheral>). After parsing, derivedFrom is
// resolved, so registers/addressBlocks are always fully populated here.
struct SvdPeripheral
{
    QString name;         // "DMA1"
    QString groupName;    // "DMA"
    QString description;
    quint64 baseAddress = 0;
    QList<SvdAddressBlock> addressBlocks;
    QList<SvdRegister>     registers;

    quint64 addressOf(const SvdRegister &r) const { return baseAddress + r.addressOffset; }
};

struct SvdCpu
{
    QString name;         // "CM7"
    QString endian;       // "little"
};

// The whole device (one SVD file).
struct SvdDevice
{
    QString name;         // "STM32H723"
    QString version;
    QString description;
    SvdCpu  cpu;
    quint32 defaultResetValue = 0;          // device-level fallbacks
    quint32 defaultResetMask  = 0xFFFFFFFF;
    int     defaultSizeBits   = 32;
    QList<SvdPeripheral> peripherals;

    bool isValid() const { return !name.isEmpty() && !peripherals.isEmpty(); }

    const SvdPeripheral *findPeripheral(const QString &n) const
    {
        for (const SvdPeripheral &p : peripherals)
            if (p.name.compare(n, Qt::CaseInsensitive) == 0)
                return &p;
        return nullptr;
    }
};
