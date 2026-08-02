#pragma once
#include "SymbolModel.h"

#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>

class QTimer;

// ── ElfSymbolSource ───────────────────────────────────────────────────────
// Thin async QProcess wrapper: runs `<nm> -S --defined-only <elf>` and
// parses the output via NmSymbolParser. Never blocks (docs/variable_watcher_plan.md
// Bolum 6.1) — same async-only convention as CliRunner elsewhere in this app.
class ElfSymbolSource : public QObject
{
    Q_OBJECT

public:
    explicit ElfSymbolSource(QObject *parent = nullptr);

    void setNmPath(const QString &path);
    void load(const QString &elfPath);   // 15s timeout

signals:
    void loaded(const QList<Symbol> &symbols, const QString &elfPath);
    void failed(const QString &message);

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);

private:
    QString    m_nmPath;
    QProcess  *m_process = nullptr;
    QString    m_pendingElfPath;
    QTimer    *m_timeoutTimer = nullptr;
};
