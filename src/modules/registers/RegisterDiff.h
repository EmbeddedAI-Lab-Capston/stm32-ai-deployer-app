#pragma once
#include "RegisterSnapshot.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QtGlobal>

// ── Snapshot diff model ────────────────────────────────────────────────────
// Field-level A/B comparison, consumed by Backend (QML) and RegisterAdvisor
// (LLM prompt). Produced by SnapshotDiffer from two RegisterSnapshot trees;
// UI-independent, same as RegisterSnapshot itself.

struct FieldDiff
{
    QString name;
    int     bitOffset = 0;
    int     bitWidth = 1;
    QString description;
    quint32 valueA = 0;
    quint32 valueB = 0;
    QString enumNameA;   // symbolic name for valueA, if matched
    QString enumNameB;
};

struct RegisterDiff
{
    QString   peripheralName;
    QString   registerName;
    quint64   addr = 0;
    RegStatus statusA = RegStatus::Ok;
    RegStatus statusB = RegStatus::Ok;
    quint32   rawA = 0;
    quint32   rawB = 0;
    QList<FieldDiff> changedFields;   // only fields where A != B
};

// A full A/B comparison. Registers are matched by address (the true unique
// key within a device); only registers whose raw value actually differs (or
// whose read status changed, e.g. clock-off -> on) are included.
struct SnapshotDiff
{
    QDateTime takenAtA;
    QDateTime takenAtB;
    QList<RegisterDiff> changedRegisters;
    bool    comparable = true;   // false if A/B come from different boards/SVDs
    QString incomparableReason;

    int changedFieldCount() const
    {
        int n = 0;
        for (const RegisterDiff &r : changedRegisters)
            n += r.changedFields.size();
        return n;
    }
};
