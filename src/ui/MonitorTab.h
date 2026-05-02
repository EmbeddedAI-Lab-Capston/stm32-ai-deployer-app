#pragma once

#include <QWidget>

class SerialWorker;
class QThread;

// Tab widget for Module 3: UART Monitor
class MonitorTab : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorTab(QWidget *parent = nullptr);
    ~MonitorTab() override;

private:
    void setupUi();

    SerialWorker *m_serialWorker  = nullptr;
    QThread      *m_workerThread  = nullptr;
};
