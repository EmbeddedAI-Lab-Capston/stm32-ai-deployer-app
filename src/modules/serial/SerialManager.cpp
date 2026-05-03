#include "SerialManager.h"

SerialManager::SerialManager(QObject *parent) : QObject(parent)
{
    m_thread = new QThread(this);
    m_worker = new SerialWorker();
    m_worker->moveToThread(m_thread);

    connect(m_worker, &SerialWorker::inferenceReceived,
            this,     &SerialManager::inferenceReceived);
    connect(m_worker, &SerialWorker::sysReceived,
            this,     &SerialManager::sysReceived);
    connect(m_worker, &SerialWorker::bootReceived,
            this,     &SerialManager::bootReceived);
    connect(m_worker, &SerialWorker::errorReceived,
            this,     &SerialManager::errorReceived);
    connect(m_worker, &SerialWorker::rawLineReceived,
            this,     &SerialManager::rawLineReceived);

    connect(m_worker, &SerialWorker::connected,
            this, [this](const QString &port, qint32 baud) {
                m_connected = true;
                emit connectionChanged(true,
                    QString("%1 @ %2").arg(port).arg(baud));
            });
    connect(m_worker, &SerialWorker::disconnected,
            this, [this]() {
                m_connected = false;
                emit connectionChanged(false, QString());
            });
    connect(m_worker, &SerialWorker::errorOccurred,
            this,     &SerialManager::errorOccurred);

    // Thread-safe command forwarding
    connect(this,     &SerialManager::requestConnect,
            m_worker, &SerialWorker::connectToPort,
            Qt::QueuedConnection);
    connect(this,     &SerialManager::requestDisconnect,
            m_worker, &SerialWorker::disconnectPort,
            Qt::QueuedConnection);

    connect(m_thread, &QThread::finished,
            m_worker, &QObject::deleteLater);

    m_thread->start();
}

SerialManager::~SerialManager()
{
    m_thread->quit();
    m_thread->wait(3000);
}

QList<QSerialPortInfo> SerialManager::availablePorts()
{
    return QSerialPortInfo::availablePorts();
}

QSerialPortInfo SerialManager::findStLink()
{
    const quint16 ST_VID = 0x0483;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        if (info.hasVendorIdentifier() && info.vendorIdentifier() == ST_VID)
            return info;
    }
    return QSerialPortInfo();
}

void SerialManager::connectToPort(const QString &portName, qint32 baudRate)
{
    emit requestConnect(portName, baudRate);
}

void SerialManager::disconnectPort()
{
    emit requestDisconnect();
}
