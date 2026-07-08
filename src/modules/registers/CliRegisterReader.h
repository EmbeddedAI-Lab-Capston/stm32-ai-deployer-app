#pragma once
#include "IRegisterReader.h"

#include <QMetaType>
#include <QStringList>

class CliRunner;

// ── CliRegisterReader ──────────────────────────────────────────────────────
// Executes a ReadPlan against STM32_Programmer_CLI and turns the raw hex dump
// into addr->value. Reuses CliRunner (the same async QProcess wrapper
// FlashManager uses) and HexDumpParser. Tolerant of block failures: a range the
// CLI could not read becomes an entry in result.errors; the rest of the snapshot
// still comes back. One read at a time. See plan Bolum 3.5 / 5.1.
//
// Concurrency: this class does not itself serialize against flash/probe/N6-reset
// — that shared ST-Link guard lives in Backend (Bolum 5.3, Faz 3).
class CliRegisterReader : public IRegisterReader
{
    Q_OBJECT
public:
    explicit CliRegisterReader(QObject *parent = nullptr);

    void setCliPath(const QString &path);   // CLI-specific; not part of IRegisterReader
    void setStlinkSn(const QString &sn) override;
    void setConnectMode(const QString &mode) override;

    bool isBusy() const override { return m_busy; }

    void read(const ReadPlan &plan) override;

private:
    void onCliFinished(bool success, int exitCode);

    CliRunner  *m_cli = nullptr;
    QString     m_stlinkSn;
    QString     m_connectMode = QStringLiteral("HOTPLUG");
    bool        m_busy = false;
    ReadPlan    m_currentPlan;
    QStringList m_output;      // accumulated stdout+stderr for the active read
};

Q_DECLARE_METATYPE(RegisterReadResult)
