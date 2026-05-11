#pragma once

#include <QWidget>
#include "core/AppState.h"
#include "modules/serial/PacketParser.h"

class SerialManager;
class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QProcess;
class QWidget;

// ── BoardTab ─────────────────────────────────────────────────────────────
// Shows active board info (read from AppState) and lets the user configure
// the sensor wiring.  Board selection itself is now in the Sidebar.
class BoardTab : public QWidget
{
    Q_OBJECT

public:
    explicit BoardTab(AppState *state, SerialManager *serial, QWidget *parent = nullptr);
    ~BoardTab() override;

private slots:
    void onBoardChanged(const BoardInfo &board);
    void onSensorChanged(int index);
    void onAddCustomBoardClicked();
    void onSerialConnectionChanged(bool connected, const QString &info);
    void onSerialBootReceived(const BootData &data);
    void onBoardComboChanged(int index);
    void onPortComboChanged(int index);
    void onBaudComboChanged(int index);
    void onConnectClicked();
    void onRefreshClicked();
    void onStLinkProbeFinished(int exitCode);

private:
    void setupUi();
    void updateStatusLabel();
    void populateBoards();
    void populatePorts();
    void populateBauds();
    void ensureBoardVisible(const BoardInfo &board);
    void startStLinkBoardProbe();
    void applyDetectedStLinkBoard(const QString &probeOutput);

    AppState      *m_appState = nullptr;
    SerialManager *m_serial   = nullptr;

    bool m_serialConnected = false;
    bool m_bootDetected    = false;
    bool m_stlinkDetected  = false;

    // ── Board info display ───────────────────────────────────────────────
    QLabel *m_infoModel  = nullptr;
    QLabel *m_infoFlash  = nullptr;
    QLabel *m_infoRam    = nullptr;
    QLabel *m_infoClock  = nullptr;
    QLabel *m_infoPort   = nullptr;
    QLabel *m_infoProbeBoard = nullptr;
    QLabel *m_infoDeviceId = nullptr;
    QLabel *m_infoRevision = nullptr;
    QLabel *m_infoDeviceName = nullptr;
    QLabel *m_infoNvm = nullptr;
    QLabel *m_infoCpu = nullptr;
    QLabel *m_infoStlink = nullptr;
    QLabel *m_infoVoltage = nullptr;
    QLabel *m_infoStatus = nullptr;

    QComboBox   *m_boardCombo  = nullptr;
    QComboBox   *m_portCombo   = nullptr;
    QComboBox   *m_baudCombo   = nullptr;
    QPushButton *m_connectBtn  = nullptr;
    QPushButton *m_refreshBtn  = nullptr;
    QLabel      *m_connStatusLabel = nullptr;
    QProcess    *m_stlinkProbe = nullptr;
    QString      m_stlinkPortName;

    // ── Sensor config ────────────────────────────────────────────────────
    QComboBox *m_sensorCombo = nullptr;
    QWidget   *m_pinWidget   = nullptr;
    QLabel    *m_pin1Label   = nullptr;
    QLabel    *m_pin2Label   = nullptr;
    QLineEdit *m_pin1Edit    = nullptr;
    QLineEdit *m_pin2Edit    = nullptr;
};
