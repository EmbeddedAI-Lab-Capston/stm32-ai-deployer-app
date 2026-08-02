#include "ElfSymbolSource.h"
#include "NmSymbolParser.h"

#include <QFile>
#include <QTimer>

namespace {
constexpr int kTimeoutMs = 15000;
}

ElfSymbolSource::ElfSymbolSource(QObject *parent) : QObject(parent) {}

void ElfSymbolSource::setNmPath(const QString &path) { m_nmPath = path; }

void ElfSymbolSource::load(const QString &elfPath)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        emit failed(QStringLiteral("ElfSymbolSource mesgul"));
        return;
    }
    if (m_nmPath.isEmpty()) {
        emit failed(QStringLiteral("arm-none-eabi-nm yolu ayarlanmamis"));
        return;
    }
    if (!QFile::exists(elfPath)) {
        emit failed(QStringLiteral("ELF dosyasi bulunamadi: %1").arg(elfPath));
        return;
    }

    m_pendingElfPath = elfPath;
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ElfSymbolSource::onFinished);
    connect(m_process, &QProcess::errorOccurred, this, &ElfSymbolSource::onErrorOccurred);

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
            if (m_process && m_process->state() != QProcess::NotRunning) {
                m_process->kill();
                emit failed(QStringLiteral("nm zaman asimina ugradi (%1 sn)").arg(kTimeoutMs / 1000));
            }
        });
    }
    m_timeoutTimer->start(kTimeoutMs);

    m_process->start(m_nmPath, { QStringLiteral("-S"), QStringLiteral("--defined-only"), elfPath });
}

void ElfSymbolSource::onFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);
    if (m_timeoutTimer) m_timeoutTimer->stop();

    QProcess *proc = m_process;
    m_process = nullptr;
    if (!proc) return;

    const QString out = QString::fromLocal8Bit(proc->readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(proc->readAllStandardError());
    proc->deleteLater();

    if (status != QProcess::NormalExit) {
        emit failed(QStringLiteral("nm beklenmedik sekilde kapandi"));
        return;
    }

    // "Exit code 0 even on error" is a recurring ST-toolchain caveat in this
    // app (see HexDumpParser::errorMarkers) — apply the same scepticism here:
    // an empty result is a failure regardless of exit code.
    if (out.trimmed().isEmpty()) {
        emit failed(err.isEmpty() ? QStringLiteral("nm bos cikti dondurdu") : err);
        return;
    }

    emit loaded(NmSymbolParser::parse(out), m_pendingElfPath);
}

void ElfSymbolSource::onErrorOccurred(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        if (m_timeoutTimer) m_timeoutTimer->stop();
        emit failed(QStringLiteral("nm baslatilamadi - yol dogru mu? (%1)").arg(m_nmPath));
    }
    // Other errors are followed by ::finished(), already handled above.
}
