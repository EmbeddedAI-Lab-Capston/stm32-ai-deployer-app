#pragma once

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QString>

struct InferenceData {
    QString   model;
    quint32   inf_us;
    quint32   ram_b;
    quint8    acc_pct;
    QString   label;
    QString   card;
    QDateTime timestamp;
};

struct SysData {
    quint32   uptime_s;
    quint8    temp_c;
    quint32   free_ram_b;
    QString   state;
    QDateTime timestamp;
};

struct BootData {
    QString  card;
    QString  sdk;
    QString  model;
    quint32  baud;
    quint32  flash_kb = 0;
    quint32  ram_kb = 0;
    quint32  clock_mhz = 0;
};

struct ErrorData {
    quint8    code;
    QString   msg;
    QDateTime timestamp;
};

Q_DECLARE_METATYPE(InferenceData)
Q_DECLARE_METATYPE(SysData)
Q_DECLARE_METATYPE(BootData)
Q_DECLARE_METATYPE(ErrorData)

// Parses the §{JSON}\r\n protocol. Call feed() with raw bytes as they arrive.
class PacketParser : public QObject
{
    Q_OBJECT

public:
    explicit PacketParser(QObject *parent = nullptr);

public slots:
    void feed(const QByteArray &data);

signals:
    void inferenceReceived(const InferenceData &data);
    void sysReceived(const SysData &data);
    void bootReceived(const BootData &data);
    void errorReceived(const ErrorData &data);
    void rawLineReceived(const QString &line);
    void malformedPacket(const QByteArray &raw);

private:
    void processPacket(const QByteArray &jsonBytes);

    QByteArray m_buffer;
};
