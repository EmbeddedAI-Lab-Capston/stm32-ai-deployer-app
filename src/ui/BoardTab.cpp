#include "BoardTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringList>
#include <QFileInfo>
#include <QProcess>
#include <QSerialPortInfo>

#include "core/AppSettings.h"
#include "modules/flash/FlashManager.h"
#include "modules/serial/SerialManager.h"

BoardTab::BoardTab(AppState *state, SerialManager *serial, QWidget *parent)
    : QWidget(parent)
    , m_appState(state)
    , m_serial(serial)
{
    setupUi();

    connect(m_appState, &AppState::activeBoardChanged,
            this, &BoardTab::onBoardChanged);

    connect(m_serial, &SerialManager::connectionChanged,
            this, &BoardTab::onSerialConnectionChanged);
    connect(m_serial, &SerialManager::bootReceived,
            this, &BoardTab::onSerialBootReceived);

    onBoardChanged(m_appState->activeBoard());
}

BoardTab::~BoardTab() = default;

void BoardTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // ── Bottom: info panel + sensor config ────────────────────────────────
    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->setSpacing(12);

    // ── Card info (read-only, driven by AppState) ─────────────────────────
    auto *infoBox  = new QGroupBox(tr("Aktif Kart Bilgileri"), this);
    auto *infoForm = new QFormLayout(infoBox);
    infoForm->setSpacing(10);
    infoForm->setLabelAlignment(Qt::AlignRight);

    auto makeVal = [&](QLabel *&lbl, const QString &text) -> QLabel * {
        lbl = new QLabel(text, this);
        lbl->setObjectName("infoValue");
        return lbl;
    };

    infoForm->addRow(tr("Model :"), makeVal(m_infoModel,  "--"));
    infoForm->addRow(tr("Flash :"), makeVal(m_infoFlash,  "--"));
    infoForm->addRow(tr("RAM   :"), makeVal(m_infoRam,    "--"));
    infoForm->addRow(tr("Hız   :"), makeVal(m_infoClock,  "--"));
    infoForm->addRow(tr("COM   :"), makeVal(m_infoPort, "--"));
    infoForm->addRow(tr("Board :"), makeVal(m_infoProbeBoard, "--"));
    infoForm->addRow(tr("Device ID :"), makeVal(m_infoDeviceId, "--"));
    infoForm->addRow(tr("Revision  :"), makeVal(m_infoRevision, "--"));
    infoForm->addRow(tr("Device    :"), makeVal(m_infoDeviceName, "--"));
    infoForm->addRow(tr("NVM       :"), makeVal(m_infoNvm, "--"));
    infoForm->addRow(tr("CPU       :"), makeVal(m_infoCpu, "--"));
    infoForm->addRow(tr("ST-LINK   :"), makeVal(m_infoStlink, "--"));
    infoForm->addRow(tr("Voltage   :"), makeVal(m_infoVoltage, "--"));

    m_infoStatus = new QLabel(tr("● Uyumlu"), this);
    m_infoStatus->setObjectName("statusOk");
    infoForm->addRow(tr("Durum :"), m_infoStatus);

    auto *noteLabel = new QLabel(
        tr("<i>Kartı değiştirmek için sol paneli kullanın.</i>"), this);
    noteLabel->setObjectName("sidebarValueLabel");
    infoForm->addRow(noteLabel);

    bottomLayout->addWidget(infoBox, 1);

    auto *selectBox = new QGroupBox(tr("Kart Seçimi ve Bağlantı"), this);
    auto *selectLayout = new QVBoxLayout(selectBox);
    selectLayout->setSpacing(10);

    auto *selectForm = new QFormLayout;
    m_boardCombo = new QComboBox(this);
    m_portCombo = new QComboBox(this);
    m_baudCombo = new QComboBox(this);
    selectForm->addRow(tr("Kart :"), m_boardCombo);
    selectForm->addRow(tr("COM  :"), m_portCombo);
    selectForm->addRow(tr("Baud :"), m_baudCombo);
    selectLayout->addLayout(selectForm);

    auto *buttonRow = new QHBoxLayout;
    m_connectBtn = new QPushButton(tr("Bağlan"), this);
    m_connectBtn->setObjectName("primaryButton");
    m_refreshBtn = new QPushButton(tr("Portları Yenile"), this);
    auto *boardCustomBtn = new QPushButton(tr("Özel Kart Ekle"), this);
    buttonRow->addWidget(m_connectBtn);
    buttonRow->addWidget(m_refreshBtn);
    buttonRow->addWidget(boardCustomBtn);
    selectLayout->addLayout(buttonRow);

    m_connStatusLabel = new QLabel(tr("● Bağlantı Yok"), this);
    m_connStatusLabel->setObjectName("sidebarConnLabel");
    selectLayout->addWidget(m_connStatusLabel);
    selectLayout->addStretch();
    bottomLayout->addWidget(selectBox, 1);

    // ── Sensor config ──────────────────────────────────────────────────────
    auto *sensorBox     = new QGroupBox(tr("Sensör Konfigürasyonu"), this);
    sensorBox->setVisible(false);
    auto *sensorVLayout = new QVBoxLayout(sensorBox);
    sensorVLayout->setSpacing(8);

    auto *sensorForm = new QFormLayout;
    m_sensorCombo = new QComboBox(this);
    m_sensorCombo->addItem(tr("Seçiniz..."));
    m_sensorCombo->addItem("MPU-6050  (I²C)");
    m_sensorCombo->addItem("BME280  (I²C)");
    m_sensorCombo->addItem("PDM Mikrofon");
    m_sensorCombo->addItem(tr("Özel"));
    sensorForm->addRow(tr("Sensör :"), m_sensorCombo);
    sensorVLayout->addLayout(sensorForm);

    // Dynamic pin fields — hidden until a sensor is selected
    m_pinWidget = new QWidget(this);
    auto *pinForm = new QFormLayout(m_pinWidget);
    pinForm->setContentsMargins(0, 4, 0, 0);
    pinForm->setSpacing(8);

    m_pin1Label = new QLabel("SDA Pin :", this);
    m_pin1Edit  = new QLineEdit(this);
    m_pin1Edit->setPlaceholderText("PB6");

    m_pin2Label = new QLabel("SCL Pin :", this);
    m_pin2Edit  = new QLineEdit(this);
    m_pin2Edit->setPlaceholderText("PB7");

    pinForm->addRow(m_pin1Label, m_pin1Edit);
    pinForm->addRow(m_pin2Label, m_pin2Edit);
    m_pinWidget->setVisible(false);
    sensorVLayout->addWidget(m_pinWidget);

    sensorVLayout->addStretch();

    auto *addCustomBtn  = new QPushButton(tr("＋  Özel Kart Ekle"), this);
    sensorVLayout->addWidget(addCustomBtn);

    auto *saveConfigBtn = new QPushButton(tr("Konfigürasyonu Kaydet"), this);
    saveConfigBtn->setObjectName("primaryButton");
    sensorVLayout->addWidget(saveConfigBtn);

    bottomLayout->addWidget(sensorBox, 1);
    mainLayout->addLayout(bottomLayout, 1);

    // ── Connections ────────────────────────────────────────────────────────
    connect(m_sensorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BoardTab::onSensorChanged);
    connect(addCustomBtn, &QPushButton::clicked,
            this, &BoardTab::onAddCustomBoardClicked);
    // TODO(asama-5): connect saveConfigBtn to Database::saveConfig
    connect(boardCustomBtn, &QPushButton::clicked,
            this, &BoardTab::onAddCustomBoardClicked);
    connect(m_boardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BoardTab::onBoardComboChanged);
    connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BoardTab::onPortComboChanged);
    connect(m_baudCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BoardTab::onBaudComboChanged);
    connect(m_connectBtn, &QPushButton::clicked,
            this, &BoardTab::onConnectClicked);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &BoardTab::onRefreshClicked);

    populateBoards();
    populatePorts();
    populateBauds();
    startStLinkBoardProbe();
}

void BoardTab::populateBoards()
{
    m_boardCombo->blockSignals(true);
    m_boardCombo->clear();

    for (const BoardInfo &b : BoardPresets::all())
        m_boardCombo->addItem(b.name, QVariant::fromValue(b));

    AppSettings settings;
    for (const BoardInfo &b : settings.customBoards())
        m_boardCombo->addItem(b.name, QVariant::fromValue(b));

    m_boardCombo->insertSeparator(m_boardCombo->count());
    m_boardCombo->addItem(tr("+ Özel Kart Ekle..."), QVariant());
    ensureBoardVisible(m_appState->activeBoard());
    m_boardCombo->blockSignals(false);
}

void BoardTab::populatePorts()
{
    m_portCombo->blockSignals(true);
    m_portCombo->clear();
    m_stlinkPortName.clear();

    const QSerialPortInfo stlink = SerialManager::findStLink();
    if (!stlink.isNull()) {
        m_stlinkPortName = stlink.portName();
        m_portCombo->addItem(QString("★ %1 (ST-Link)").arg(stlink.portName()),
                             stlink.portName());
    }

    for (const QSerialPortInfo &info : SerialManager::availablePorts()) {
        if (!stlink.isNull() && info.portName() == stlink.portName())
            continue;
        m_portCombo->addItem(info.portName(), info.portName());
    }

    if (m_portCombo->count() == 0)
        m_portCombo->addItem(tr("Port bulunamadı"), QString());

    m_appState->setActivePort(m_portCombo->currentData().toString());
    m_portCombo->blockSignals(false);
}

void BoardTab::populateBauds()
{
    m_baudCombo->blockSignals(true);
    m_baudCombo->clear();
    m_baudCombo->addItems({"9600", "38400", "115200", "230400", "921600"});
    m_baudCombo->setCurrentText(QString::number(m_appState->activeBaud()));
    m_baudCombo->blockSignals(false);
}

void BoardTab::ensureBoardVisible(const BoardInfo &board)
{
    if (board.isNull() || !m_boardCombo)
        return;

    for (int i = 0; i < m_boardCombo->count(); ++i) {
        const QVariant data = m_boardCombo->itemData(i);
        if (!data.isValid())
            continue;
        const BoardInfo existing = data.value<BoardInfo>();
        if (existing.name.compare(board.name, Qt::CaseInsensitive) == 0) {
            m_boardCombo->blockSignals(true);
            m_boardCombo->setItemData(i, QVariant::fromValue(board));
            m_boardCombo->setCurrentIndex(i);
            m_boardCombo->blockSignals(false);
            return;
        }
    }
}

// ── Slots ──────────────────────────────────────────────────────────────────

void BoardTab::onBoardChanged(const BoardInfo &board)
{
    if (board.isNull()) return;
    m_stlinkDetected = !board.probeBoardName.isEmpty()
        || !board.deviceName.isEmpty()
        || !board.deviceId.isEmpty();
    ensureBoardVisible(board);
    m_infoModel->setText(board.name);
    m_infoFlash->setText(QString::number(board.flashKb) + " KB");
    m_infoRam->setText(  QString::number(board.ramKb)   + " KB");
    m_infoClock->setText(QString::number(board.clockMhz)+ " MHz");
    const auto valueOrDash = [](const QString &value) {
        const QString trimmed = value.trimmed();
        return trimmed.isEmpty() ? QStringLiteral("--") : trimmed;
    };
    m_infoPort->setText(valueOrDash(board.portName));
    m_infoProbeBoard->setText(valueOrDash(board.probeBoardName));
    m_infoDeviceId->setText(valueOrDash(board.deviceId));
    m_infoRevision->setText(valueOrDash(board.revisionId));
    m_infoDeviceName->setText(valueOrDash(board.deviceName));
    m_infoNvm->setText(valueOrDash(board.nvmSize));
    m_infoCpu->setText(valueOrDash(board.deviceCpu));
    const QString stlink = QStringList{board.stlinkSn, board.stlinkFw}
        .filter(QRegularExpression("\\S"))
        .join(QStringLiteral(" / "));
    m_infoStlink->setText(valueOrDash(stlink));
    m_infoVoltage->setText(valueOrDash(board.voltage));
    updateStatusLabel();
}

void BoardTab::updateStatusLabel()
{
    QString text;
    QString objectName;

    if (m_stlinkDetected) {
        text       = tr("● ST-Link'ten Algılandı");
        objectName = "statusOk";
    } else if (!m_serialConnected) {
        text       = tr("● Manuel Seçim");
        objectName = "statusOk";
    } else if (m_bootDetected) {
        text       = tr("● UART'tan Algılandı");
        objectName = "statusOk";
    } else {
        text       = tr("● Kart Bilgisi Bekleniyor...");
        objectName = "statusWarn";
    }

    m_infoStatus->setText(text);
    m_infoStatus->setObjectName(objectName);
    m_infoStatus->style()->unpolish(m_infoStatus);
    m_infoStatus->style()->polish(m_infoStatus);
}

void BoardTab::onSerialConnectionChanged(bool connected, const QString &)
{
    m_serialConnected = connected;
    if (!connected)
        m_bootDetected = false;
    if (m_connectBtn)
        m_connectBtn->setText(connected ? tr("Bağlantıyı Kes") : tr("Bağlan"));
    if (m_connStatusLabel) {
        m_connStatusLabel->setText(connected ? tr("● Bağlı") : tr("● Bağlantı Yok"));
        m_connStatusLabel->setStyleSheet(connected
            ? QStringLiteral("color: #A6E3A1; font-weight: bold;")
            : QStringLiteral("color: #6C7086;"));
    }
    if (m_portCombo) m_portCombo->setEnabled(!connected);
    if (m_baudCombo) m_baudCombo->setEnabled(!connected);
    if (m_refreshBtn) m_refreshBtn->setEnabled(!connected);
    updateStatusLabel();
}

void BoardTab::onSerialBootReceived(const BootData &data)
{
    if (data.card.isEmpty())
        return;
    m_bootDetected = true;
    updateStatusLabel();
}

void BoardTab::onSensorChanged(int index)
{
    m_pinWidget->setVisible(index > 0);
    switch (index) {
    case 1: // MPU-6050
        m_pin1Label->setText("SDA Pin :"); m_pin1Edit->setPlaceholderText("PB6");
        m_pin2Label->setText("SCL Pin :"); m_pin2Edit->setPlaceholderText("PB7");
        break;
    case 2: // BME280
        m_pin1Label->setText("SDA Pin :"); m_pin1Edit->setPlaceholderText("PB6");
        m_pin2Label->setText("SCL Pin :"); m_pin2Edit->setPlaceholderText("PB7");
        break;
    case 3: // PDM Mic
        m_pin1Label->setText("DATA Pin :"); m_pin1Edit->setPlaceholderText("PA7");
        m_pin2Label->setText("CLK  Pin :"); m_pin2Edit->setPlaceholderText("PA5");
        break;
    case 4: // Custom
        m_pin1Label->setText("Pin 1 :"); m_pin1Edit->setPlaceholderText("PA0");
        m_pin2Label->setText("Pin 2 :"); m_pin2Edit->setPlaceholderText("PA1");
        break;
    default: break;
    }
}

void BoardTab::onBoardComboChanged(int index)
{
    const QVariant v = m_boardCombo->itemData(index);
    if (!v.isValid()) {
        onAddCustomBoardClicked();
        return;
    }
    m_appState->setActiveBoard(v.value<BoardInfo>());
}

void BoardTab::onPortComboChanged(int index)
{
    m_appState->setActivePort(m_portCombo->itemData(index).toString());
    startStLinkBoardProbe();
}

void BoardTab::onBaudComboChanged(int)
{
    m_appState->setActiveBaud(m_baudCombo->currentText().toInt());
}

void BoardTab::onConnectClicked()
{
    if (m_appState->isConnected()) {
        m_serial->disconnectPort();
        return;
    }

    const QString port = m_appState->activePort();
    if (port.isEmpty()) {
        m_connStatusLabel->setText(tr("⚠ Port seçilmedi"));
        return;
    }

    startStLinkBoardProbe();
    m_serial->connectToPort(port, m_appState->activeBaud());
}

void BoardTab::onRefreshClicked()
{
    populatePorts();
    startStLinkBoardProbe();
}

void BoardTab::startStLinkBoardProbe()
{
    if (m_stlinkPortName.isEmpty())
        return;
    if (m_appState->activePort() != m_stlinkPortName)
        return;

    AppSettings settings;
    QString cliPath = settings.programmerCliPath();
    if (cliPath.isEmpty()) {
        cliPath = FlashManager::detectCliPath();
        if (!cliPath.isEmpty())
            settings.setProgrammerCliPath(cliPath);
    }
    if (cliPath.isEmpty())
        return;

    const QFileInfo cliInfo(cliPath);
    if (cliInfo.isAbsolute() && !cliInfo.exists())
        return;

    if (m_stlinkProbe) {
        m_stlinkProbe->kill();
        m_stlinkProbe->deleteLater();
    }

    m_stlinkProbe = new QProcess(this);
    connect(m_stlinkProbe, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                onStLinkProbeFinished(exitCode);
            });
    m_stlinkProbe->start(cliPath, {QStringLiteral("-c"), QStringLiteral("port=SWD")});
}

void BoardTab::onStLinkProbeFinished(int exitCode)
{
    if (!m_stlinkProbe)
        return;

    const QString output = QString::fromLocal8Bit(m_stlinkProbe->readAllStandardOutput()
                                                  + m_stlinkProbe->readAllStandardError());
    m_stlinkProbe->deleteLater();
    m_stlinkProbe = nullptr;

    if (exitCode != 0 || output.trimmed().isEmpty())
        return;

    applyDetectedStLinkBoard(output);
}

void BoardTab::applyDetectedStLinkBoard(const QString &probeOutput)
{
    QString boardName;
    QString deviceName;
    QString deviceId;
    QString revisionId;
    QString nvmSize;
    QString cpuName;
    QString stlinkSn;
    QString stlinkFw;
    QString voltage;

    for (const QString &line : probeOutput.split(QRegularExpression("[\\r\\n]+"),
                                                 Qt::SkipEmptyParts)) {
        const QString trimmed = line.trimmed();
        const int sep = trimmed.indexOf(':');
        if (sep < 0)
            continue;

        const QString key = trimmed.left(sep).trimmed();
        const QString value = trimmed.mid(sep + 1).trimmed();
        if (key.compare(QStringLiteral("Board"), Qt::CaseInsensitive) == 0)
            boardName = value;
        else if (key.compare(QStringLiteral("ST-LINK SN"), Qt::CaseInsensitive) == 0)
            stlinkSn = value;
        else if (key.compare(QStringLiteral("ST-LINK FW"), Qt::CaseInsensitive) == 0)
            stlinkFw = value;
        else if (key.compare(QStringLiteral("Voltage"), Qt::CaseInsensitive) == 0)
            voltage = value;
        else if (key.compare(QStringLiteral("Device name"), Qt::CaseInsensitive) == 0)
            deviceName = value;
        else if (key.compare(QStringLiteral("Device ID"), Qt::CaseInsensitive) == 0)
            deviceId = value;
        else if (key.compare(QStringLiteral("Revision ID"), Qt::CaseInsensitive) == 0)
            revisionId = value;
        else if (key.compare(QStringLiteral("NVM size"), Qt::CaseInsensitive) == 0)
            nvmSize = value;
        else if (key.compare(QStringLiteral("Device CPU"), Qt::CaseInsensitive) == 0)
            cpuName = value;
    }

    const QString lookup = QStringList{boardName, deviceName, deviceId, cpuName}
        .join(QStringLiteral(" "));
    BoardInfo board = BoardPresets::find(lookup);
    if (board.isNull() && !boardName.isEmpty())
        board = BoardPresets::find(boardName);
    if (board.isNull() && !deviceName.isEmpty())
        board = BoardPresets::find(deviceName);
    if (board.isNull())
        return;

    board.portName = m_stlinkPortName;
    board.probeBoardName = boardName;
    board.deviceId = deviceId;
    board.revisionId = revisionId;
    board.deviceName = deviceName;
    board.nvmSize = nvmSize;
    board.deviceCpu = cpuName;
    board.stlinkSn = stlinkSn;
    board.stlinkFw = stlinkFw;
    board.voltage = voltage;

    ensureBoardVisible(board);
    m_appState->setActiveBoard(board);
}

void BoardTab::onAddCustomBoardClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Özel Kart Ekle"));
    dlg.setMinimumWidth(360);

    auto *layout    = new QVBoxLayout(&dlg);
    auto *form      = new QFormLayout;
    auto *nameEdit  = new QLineEdit(&dlg);
    auto *flashSpin = new QSpinBox(&dlg);
    auto *ramSpin   = new QSpinBox(&dlg);
    auto *clockSpin = new QSpinBox(&dlg);

    nameEdit->setPlaceholderText("örn. STM32L4");
    flashSpin->setRange(1, 32768); flashSpin->setSuffix(" KB");  flashSpin->setValue(512);
    ramSpin->setRange(1, 16384);   ramSpin->setSuffix(" KB");    ramSpin->setValue(128);
    clockSpin->setRange(1, 1000);  clockSpin->setSuffix(" MHz"); clockSpin->setValue(80);

    form->addRow(tr("Kart Adı  :"), nameEdit);
    form->addRow(tr("Flash     :"), flashSpin);
    form->addRow(tr("RAM       :"), ramSpin);
    form->addRow(tr("Saat Hızı :"), clockSpin);
    layout->addLayout(form);

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Özel Kart"), tr("Kart adı boş olamaz."));
        return;
    }

    BoardInfo custom;
    custom.name     = name;
    custom.flashKb  = flashSpin->value();
    custom.ramKb    = ramSpin->value();
    custom.clockMhz = clockSpin->value();
    custom.isPreset = false;

    AppSettings settings;
    settings.addCustomBoard(custom);
    populateBoards();
    m_appState->setActiveBoard(custom);

    QMessageBox::information(this, tr("Kart Eklendi"),
        tr("'%1' aktif kart olarak ayarlandı.\n"
           "Sol paneldeki kart listesine de eklendi.")
        .arg(custom.name));
}
