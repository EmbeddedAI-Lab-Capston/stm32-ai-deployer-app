#include "SerialSimulator.h"

#include <QTimer>
#include <QRandomGenerator>

SerialSimulator::SerialSimulator(PacketParser *parser, QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_parser(parser)
{
    connect(m_timer, &QTimer::timeout, this, &SerialSimulator::generatePacket);
}

void SerialSimulator::start(int intervalMs)
{
    m_uptime = 0;
    m_timer->start(intervalMs);

    // Emit a boot packet immediately so the terminal shows the card banner
    QByteArray boot =
        QStringLiteral("\xC2\xA7{\"t\":\"boot\","
                       "\"card\":\"STM32F4\","
                       "\"sdk\":\"EdgeAI_v1.0\","
                       "\"model\":\"MLP_INT8\","
                       "\"baud\":115200}\r\n")
            .toUtf8();
    m_parser->feed(boot);
}

void SerialSimulator::stop()
{
    m_timer->stop();
}

void SerialSimulator::generatePacket()
{
    ++m_uptime;

    const quint32 infUs  = 7800 + QRandomGenerator::global()->bounded(800u);
    const quint8  acc    = static_cast<quint8>(94 + QRandomGenerator::global()->bounded(4u));
    const QString label  = m_labels.at(
        static_cast<int>(QRandomGenerator::global()->bounded(
            static_cast<quint32>(m_labels.size()))));

    QByteArray inf = QString(
        "\xC2\xA7{\"t\":\"inf\","
        "\"model\":\"MLP_INT8\","
        "\"inf_us\":%1,"
        "\"ram_b\":3072,"
        "\"acc_pct\":%2,"
        "\"label\":\"%3\","
        "\"card\":\"STM32F4\"}\r\n")
        .arg(infUs).arg(acc).arg(label)
        .toUtf8();
    m_parser->feed(inf);

    // Send a sys packet every 5 inference packets
    if (m_uptime % 5 == 0) {
        QByteArray sys = QString(
            "\xC2\xA7{\"t\":\"sys\","
            "\"uptime_s\":%1,"
            "\"temp_c\":38,"
            "\"free_ram_b\":185000,"
            "\"state\":\"running\"}\r\n")
            .arg(m_uptime)
            .toUtf8();
        m_parser->feed(sys);
    }
}
