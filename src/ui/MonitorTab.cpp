#include "MonitorTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QThread>
#include <QFont>

#include "modules/serial/SerialWorker.h"

MonitorTab::MonitorTab(QWidget *parent)
    : QWidget(parent)
    , m_serialWorker(new SerialWorker)
    , m_workerThread(new QThread(this))
{
    // Move worker to dedicated thread — serial I/O never blocks the UI
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
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ── Connection panel ───────────────────────────────────────────────────
    auto *connBox    = new QGroupBox(tr("Bağlantı"), this);
    auto *connLayout = new QHBoxLayout(connBox);
    connLayout->setSpacing(8);

    connLayout->addWidget(new QLabel(tr("Port:"), this));
    m_portCombo = new QComboBox(this);
    for (int i = 1; i <= 20; ++i)
        m_portCombo->addItem(QString("COM%1").arg(i));
    m_portCombo->setCurrentText("COM3");
    m_portCombo->setFixedWidth(90);
    connLayout->addWidget(m_portCombo);

    auto *refreshBtn = new QPushButton("⟳", this);
    refreshBtn->setFixedSize(32, 32);
    refreshBtn->setToolTip(tr("Portları yenile"));
    connLayout->addWidget(refreshBtn);

    connLayout->addSpacing(12);
    connLayout->addWidget(new QLabel(tr("Baud:"), this));

    m_baudCombo = new QComboBox(this);
    m_baudCombo->addItems({"9600", "38400", "115200", "230400", "921600"});
    m_baudCombo->setCurrentText("115200");
    m_baudCombo->setFixedWidth(100);
    connLayout->addWidget(m_baudCombo);

    connLayout->addSpacing(16);

    m_connectBtn = new QPushButton(tr("Bağlan"), this);
    m_connectBtn->setObjectName("primaryButton");
    m_connectBtn->setFixedWidth(120);
    connLayout->addWidget(m_connectBtn);

    m_connStatus = new QLabel(tr("  ● Bağlantı Yok"), this);
    m_connStatus->setObjectName("connStatusDisconnected");
    connLayout->addWidget(m_connStatus);

    connLayout->addStretch();
    mainLayout->addWidget(connBox);

    // ── Splitter: terminal (left) | metrics (right) ────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(5);
    splitter->setObjectName("monitorSplitter");

    // Terminal side
    auto *termBox    = new QGroupBox(tr("Terminal"), splitter);
    auto *termLayout = new QVBoxLayout(termBox);

    m_terminal = new QTextEdit(termBox);
    m_terminal->setReadOnly(true);
    m_terminal->setObjectName("terminalOutput");

    QFont monoFont("Consolas", 10);
    m_terminal->setFont(monoFont);

    // Placeholder sample lines demonstrating § protocol
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    const auto append = [&](const QString &color, const QString &text) {
        m_terminal->append(
            QString("<span style='color:#6C7086;'>[%1]</span> "
                    "<span style='color:%2;'>%3</span>").arg(ts, color, text));
    };
    append("#A6E3A1", "Sistem hazır — COM port bağlantısı bekleniyor...");
    append("#89B4FA", "§{\"t\":\"boot\",\"card\":\"STM32F4\",\"sdk\":\"EdgeAI_v1.0\","
                      "\"model\":\"MLP_INT8\",\"baud\":115200}");
    append("#89B4FA", "§{\"t\":\"inf\",\"model\":\"MLP_INT8\",\"inf_us\":8200,"
                      "\"ram_b\":3072,\"acc_pct\":96,\"label\":\"walking\",\"card\":\"STM32F4\"}");
    append("#89B4FA", "§{\"t\":\"sys\",\"uptime_s\":42,\"temp_c\":38,"
                      "\"free_ram_b\":185000,\"state\":\"running\"}");
    append("#F38BA8", "§{\"t\":\"err\",\"code\":3,\"msg\":\"sensor_timeout\"}");

    termLayout->addWidget(m_terminal);
    splitter->addWidget(termBox);

    // Metrics side
    auto *metricsWidget = new QWidget(splitter);
    auto *metricsLayout = new QVBoxLayout(metricsWidget);
    metricsLayout->setContentsMargins(0, 0, 0, 0);
    metricsLayout->setSpacing(8);

    auto makeCard = [&](const QString &title, QLabel *&valueOut) -> QGroupBox * {
        auto *card  = new QGroupBox(title, metricsWidget);
        card->setObjectName("metricCard");
        auto *vl    = new QVBoxLayout(card);
        vl->setAlignment(Qt::AlignCenter);

        valueOut = new QLabel("--", card);
        valueOut->setObjectName("metricValue");
        valueOut->setAlignment(Qt::AlignCenter);

        QFont f = valueOut->font();
        f.setPointSize(20);
        f.setBold(true);
        valueOut->setFont(f);

        vl->addWidget(valueOut);
        return card;
    };

    metricsLayout->addWidget(makeCard(tr("Inference (ms)"),  m_metricInfValue));
    metricsLayout->addWidget(makeCard(tr("RAM Kullanımı"),    m_metricRamValue));
    metricsLayout->addWidget(makeCard(tr("Son Tahmin"),       m_metricLabelValue));
    metricsLayout->addStretch();
    splitter->addWidget(metricsWidget);

    splitter->setSizes({700, 280});
    mainLayout->addWidget(splitter, 1);

    // ── Bottom bar ─────────────────────────────────────────────────────────
    auto *bottomRow    = new QWidget(this);
    auto *bottomLayout = new QHBoxLayout(bottomRow);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(8);

    auto *clearBtn = new QPushButton(tr("Temizle"), this);
    clearBtn->setFixedWidth(90);
    auto *saveBtn = new QPushButton(tr("Kaydet"), this);
    saveBtn->setFixedWidth(90);

    bottomLayout->addWidget(clearBtn);
    bottomLayout->addWidget(saveBtn);
    bottomLayout->addStretch();

    m_bottomInfoLabel = new QLabel(tr("Kart: --    Uptime: -- s"), this);
    m_bottomInfoLabel->setObjectName("sidebarValueLabel");
    bottomLayout->addWidget(m_bottomInfoLabel);

    mainLayout->addWidget(bottomRow);

    // ── Connections ────────────────────────────────────────────────────────
    connect(m_connectBtn, &QPushButton::clicked, this, &MonitorTab::onConnectClicked);
    connect(clearBtn,     &QPushButton::clicked, this, &MonitorTab::onClearClicked);
    connect(saveBtn,      &QPushButton::clicked, this, &MonitorTab::onSaveClicked);
    // TODO(asama-3): connect SerialWorker signals → terminal/metric update slots
}

void MonitorTab::onConnectClicked()
{
    // TODO(asama-3): replace with SerialWorker::connectPort / disconnectPort
    m_connected = !m_connected;
    if (m_connected) {
        m_connectBtn->setText(tr("Bağlantıyı Kes"));
        m_connStatus->setText(tr("  ● Bağlandı"));
        m_connStatus->setObjectName("connStatusConnected");
    } else {
        m_connectBtn->setText(tr("Bağlan"));
        m_connStatus->setText(tr("  ● Bağlantı Yok"));
        m_connStatus->setObjectName("connStatusDisconnected");
    }
    m_connStatus->style()->unpolish(m_connStatus);
    m_connStatus->style()->polish(m_connStatus);
}

void MonitorTab::onClearClicked()
{
    m_terminal->clear();
}

void MonitorTab::onSaveClicked()
{
    // TODO(asama-3): save only received data, not placeholder text
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Terminali Kaydet"), {},
        tr("Text Files (*.txt);;All Files (*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QFile::WriteOnly | QFile::Text)) {
        QTextStream stream(&file);
        stream << m_terminal->toPlainText();
    }
}
