#include "SerialWorker.h"

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
    , m_parser(new PacketParser(this))
{
    connect(m_parser, &PacketParser::inferenceReceived,
            this,     &SerialWorker::inferenceReceived);
    connect(m_parser, &PacketParser::sysReceived,
            this,     &SerialWorker::sysReceived);
    connect(m_parser, &PacketParser::bootReceived,
            this,     &SerialWorker::bootReceived);
    connect(m_parser, &PacketParser::errorReceived,
            this,     &SerialWorker::errorReceived);
    connect(m_parser, &PacketParser::rawLineReceived,
            this,     &SerialWorker::rawLineReceived);
    connect(m_parser, &PacketParser::malformedPacket,
            this,     &SerialWorker::malformedPacket);
}

SerialWorker::~SerialWorker()
{
    if (m_port && m_port->isOpen())
        m_port->close();
}

void SerialWorker::connectToPort(const QString &portName, qint32 baudRate)
{
    if (m_port) {
        if (m_port->isOpen())
            m_port->close();
        m_port->deleteLater();
        m_port = nullptr;
    }

    m_port = new QSerialPort(this);
    m_port->setPortName(portName);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_port, &QSerialPort::readyRead,
            this,   &SerialWorker::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred,
            this,   &SerialWorker::onSerialError);

    if (!m_port->open(QIODevice::ReadOnly)) {
        emit errorOccurred(
            QString("Port açılamadı: %1 — %2")
                .arg(portName, m_port->errorString()));
        return;
    }

    emit connected(portName, baudRate);
}

void SerialWorker::disconnectPort()
{
    if (m_port && m_port->isOpen()) {
        m_port->close();
        emit disconnected();
    }
}

void SerialWorker::onReadyRead()
{
    m_parser->feed(m_port->readAll());
}

void SerialWorker::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    // NOTE: ResourceError means the device was physically removed
    if (error == QSerialPort::ResourceError) {
        emit errorOccurred(tr("Bağlantı kesildi — kart çıkarıldı olabilir."));
        disconnectPort();
        return;
    }

    emit errorOccurred(
        QString(tr("Seri port hatası: %1")).arg(m_port->errorString()));
}
