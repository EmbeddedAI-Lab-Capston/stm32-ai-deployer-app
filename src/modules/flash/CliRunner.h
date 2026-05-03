#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

// Thin QProcess wrapper for STM32_Programmer_CLI.
// All I/O is asynchronous — never calls waitForFinished().
class CliRunner : public QObject
{
    Q_OBJECT

public:
    explicit CliRunner(QObject *parent = nullptr);
    ~CliRunner() override;

    void    setCliPath(const QString &path);
    QString cliPath()   const { return m_cliPath; }
    bool    isRunning() const;
    void    cancel();

public slots:
    void run(const QStringList &args);

signals:
    void outputLine(const QString &line);
    void errorLine(const QString &line);
    void started();
    void finished(bool success, int exitCode);
    void progressChanged(int percent);

private slots:
    void onReadyReadStdOut();
    void onReadyReadStdErr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    void parseProgressFromLine(const QString &line);

    QProcess *m_process = nullptr;
    QString   m_cliPath;
};
