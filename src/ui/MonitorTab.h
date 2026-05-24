#pragma once

#include <QWidget>
#include "core/AppState.h"
#include "modules/serial/PacketParser.h"
#include "modules/serial/CircularBuffer.h"

class SerialManager;
class PacketParser;
class QTextEdit;
class QLabel;
class QCheckBox;
class QScrollBar;
class QSpinBox;
class QDoubleSpinBox;
class QTimer;

// ── MonitorTab ────────────────────────────────────────────────────────────
// UART terminal + inference metrics.
// Connection controls (port/baud/connect) have moved to Sidebar.
// This tab only observes AppState and renders data.
class MonitorTab : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorTab(AppState      *state,
                        SerialManager *serial,
                        QWidget       *parent = nullptr);
    ~MonitorTab() override;

private slots:
    void onClearClicked();
    void onSaveClicked();
    void onSimulationToggled(bool checked);
    void sendSimulatedSensorFrame();

    // AppState / Serial reactions
    void onConnectionChanged(bool connected, const QString &info);
    void onBoardChanged(const BoardInfo &board);
    void appendRawLog(const QString &line);
    void onBootReceived(const BootData &data);
    void onInferenceReceived(const InferenceData &data);
    void onSysReceived(const SysData &data);
    void onSerialError(const QString &msg);

private:
    void setupUi();
    void appendHtmlLine(const QString &color, const QString &text);
    void startHardwareSimulation();
    void stopHardwareSimulation();

    AppState        *m_appState     = nullptr;
    SerialManager   *m_serialManager = nullptr;

    // Circular buffer — last 500 inference records (Phase 6 charts)
    CircularBuffer<InferenceData> m_infBuffer{500};

    // Status bar (top of terminal)
    QLabel    *m_statusBar   = nullptr;

    // Simulation toggle
    QCheckBox *m_simCheck    = nullptr;
    QSpinBox *m_simIntervalSpin = nullptr;
    QDoubleSpinBox *m_simMinSpin = nullptr;
    QDoubleSpinBox *m_simMaxSpin = nullptr;
    QTimer *m_simTimer = nullptr;
    quint32 m_simSeed = 1234;
    quint32 m_simSentCount = 0;
    quint32 m_simResponseCount = 0;

    // Terminal
    QTextEdit *m_terminal = nullptr;

    // Metric cards
    QLabel *m_metricInfValue   = nullptr;
    QLabel *m_metricRamValue   = nullptr;
    QLabel *m_metricLabelValue = nullptr;

    // Bottom bar
    QLabel *m_bottomInfoLabel = nullptr;
};
