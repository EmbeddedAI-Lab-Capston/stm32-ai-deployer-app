#pragma once

#include <QWidget>
#include "modules/serial/PacketParser.h"
#include "modules/serial/CircularBuffer.h"

class SerialManager;
class SerialSimulator;
class PacketParser;
class QThread;
class QComboBox;
class QPushButton;
class QLabel;
class QTextEdit;
class QCheckBox;
class QScrollBar;

class MonitorTab : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorTab(QWidget *parent = nullptr);
    ~MonitorTab() override;

signals:
    // Forwarded to MainWindow for sidebar / status bar updates
    void connectionStatusChanged(bool connected, const QString &info);
    void inferenceMetricUpdated(const QString &model, double ms, int accPct);

private slots:
    void onConnectClicked();
    void onRefreshPortsClicked();
    void onClearClicked();
    void onSaveClicked();
    void onSimulationToggled(bool checked);

    void onConnectionChanged(bool connected, const QString &info);
    void appendRawLog(const QString &line);
    void onInferenceReceived(const InferenceData &data);
    void onSysReceived(const SysData &data);
    void onSerialError(const QString &msg);

private:
    void setupUi();
    void refreshPorts();
    void appendHtmlLine(const QString &color, const QString &text);

    // Serial backend
    SerialManager   *m_serialManager = nullptr;

    // Simulator — lives on main thread, shares m_simParser
    PacketParser    *m_simParser    = nullptr;
    SerialSimulator *m_simulator    = nullptr;

    // Circular buffer — last 500 inference records (used in Phase 6 for charts)
    CircularBuffer<InferenceData> m_infBuffer{500};

    // Connection panel
    QComboBox   *m_portCombo  = nullptr;
    QComboBox   *m_baudCombo  = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QLabel      *m_connStatus = nullptr;
    QCheckBox   *m_simCheck   = nullptr;

    // Terminal
    QTextEdit *m_terminal = nullptr;

    // Metric cards
    QLabel *m_metricInfValue   = nullptr;
    QLabel *m_metricRamValue   = nullptr;
    QLabel *m_metricLabelValue = nullptr;

    // Bottom bar
    QLabel *m_bottomInfoLabel = nullptr;

    bool m_connected = false;
};
