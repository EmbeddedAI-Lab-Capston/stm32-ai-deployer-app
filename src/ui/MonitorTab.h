#pragma once

#include <QWidget>
#include "core/AppState.h"
#include "modules/serial/PacketParser.h"
#include "modules/serial/CircularBuffer.h"

class SerialManager;
class SerialSimulator;
class PacketParser;
class QTextEdit;
class QLabel;
class QCheckBox;
class QScrollBar;

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

    // AppState / Serial reactions
    void onConnectionChanged(bool connected, const QString &info);
    void onBoardChanged(const BoardInfo &board);
    void appendRawLog(const QString &line);
    void onInferenceReceived(const InferenceData &data);
    void onSysReceived(const SysData &data);
    void onSerialError(const QString &msg);

private:
    void setupUi();
    void appendHtmlLine(const QString &color, const QString &text);

    AppState        *m_appState     = nullptr;
    SerialManager   *m_serialManager = nullptr;

    // Simulator (stays on main thread)
    PacketParser    *m_simParser   = nullptr;
    SerialSimulator *m_simulator   = nullptr;

    // Circular buffer — last 500 inference records (Phase 6 charts)
    CircularBuffer<InferenceData> m_infBuffer{500};

    // Status bar (top of terminal)
    QLabel    *m_statusBar   = nullptr;

    // Simulation toggle
    QCheckBox *m_simCheck    = nullptr;

    // Terminal
    QTextEdit *m_terminal = nullptr;

    // Metric cards
    QLabel *m_metricInfValue   = nullptr;
    QLabel *m_metricRamValue   = nullptr;
    QLabel *m_metricLabelValue = nullptr;

    // Bottom bar
    QLabel *m_bottomInfoLabel = nullptr;
};
