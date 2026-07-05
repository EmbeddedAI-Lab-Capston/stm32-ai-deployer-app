#pragma once
#include "RegisterReadModel.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QtGlobal>

// ── Decoded snapshot model ─────────────────────────────────────────────────
// The SVD + raw-read combined view (plan Bolum 3.6 / 4.2). Produced by
// RegisterDecoder, held by RegisterInspector, and flattened to QVariant by
// Backend for QML / JSON. UI-independent so the same tree feeds QML, the JSON
// export (Faz 5) and any future LLM layer (Bolum 9).

// Per-register status in a snapshot.
enum class RegStatus
{
    Ok,             // read and decoded
    SkippedSideEffect,  // not read: readAction / data-register blacklist
    SkippedWriteOnly,   // not read: write-only
    ClockOff,       // not read: peripheral clock disabled
    Unreadable      // attempted but no value came back (block error)
};

// Whether a peripheral's clock is on (from RCC decode).
enum class ClockStatus
{
    On,
    Off,
    Unknown         // always-on / not gateable — read regardless
};

struct DecodedField
{
    QString name;
    quint32 value = 0;
    QString enumName;        // symbolic value name if matched (may be empty)
    quint32 resetValue = 0;
    bool    changed = false; // value != resetValue
    int     bitOffset = 0;
    int     bitWidth = 1;
    QString description;
};

struct DecodedRegister
{
    QString   name;
    QString   displayName;
    quint64   addr = 0;
    quint32   rawValue = 0;
    quint32   resetValue = 0;
    quint32   resetMask = 0xFFFFFFFF;
    bool      changedFromReset = false;
    RegStatus status = RegStatus::Ok;
    QString   description;
    QList<DecodedField> fields;
};

struct DecodedPeripheral
{
    QString     name;
    QString     groupName;
    QString     description;
    quint64     baseAddress = 0;
    ClockStatus clock = ClockStatus::Unknown;
    QList<DecodedRegister> registers;
};

// A full snapshot: metadata + decoded tree + block errors.
struct RegisterSnapshot
{
    QDateTime takenAt;
    QString   boardName;
    QString   deviceName;
    QString   svdFile;
    QString   connectMode;      // HOTPLUG / UR
    QString   supportLevel;     // stable / experimental
    QList<DecodedPeripheral>  peripherals;
    QList<RegisterReadError>  errors;
    bool      valid = false;

    int changedRegisterCount() const
    {
        int n = 0;
        for (const DecodedPeripheral &p : peripherals)
            for (const DecodedRegister &r : p.registers)
                if (r.status == RegStatus::Ok && r.changedFromReset)
                    ++n;
        return n;
    }
};
