#include "PacketParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

PacketParser::PacketParser(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<InferenceData>("InferenceData");
    qRegisterMetaType<SysData>("SysData");
    qRegisterMetaType<BootData>("BootData");
    qRegisterMetaType<ErrorData>("ErrorData");
}

void PacketParser::feed(const QByteArray &data)
{
    m_buffer.append(data);

    if (m_buffer.size() > 4096) {
        qWarning("PacketParser: buffer overflow, clearing");
        m_buffer.clear();
        return;
    }

    while (true) {
        int end = m_buffer.indexOf("\r\n");
        if (end < 0) break;

        QByteArray line = m_buffer.left(end).trimmed();
        m_buffer.remove(0, end + 2);

        if (line.isEmpty()) continue;

        // § = UTF-8 0xC2 0xA7
        if (line.startsWith("\xC2\xA7")) {
            processPacket(line.mid(2));
        } else {
            emit rawLineReceived(QString::fromUtf8(line));
        }
    }
}

void PacketParser::processPacket(const QByteArray &jsonBytes)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &err);

    if (doc.isNull() || !doc.isObject()) {
        emit malformedPacket(jsonBytes);
        return;
    }

    QJsonObject obj  = doc.object();
    QString     type = obj.value("t").toString();

    if (type == "inf") {
        InferenceData d;
        d.model     = obj.value("model").toString();
        d.inf_us    = static_cast<quint32>(obj.value("inf_us").toInt());
        d.ram_b     = static_cast<quint32>(obj.value("ram_b").toInt());
        d.acc_pct   = static_cast<quint8>(obj.value("acc_pct").toInt());
        d.label     = obj.value("label").toString();
        d.card      = obj.value("card").toString();
        d.timestamp = QDateTime::currentDateTime();
        emit inferenceReceived(d);
    }
    else if (type == "sys") {
        SysData d;
        d.uptime_s   = static_cast<quint32>(obj.value("uptime_s").toInt());
        d.temp_c     = static_cast<quint8>(obj.value("temp_c").toInt());
        d.free_ram_b = static_cast<quint32>(obj.value("free_ram_b").toInt());
        d.state      = obj.value("state").toString();
        d.timestamp  = QDateTime::currentDateTime();
        emit sysReceived(d);
    }
    else if (type == "boot") {
        BootData d;
        d.card  = obj.value("card").toString();
        d.sdk   = obj.value("sdk").toString();
        d.model = obj.value("model").toString();
        d.baud  = static_cast<quint32>(obj.value("baud").toInt());
        d.flash_kb = static_cast<quint32>(obj.value("flash_kb").toInt());
        d.ram_kb = static_cast<quint32>(obj.value("ram_kb").toInt());
        d.clock_mhz = static_cast<quint32>(obj.value("clock_mhz").toInt());
        emit bootReceived(d);
    }
    else if (type == "err") {
        ErrorData d;
        d.code      = static_cast<quint8>(obj.value("code").toInt());
        d.msg       = obj.value("msg").toString();
        d.timestamp = QDateTime::currentDateTime();
        emit errorReceived(d);
    }
    else {
        emit malformedPacket(jsonBytes);
    }
}
