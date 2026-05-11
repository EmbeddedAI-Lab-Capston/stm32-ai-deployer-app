#pragma once

#include <QObject>
#include <QSerialPort>
#include "PacketParser.h"

// Lives on a dedicated QThread. Owns QSerialPort and PacketParser.
// Communicate via queued signal/slot — never call methods directly from another thread.
class SerialWorker : public QObject
{
    Q_OBJECT

public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker() override;

public slots:
    void connectToPort(const QString &portName, qint32 baudRate);
    void disconnectPort();
    void writeLine(const QByteArray &line);
    void requestBoardInfo();

signals:
    void inferenceReceived(const InferenceData &data);
    void sysReceived(const SysData &data);
    void bootReceived(const BootData &data);
    void errorReceived(const ErrorData &data);
    void benchReceived(const BenchData &data);
    void rawLineReceived(const QString &line);
    void malformedPacket(const QByteArray &raw);

    void connected(const QString &portName, qint32 baudRate);
    void disconnected();
    void errorOccurred(const QString &errorMessage);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    QSerialPort  *m_port   = nullptr;
    PacketParser *m_parser = nullptr;
};
