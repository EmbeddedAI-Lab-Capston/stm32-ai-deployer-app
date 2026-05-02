#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Runs on a dedicated QThread. Owns QSerialPort and emits parsed UART packets.
// Protocol: §{JSON}\r\n  (§ = 0xC2 0xA7 in UTF-8)
class SerialWorker : public QObject
{
    Q_OBJECT

public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker() override;

    // TODO: Return list of available COM port names from the system
    // static QStringList availablePorts();

    // TODO: Open serial port at the given name and baud rate
    // void openPort(const QString &portName, int baudRate = 115200);

    // TODO: Close the currently open serial port
    // void closePort();

    // TODO: Return true if the serial port is currently open
    // bool isConnected() const;

    // TODO: Return the currently open port name, or empty string
    // QString currentPort() const;

signals:
    // TODO: Emitted for each raw line received (for terminal display)
    // void rawLineReceived(const QString &line);

    // TODO: Emitted when a §{...} JSON packet is parsed
    // void packetReceived(const QJsonObject &packet);

    // TODO: Emitted when the port connects or disconnects
    // void connectionChanged(bool connected);

    // TODO: Emitted on serial port errors
    // void errorOccurred(const QString &errorMessage);

public slots:
    // TODO: Slot to call openPort from the owning thread
    // void onOpenRequested(const QString &portName, int baudRate);

    // TODO: Slot to call closePort from the owning thread
    // void onCloseRequested();
};
