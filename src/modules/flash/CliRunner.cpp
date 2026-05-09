#include "CliRunner.h"

#include <QFile>

CliRunner::CliRunner(QObject *parent) : QObject(parent) {}

CliRunner::~CliRunner()
{
    if (isRunning())
        m_process->kill();
}

void CliRunner::setCliPath(const QString &path) { m_cliPath = path; }

void CliRunner::setEnvironment(const QProcessEnvironment &env)
{
    m_env    = env;
    m_envSet = true;
}

bool CliRunner::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void CliRunner::cancel()
{
    if (isRunning()) {
        m_process->kill();
        emit errorLine(tr("İşlem iptal edildi."));
    }
}

void CliRunner::run(const QStringList &args)
{
    if (isRunning()) {
        emit errorLine(tr("Zaten çalışan bir işlem var."));
        return;
    }
    if (m_cliPath.isEmpty() || !QFile::exists(m_cliPath)) {
        emit errorLine(tr("Çalıştırılabilir bulunamadı: ") + m_cliPath);
        emit finished(false, -1);
        return;
    }

    m_process = new QProcess(this);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &CliRunner::onReadyReadStdOut);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &CliRunner::onReadyReadStdErr);
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &CliRunner::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &CliRunner::onProcessError);

    if (m_envSet)
        m_process->setProcessEnvironment(m_env);

    emit started();
    m_process->start(m_cliPath, args);
}

void CliRunner::onReadyReadStdOut()
{
    while (m_process->canReadLine()) {
        const QString line =
            QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (!line.isEmpty()) {
            emit outputLine(line);
            parseProgressFromLine(line);
        }
    }
}

void CliRunner::onReadyReadStdErr()
{
    while (m_process->canReadLine()) {
        const QString line =
            QString::fromLocal8Bit(m_process->readLine()).trimmed();
        if (!line.isEmpty())
            emit errorLine(line);
    }
}

void CliRunner::parseProgressFromLine(const QString &line)
{
    if      (line.contains("Connecting"))           emit progressChanged(5);
    else if (line.contains("Memory Programm"))      emit progressChanged(30);
    else if (line.contains("download complete"))    emit progressChanged(80);
    else if (line.contains("Verif"))                emit progressChanged(90);
    else if (line.contains("Complete") ||
             line.contains("successful"))           emit progressChanged(100);
}

void CliRunner::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    // Drain any output that arrived after the last readyRead signal
    if (m_process) {
        onReadyReadStdOut();
        onReadyReadStdErr();
    }

    const bool success = (exitCode == 0 && status == QProcess::NormalExit);
    QProcess *proc = m_process;
    m_process = nullptr;
    emit finished(success, exitCode);
    proc->deleteLater();
}

void CliRunner::onProcessError(QProcess::ProcessError error)
{
    QString msg;
    switch (error) {
        case QProcess::FailedToStart: msg = tr("CLI başlatılamadı — yol doğru mu?"); break;
        case QProcess::Crashed:       msg = tr("CLI beklenmedik şekilde kapandı.");   break;
        case QProcess::Timedout:      msg = tr("CLI zaman aşımına uğradı.");          break;
        default:                      msg = tr("Bilinmeyen QProcess hatası.");
    }
    emit errorLine(msg);
    emit finished(false, -1);
}
