#pragma once

#include <QWidget>

class SerialWorker;
class QThread;
class QComboBox;
class QPushButton;
class QLabel;
class QTextEdit;

class MonitorTab : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorTab(QWidget *parent = nullptr);
    ~MonitorTab() override;

private slots:
    void onConnectClicked();
    void onClearClicked();
    void onSaveClicked();

private:
    void setupUi();

    SerialWorker *m_serialWorker = nullptr;
    QThread      *m_workerThread = nullptr;

    QComboBox   *m_portCombo  = nullptr;
    QComboBox   *m_baudCombo  = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QLabel      *m_connStatus = nullptr;

    QTextEdit *m_terminal = nullptr;

    QLabel *m_metricInfValue   = nullptr;
    QLabel *m_metricRamValue   = nullptr;
    QLabel *m_metricLabelValue = nullptr;

    QLabel *m_bottomInfoLabel = nullptr;
    bool    m_connected       = false;
};
