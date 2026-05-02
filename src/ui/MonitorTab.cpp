#include "MonitorTab.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QThread>
#include "modules/serial/SerialWorker.h"

MonitorTab::MonitorTab(QWidget *parent)
    : QWidget(parent)
    , m_serialWorker(new SerialWorker)
    , m_workerThread(new QThread(this))
{
    // Move worker to dedicated thread so serial I/O never blocks the UI
    m_serialWorker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_serialWorker, &QObject::deleteLater);
    m_workerThread->start();

    setupUi();
}

MonitorTab::~MonitorTab()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

void MonitorTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);

    auto *placeholder = new QLabel(
        tr("UART Monitör\n\n"
           "Bu sekme COM port bağlantısı, terminal görünümü ve\n"
           "JSON metrik paketi ayrıştırma işlemlerini sağlar.\n"
           "(Aşama 3'te geliştirilecek)"),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName("placeholderLabel");

    layout->addWidget(placeholder);
    setLayout(layout);
}
