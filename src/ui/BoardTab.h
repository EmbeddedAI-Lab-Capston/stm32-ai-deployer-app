#pragma once

#include <QWidget>
#include "core/AppState.h"

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
    explicit BoardTab(AppState *state, QWidget *parent = nullptr);
    ~BoardTab() override;

private slots:
    void onBoardChanged(const BoardInfo &board);
    void onSensorChanged(int index);
    void onAddCustomBoardClicked();

private:
    void setupUi();

    AppState *m_appState = nullptr;

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
