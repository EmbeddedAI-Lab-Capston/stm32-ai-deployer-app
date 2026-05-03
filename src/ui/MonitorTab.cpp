#include "MonitorTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <QCheckBox>
#include <QScrollBar>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QFont>

#include "modules/serial/SerialManager.h"
#include "modules/serial/SerialSimulator.h"

MonitorTab::MonitorTab(QWidget *parent)
    : QWidget(parent)
    , m_serialManager(new SerialManager(this))
    , m_simParser(new PacketParser(this))
    , m_simulator(new SerialSimulator(m_simParser, this))
{
    setupUi();

    // --- SerialManager → UI connections ---
    connect(m_serialManager, &SerialManager::connectionChanged,
            this, &MonitorTab::onConnectionChanged);
    connect(m_serialManager, &SerialManager::rawLineReceived,
            this, &MonitorTab::appendRawLog);
    connect(m_serialManager, &SerialManager::inferenceReceived,
            this, &MonitorTab::onInferenceReceived);
    connect(m_serialManager, &SerialManager::sysReceived,
            this, &MonitorTab::onSysReceived);
    connect(m_serialManager, &SerialManager::errorOccurred,
            this, &MonitorTab::onSerialError);

    // --- Simulator parser → same UI slots ---
    connect(m_simParser, &PacketParser::rawLineReceived,
            this, &MonitorTab::appendRawLog);
    connect(m_simParser, &PacketParser::inferenceReceived,
            this, &MonitorTab::onInferenceReceived);
    connect(m_simParser, &PacketParser::sysReceived,
            this, &MonitorTab::onSysReceived);

    refreshPorts();
}

MonitorTab::~MonitorTab() = default;

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
    m_portCombo->setFixedWidth(140);
    connLayout->addWidget(m_portCombo);

    auto *refreshBtn = new QPushButton("⟳", this);
    refreshBtn->setFixedSize(32, 32);
    refreshBtn->setToolTip(tr("Port listesini yenile"));
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
    m_connectBtn->setFixedWidth(130);
    connLayout->addWidget(m_connectBtn);

    m_connStatus = new QLabel(tr("  ● Bağlantı Yok"), this);
    m_connStatus->setObjectName("connStatusDisconnected");
    connLayout->addWidget(m_connStatus);

    connLayout->addStretch();

    m_simCheck = new QCheckBox(tr("Simülasyon Modu"), this);
    connLayout->addWidget(m_simCheck);

    mainLayout->addWidget(connBox);

    // ── Splitter: terminal (left) | metrics (right) ────────────────────────
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(5);

    // Terminal side
    auto *termBox    = new QGroupBox(tr("Terminal"), splitter);
    auto *termLayout = new QVBoxLayout(termBox);

    m_terminal = new QTextEdit(termBox);
    m_terminal->setReadOnly(true);
    m_terminal->setObjectName("terminalOutput");
    m_terminal->setFont(QFont("Consolas", 10));
    termLayout->addWidget(m_terminal);
    splitter->addWidget(termBox);

    // Metrics side
    auto *metricsWidget = new QWidget(splitter);
    auto *metricsLayout = new QVBoxLayout(metricsWidget);
    metricsLayout->setContentsMargins(0, 0, 0, 0);
    metricsLayout->setSpacing(8);

    auto makeCard = [&](const QString &title, QLabel *&valueOut) -> QGroupBox * {
        auto *card = new QGroupBox(title, metricsWidget);
        card->setObjectName("metricCard");
        auto *vl = new QVBoxLayout(card);
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

    metricsLayout->addWidget(makeCard(tr("Inference (ms)"), m_metricInfValue));
    metricsLayout->addWidget(makeCard(tr("RAM Kullanımı"),   m_metricRamValue));
    metricsLayout->addWidget(makeCard(tr("Son Tahmin"),      m_metricLabelValue));
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

    m_bottomInfoLabel = new QLabel(tr("Kart: --    Uptime: -- s    MCU: -- °C"), this);
    m_bottomInfoLabel->setObjectName("sidebarValueLabel");
    bottomLayout->addWidget(m_bottomInfoLabel);

    mainLayout->addWidget(bottomRow);

    // ── Button connections ─────────────────────────────────────────────────
    connect(m_connectBtn, &QPushButton::clicked, this, &MonitorTab::onConnectClicked);
    connect(refreshBtn,   &QPushButton::clicked, this, &MonitorTab::onRefreshPortsClicked);
    connect(clearBtn,     &QPushButton::clicked, this, &MonitorTab::onClearClicked);
    connect(saveBtn,      &QPushButton::clicked, this, &MonitorTab::onSaveClicked);
    connect(m_simCheck,   &QCheckBox::toggled,   this, &MonitorTab::onSimulationToggled);
}

void MonitorTab::refreshPorts()
{
    m_portCombo->clear();

    QSerialPortInfo stlink = SerialManager::findStLink();
    if (!stlink.isNull()) {
        m_portCombo->addItem(
            QString("★ %1 (ST-Link)").arg(stlink.portName()),
            stlink.portName());
    }

    for (const QSerialPortInfo &info : SerialManager::availablePorts()) {
        if (!stlink.isNull() && info.portName() == stlink.portName())
            continue;
        m_portCombo->addItem(info.portName(), info.portName());
    }

    if (m_portCombo->count() == 0)
        m_portCombo->addItem(tr("Port bulunamadı"), QString());
}

void MonitorTab::appendHtmlLine(const QString &color, const QString &text)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_terminal->append(
        QString("<span style='color:#6C7086;'>[%1]</span> "
                "<span style='color:%2;'>%3</span>")
            .arg(ts, color, text.toHtmlEscaped()));

    QScrollBar *sb = m_terminal->verticalScrollBar();
    sb->setValue(sb->maximum());
}

// ── Slots ──────────────────────────────────────────────────────────────────

void MonitorTab::onConnectClicked()
{
    if (m_simCheck->isChecked()) return; // simulation mode manages its own state

    if (m_connected) {
        m_serialManager->disconnectPort();
    } else {
        const QString port = m_portCombo->currentData().toString();
        const qint32  baud = m_baudCombo->currentText().toInt();
        if (port.isEmpty()) return;
        m_serialManager->connectToPort(port, baud);
    }
}

void MonitorTab::onRefreshPortsClicked()
{
    refreshPorts();
}

void MonitorTab::onClearClicked()
{
    m_terminal->clear();
    m_metricInfValue->setText("--");
    m_metricRamValue->setText("--");
    m_metricLabelValue->setText("--");
    m_infBuffer.clear();
}

void MonitorTab::onSaveClicked()
{
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

void MonitorTab::onSimulationToggled(bool checked)
{
    if (checked) {
        m_connectBtn->setEnabled(false);
        m_portCombo->setEnabled(false);
        m_baudCombo->setEnabled(false);

        onConnectionChanged(true, "Simülasyon @ 115200");
        m_simulator->start(800);
    } else {
        m_simulator->stop();
        m_connectBtn->setEnabled(true);
        m_portCombo->setEnabled(true);
        m_baudCombo->setEnabled(true);

        onConnectionChanged(false, QString());
        onClearClicked();
    }
}

void MonitorTab::onConnectionChanged(bool connected, const QString &info)
{
    m_connected = connected;

    if (connected) {
        m_connectBtn->setText(tr("Bağlantıyı Kes"));
        m_connStatus->setText(tr("  ● ") + info);
        m_connStatus->setObjectName("connStatusConnected");
        appendHtmlLine("#A6E3A1", tr("Bağlantı kuruldu: ") + info);
    } else {
        m_connectBtn->setText(tr("Bağlan"));
        m_connStatus->setText(tr("  ● Bağlantı Yok"));
        m_connStatus->setObjectName("connStatusDisconnected");
        if (!info.isEmpty())
            appendHtmlLine("#6C7086", tr("Bağlantı kesildi."));
    }
    m_connStatus->style()->unpolish(m_connStatus);
    m_connStatus->style()->polish(m_connStatus);

    emit connectionStatusChanged(connected, info);
}

void MonitorTab::appendRawLog(const QString &line)
{
    appendHtmlLine("#CDD6F4", line);
}

void MonitorTab::onInferenceReceived(const InferenceData &data)
{
    const double ms    = data.inf_us / 1000.0;
    const double ramKb = data.ram_b  / 1024.0;

    m_metricInfValue->setText(
        QString("%1 ms").arg(ms, 0, 'f', 1));
    m_metricRamValue->setText(
        QString("%1 KB").arg(ramKb, 0, 'f', 1));
    m_metricLabelValue->setText(
        QString("%1\n%2%").arg(data.label).arg(data.acc_pct));

    appendHtmlLine("#89B4FA",
        QString("§ inf  %1  %2 ms  %3 KB  acc=%4%  label=%5")
            .arg(data.model)
            .arg(ms, 0, 'f', 1)
            .arg(ramKb, 0, 'f', 1)
            .arg(data.acc_pct)
            .arg(data.label));

    m_infBuffer.push(data);

    // TODO(asama-5): SQLite'a yaz
    // TODO(asama-6): Grafik güncelle

    emit inferenceMetricUpdated(data.model, ms, data.acc_pct);
}

void MonitorTab::onSysReceived(const SysData &data)
{
    m_bottomInfoLabel->setText(
        QString("Kart: %1    Uptime: %2 s    MCU: %3 °C")
            .arg(data.state.isEmpty() ? "--" : "STM32")
            .arg(data.uptime_s)
            .arg(data.temp_c));

    appendHtmlLine("#A6ADC8",
        QString("§ sys  uptime=%1s  temp=%2°C  free=%3 KB  state=%4")
            .arg(data.uptime_s)
            .arg(data.temp_c)
            .arg(data.free_ram_b / 1024)
            .arg(data.state));

    // TODO(asama-6): Sistem grafiğini güncelle
}

void MonitorTab::onSerialError(const QString &msg)
{
    appendHtmlLine("#F38BA8", tr("HATA: ") + msg);

    // Port disconnected — reset UI state
    onConnectionChanged(false, QString());
}
