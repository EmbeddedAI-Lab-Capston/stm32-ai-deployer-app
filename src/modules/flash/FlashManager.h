#pragma once

#include <QObject>
#include <QString>

// Handles firmware flashing via STM32_Programmer_CLI (QProcess).
class FlashManager : public QObject
{
    Q_OBJECT

public:
    explicit FlashManager(QObject *parent = nullptr);
    ~FlashManager() override;

    // TODO: Validate that the CLI executable exists at the configured path
    // bool isCliAvailable() const;

    // TODO: Start async flash of the given .hex/.bin file to the connected board
    // void flashFile(const QString &filePath, const QString &boardTarget);

    // TODO: Abort an in-progress flash operation
    // void cancelFlash();

    // TODO: Return true if a flash operation is currently running
    // bool isFlashing() const;

signals:
    // TODO: Emitted when flash progress changes (0–100)
    // void progressChanged(int percent);

    // TODO: Emitted when flash completes successfully
    // void flashSucceeded();

    // TODO: Emitted when flash fails with an error message
    // void flashFailed(const QString &errorMessage);

    // TODO: Emitted with a line of raw CLI output (for log display)
    // void cliOutputLine(const QString &line);
};
