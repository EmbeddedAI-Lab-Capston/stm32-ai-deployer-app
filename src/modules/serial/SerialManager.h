#pragma once

#include <QObject>
#include <QThread>
#include <QSerialPortInfo>
#include "SerialWorker.h"

// Lives on the main thread. Owns the serial QThread and SerialWorker.
// All cross-thread communication is handled internally via queued connections.
class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager() override;

    static QList<QSerialPortInfo> availablePorts();

    // Returns the first port with ST-Microelectronics VID (0x0483), or isNull()==true if not found
    static QSerialPortInfo findStLink();

    bool isConnected() const { return m_connected; }

public slots:
    void connectToPort(const QString &portName, qint32 baudRate = 115200);
    void disconnectPort();

signals:
    void inferenceReceived(const InferenceData &data);
    void sysReceived(const SysData &data);
    void bootReceived(const BootData &data);
    void errorReceived(const ErrorData &data);
    void rawLineReceived(const QString &line);
    void connectionChanged(bool connected, const QString &info);
    void errorOccurred(const QString &msg);

    // Internal — used to forward commands to the worker thread
    void requestConnect(const QString &portName, qint32 baudRate);
    void requestDisconnect();

private:
    QThread      *m_thread    = nullptr;
    SerialWorker *m_worker    = nullptr;
    bool          m_connected = false;
};
