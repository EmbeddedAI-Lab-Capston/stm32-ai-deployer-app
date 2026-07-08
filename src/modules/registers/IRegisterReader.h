#pragma once
#include "RegisterReadModel.h"

#include <QObject>
#include <QString>

// ── IRegisterReader ────────────────────────────────────────────────────────
// Abstract read-backend interface. RegisterInspector depends on this, not on
// a concrete implementation, so a future native backend (e.g. probe-rs/pyOCD)
// can replace STM32_Programmer_CLI without touching orchestration, decode, or
// UI (plan Bolum 1d / docs/register_inspector_plan.md Bolum 9). CliRegisterReader
// is the only implementation today.
//
// setCliPath() is deliberately NOT part of this interface — it is CLI-specific
// configuration, not a general read-backend concept. RegisterInspector talks
// to CliRegisterReader concretely for that one setting.
class IRegisterReader : public QObject
{
    Q_OBJECT
public:
    explicit IRegisterReader(QObject *parent = nullptr) : QObject(parent) {}
    ~IRegisterReader() override = default;

    virtual void setStlinkSn(const QString &sn) = 0;      // empty = let backend pick
    virtual void setConnectMode(const QString &mode) = 0; // "HOTPLUG" (default) | "UR"

    virtual bool isBusy() const = 0;

    // Execute a plan. Emits readFinished (or readFailed if it could not even start).
    virtual void read(const ReadPlan &plan) = 0;

signals:
    void readFinished(const RegisterReadResult &result);
    void readFailed(const QString &message);
};
