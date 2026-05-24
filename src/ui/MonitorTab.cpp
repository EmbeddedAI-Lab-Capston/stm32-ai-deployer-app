#include "MonitorTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QScrollBar>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QSpinBox>
#include <QTimer>

#include "core/AppState.h"
#include "modules/serial/SerialManager.h"

MonitorTab::MonitorTab(AppState *state, SerialManager *serial, QWidget *parent)
    : QWidget(parent)
    , m_appState(state)
    , m_serialManager(serial)
    , m_simTimer(new QTimer(this))
{
    setupUi();

    // ── SerialManager → UI ────────────────────────────────────────────────
    connect(m_serialManager, &SerialManager::connectionChanged,
            this, &MonitorTab::onConnectionChanged);
    connect(m_serialManager, &SerialManager::rawLineReceived,
            this, &MonitorTab::appendRawLog);
    connect(m_serialManager, &SerialManager::inferenceReceived,
            this, &MonitorTab::onInferenceReceived);
    connect(m_serialManager, &SerialManager::bootReceived,
            this, &MonitorTab::onBootReceived);
    connect(m_serialManager, &SerialManager::sysReceived,
            this, &MonitorTab::onSysReceived);
    connect(m_serialManager, &SerialManager::errorOccurred,
            this, &MonitorTab::onSerialError);

    connect(m_simTimer, &QTimer::timeout,
            this, &MonitorTab::sendSimulatedSensorFrame);

    // ── AppState reactions ────────────────────────────────────────────────
    connect(m_appState, &AppState::connectionChanged,
            this, &MonitorTab::onConnectionChanged);
    connect(m_appState, &AppState::activeBoardChanged,
            this, &MonitorTab::onBoardChanged);
    connect(m_appState, &AppState::lastModelChanged,
            this, [this](const QString &name, double infMs, quint8 acc) {
                // Update sidebar through AppState — already handled there.
                // Here we can update bottom info if desired.
                Q_UNUSED(name); Q_UNUSED(infMs); Q_UNUSED(acc);
            });

    // Show initial state
    onConnectionChanged(m_appState->isConnected(), m_appState->connectionInfo());
    onBoardChanged(m_appState->activeBoard());
}

MonitorTab::~MonitorTab() = default;

void MonitorTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ── Readonly status bar ────────────────────────────────────────────────
    auto *statusFrame  = new QFrame(this);
    statusFrame->setObjectName("monitorStatusBar");
    statusFrame->setFrameShape(QFrame::StyledPanel);
    auto *statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(8, 4, 8, 4);
    statusLayout->setSpacing(16);

    m_statusBar = new QLabel(tr("Bağlantı: ● Bağlantı Yok    Kart: --"), this);
    m_statusBar->setObjectName("sidebarValueLabel");
    statusLayout->addWidget(m_statusBar, 1);

    m_simCheck = new QCheckBox(tr("Simülasyon Modu"), this);
    statusLayout->addWidget(m_simCheck);

    m_simIntervalSpin = new QSpinBox(this);
    m_simIntervalSpin->setRange(50, 5000);
    m_simIntervalSpin->setSingleStep(50);
    m_simIntervalSpin->setValue(500);
    m_simIntervalSpin->setSuffix(" ms");
    m_simIntervalSpin->setEnabled(false);
    statusLayout->addWidget(new QLabel(tr("Periyot:"), this));
    statusLayout->addWidget(m_simIntervalSpin);

    m_simMinSpin = new QDoubleSpinBox(this);
    m_simMinSpin->setRange(-100000.0, 100000.0);
    m_simMinSpin->setDecimals(3);
    m_simMinSpin->setValue(0.0);
    m_simMinSpin->setEnabled(false);
    m_simMaxSpin = new QDoubleSpinBox(this);
    m_simMaxSpin->setRange(-100000.0, 100000.0);
    m_simMaxSpin->setDecimals(3);
    m_simMaxSpin->setValue(1.0);
    m_simMaxSpin->setEnabled(false);
    statusLayout->addWidget(new QLabel(tr("Aralık:"), this));
    statusLayout->addWidget(m_simMinSpin);
    statusLayout->addWidget(m_simMaxSpin);

    mainLayout->addWidget(statusFrame);

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
    metricsLayout->addWidget(makeCard(tr("RAM Kullanımı"),  m_metricRamValue));
    metricsLayout->addWidget(makeCard(tr("Son Tahmin"),     m_metricLabelValue));
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
    connect(clearBtn,   &QPushButton::clicked, this, &MonitorTab::onClearClicked);
    connect(saveBtn,    &QPushButton::clicked, this, &MonitorTab::onSaveClicked);
    connect(m_simCheck, &QCheckBox::toggled,   this, &MonitorTab::onSimulationToggled);
}

// ── Helpers ────────────────────────────────────────────────────────────────

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
        startHardwareSimulation();
    } else {
        stopHardwareSimulation();
    }
}

void MonitorTab::startHardwareSimulation()
{
    const QString port = m_appState->activePort();
    if (port.isEmpty()) {
        appendHtmlLine("#F38BA8", tr("Simülasyon için aktif COM port seçili değil."));
        m_simCheck->setChecked(false);
        return;
    }

    m_simIntervalSpin->setEnabled(true);
    m_simMinSpin->setEnabled(true);
    m_simMaxSpin->setEnabled(true);
    m_simSeed = 1234;
    m_simSentCount = 0;
    m_simResponseCount = 0;
    m_simTotalInfUs = 0;
    m_simTotalRamB = 0;
    m_simTotalAcc = 0;
    m_simLastInference = {};

    appendHtmlLine("#F9E2AF",
        tr("Donanım simülasyonu başlatılıyor: sentetik sensör verileri karta gönderilecek."));

    if (!m_appState->isConnected()) {
        m_serialManager->connectToPort(port, m_appState->activeBaud());
        QTimer::singleShot(1200, this, [this]() {
            if (m_simCheck->isChecked()) {
                m_serialManager->writeLine("BOOT?");
                sendSimulatedSensorFrame();
                m_simTimer->start(m_simIntervalSpin->value());
            }
        });
    } else {
        m_serialManager->writeLine("BOOT?");
        sendSimulatedSensorFrame();
        m_simTimer->start(m_simIntervalSpin->value());
    }
}

void MonitorTab::stopHardwareSimulation()
{
    m_simTimer->stop();
    m_simIntervalSpin->setEnabled(false);
    m_simMinSpin->setEnabled(false);
    m_simMaxSpin->setEnabled(false);

    if (m_simResponseCount > 0) {
        InferenceData summary = m_simLastInference;
        summary.inf_us = static_cast<quint32>(m_simTotalInfUs / m_simResponseCount);
        summary.ram_b = static_cast<quint32>(m_simTotalRamB / m_simResponseCount);
        summary.acc_pct = static_cast<quint8>(m_simTotalAcc / m_simResponseCount);
        emit simulationSessionFinished(summary, m_simResponseCount);
    }

    appendHtmlLine("#6C7086", tr("Donanım simülasyonu durduruldu."));
}

void MonitorTab::sendSimulatedSensorFrame()
{
    if (!m_appState->isConnected()) {
        m_simTimer->stop();
        m_simCheck->setChecked(false);
        appendHtmlLine("#F38BA8", tr("UART bağlantısı yok; simülasyon durduruldu."));
        return;
    }

    if (m_simTimer->interval() != m_simIntervalSpin->value())
        m_simTimer->setInterval(m_simIntervalSpin->value());

    const double minValue = m_simMinSpin->value();
    const double maxValue = m_simMaxSpin->value();
    const double lo = qMin(minValue, maxValue);
    const double hi = qMax(minValue, maxValue);

    QStringList values;
    QStringList displayValues;
    for (int i = 0; i < 3; ++i) {
        m_simSeed = (m_simSeed * 1664525u) + 1013904223u;
        const double unit = static_cast<double>(m_simSeed & 0x00FFFFFFu) / 16777215.0;
        const double value = lo + unit * (hi - lo);
        const int milli = qRound(value * 1000.0);
        values << QString::number(milli);
        displayValues << QString::number(value, 'f', 3);
    }

    const QByteArray command = QString("INFER %1").arg(values.join(' ')).toUtf8();
    ++m_simSentCount;
    appendHtmlLine("#F9E2AF",
        QString("sim sensor  x1=%1  x2=%2  x3=%3")
            .arg(displayValues.value(0), displayValues.value(1), displayValues.value(2)));
    m_serialManager->writeLine(command);

    if (m_simSentCount >= 6 && m_simResponseCount == 0) {
        appendHtmlLine("#F38BA8",
            tr("Karttan INFER cevabı gelmedi. Pipeline Wizard ile firmware'i yeniden üretip flash ettiğinizden emin olun."));
        m_simSentCount = 0;
    }
}

void MonitorTab::onConnectionChanged(bool connected, const QString &info)
{
    // Update status bar label
    const QString board = m_appState->activeBoard().name;
    if (connected) {
        m_statusBar->setText(
            QString("Bağlantı: ● %1    Kart: %2")
                .arg(info.isEmpty() ? "Bağlı" : info, board));
        appendHtmlLine("#A6E3A1", tr("Bağlantı kuruldu: ") + info);
    } else {
        m_statusBar->setText(
            QString("Bağlantı: ● Bağlantı Yok    Kart: %1").arg(board));
        if (!info.isEmpty())
            appendHtmlLine("#6C7086", tr("Bağlantı kesildi."));
    }
}

void MonitorTab::onBoardChanged(const BoardInfo &board)
{
    // Refresh status bar with new board name
    const bool connected = m_appState->isConnected();
    const QString info   = m_appState->connectionInfo();
    onConnectionChanged(connected, info);
    Q_UNUSED(board)
}

void MonitorTab::appendRawLog(const QString &line)
{
    appendHtmlLine("#CDD6F4", line);
}

void MonitorTab::onBootReceived(const BootData &data)
{
    appendHtmlLine("#A6E3A1",
        QString("§ boot  card=%1  sdk=%2  model=%3  baud=%4")
            .arg(data.card.isEmpty() ? "--" : data.card)
            .arg(data.sdk.isEmpty() ? "--" : data.sdk)
            .arg(data.model.isEmpty() ? "--" : data.model)
            .arg(data.baud));
    if (!data.sensor.isEmpty())
        appendHtmlLine("#A6E3A1", QString("sensor firmware=%1").arg(data.sensor));
}

void MonitorTab::onInferenceReceived(const InferenceData &data)
{
    if (m_simCheck && m_simCheck->isChecked()) {
        ++m_simResponseCount;
        m_simTotalInfUs += data.inf_us;
        m_simTotalRamB += data.ram_b;
        m_simTotalAcc += data.acc_pct;
        m_simLastInference = data;
    }

    const double ms    = data.inf_us / 1000.0;
    const double ramKb = data.ram_b  / 1024.0;

    m_metricInfValue->setText(QString("%1 ms").arg(ms, 0, 'f', 1));
    m_metricRamValue->setText(QString("%1 KB").arg(ramKb, 0, 'f', 1));
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

    // Push to AppState so Sidebar's "Son Model" section updates
    m_appState->setLastModel(data.model, ms, data.acc_pct);

    // TODO(asama-5): SQLite'a yaz
    // TODO(asama-6): Grafik güncelle
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
    onConnectionChanged(false, QString());
}
