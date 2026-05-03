#pragma once

#include <QObject>
#include <QStringList>
#include "PacketParser.h"

class QTimer;

// Generates fake §{JSON}\r\n packets via QTimer and feeds them into a PacketParser.
// Used for UI development and demo without physical hardware.
// Lives on the main thread alongside PacketParser (no thread boundary crossing).
class SerialSimulator : public QObject
{
    Q_OBJECT

public:
    explicit SerialSimulator(PacketParser *parser, QObject *parent = nullptr);

    void start(int intervalMs = 1000);
    void stop();

private slots:
    void generatePacket();

private:
    QTimer       *m_timer  = nullptr;
    PacketParser *m_parser = nullptr;
    quint32       m_uptime = 0;

    const QStringList m_labels = {
        "walking", "running", "sitting",
        "standing", "upstairs", "downstairs"
    };
};
