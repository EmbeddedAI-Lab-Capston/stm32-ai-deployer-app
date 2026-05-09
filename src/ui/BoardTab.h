#pragma once

#include <QWidget>
#include "core/AppState.h"
#include "modules/serial/PacketParser.h"

class SerialManager;
class QLabel;
class QComboBox;
class QLineEdit;
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

private:
    void setupUi();
    void updateStatusLabel();

    AppState      *m_appState = nullptr;
    SerialManager *m_serial   = nullptr;

    bool m_serialConnected = false;
    bool m_bootDetected    = false;

    // ── Board info display ───────────────────────────────────────────────
    QLabel *m_infoModel  = nullptr;
    QLabel *m_infoFlash  = nullptr;
    QLabel *m_infoRam    = nullptr;
    QLabel *m_infoClock  = nullptr;
    QLabel *m_infoStatus = nullptr;

    // ── Sensor config ────────────────────────────────────────────────────
    QComboBox *m_sensorCombo = nullptr;
    QWidget   *m_pinWidget   = nullptr;
    QLabel    *m_pin1Label   = nullptr;
    QLabel    *m_pin2Label   = nullptr;
    QLineEdit *m_pin1Edit    = nullptr;
    QLineEdit *m_pin2Edit    = nullptr;
};
