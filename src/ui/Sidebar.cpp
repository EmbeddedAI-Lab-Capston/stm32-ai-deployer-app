#include "Sidebar.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFrame>
#include <QSerialPortInfo>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#include "core/AppSettings.h"
#include "modules/flash/FlashManager.h"
#include "modules/serial/SerialManager.h"

// ── helpers ───────────────────────────────────────────────────────────────
static QFrame *makeSep(QWidget *parent)
{
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setObjectName("sidebarSeparator");
    return f;
}

static QLabel *makeSectionLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName("sidebarSectionLabel");
    return l;
}

static QLabel *makeValueLabel(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName("sidebarValueLabel");
    return l;
}

// ── Sidebar ───────────────────────────────────────────────────────────────
Sidebar::Sidebar(AppState *state, SerialManager *serial, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_serial(serial)
{
    setObjectName("sidebar");
    setFixedWidth(220);
    setupUi();
    setupConnections();

    // Initialise labels with the default AppState values
    onBoardStateChanged(m_state->activeBoard());
    startStLinkBoardProbe();
}

void Sidebar::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 10);
    layout->setSpacing(2);

    // ── App title ─────────────────────────────────────────────────────────
    auto *titleLabel = new QLabel("STM32 AI Deployer", this);
    titleLabel->setObjectName("sidebarTitle");
    layout->addWidget(titleLabel);

    auto *versionLabel = new QLabel("v0.1.0", this);
    versionLabel->setObjectName("sidebarVersionLabel");
    layout->addWidget(versionLabel);

    layout->addSpacing(6);
    layout->addWidget(makeSep(this));
    layout->addSpacing(6);

    // ── Board section ─────────────────────────────────────────────────────
    layout->addWidget(makeSectionLabel("KART", this));

    m_boardCombo = new QComboBox(this);
    m_boardCombo->setObjectName("sidebarCombo");
    layout->addWidget(m_boardCombo);
    populateBoards();
    m_boardCombo->setVisible(false);

    m_boardNameLabel = makeValueLabel("Kart  : --", this);
    m_flashLabel = makeValueLabel("Flash : --", this);
    m_ramLabel   = makeValueLabel("RAM   : --", this);
    m_clockLabel = makeValueLabel("Hız   : --", this);
    layout->addWidget(m_boardNameLabel);
    layout->addWidget(m_flashLabel);
    layout->addWidget(m_ramLabel);
    layout->addWidget(m_clockLabel);

    layout->addSpacing(6);
    layout->addWidget(makeSep(this));
    layout->addSpacing(6);

    // ── Connection section ────────────────────────────────────────────────
    layout->addWidget(makeSectionLabel("BAĞLANTI", this));

    m_portCombo = new QComboBox(this);
    m_portCombo->setObjectName("sidebarCombo");
    layout->addWidget(m_portCombo);
    populatePorts();
    m_portCombo->setVisible(false);

    m_baudCombo = new QComboBox(this);
    m_baudCombo->setObjectName("sidebarCombo");
    layout->addWidget(m_baudCombo);
    populateBauds();
    m_baudCombo->setVisible(false);

    m_connectBtn = new QPushButton(tr("Bağlan"), this);
    m_connectBtn->setObjectName("primaryButton");
    layout->addWidget(m_connectBtn);
    m_connectBtn->setVisible(false);

    m_statusLabel = new QLabel(tr("● Bağlantı Yok"), this);
    m_statusLabel->setObjectName("sidebarConnLabel");
    layout->addWidget(m_statusLabel);

    // Small refresh row
    auto *refreshRow    = new QWidget(this);
    auto *refreshLayout = new QHBoxLayout(refreshRow);
    refreshLayout->setContentsMargins(0, 0, 0, 0);
    refreshLayout->setSpacing(4);
    m_refreshBtn = new QPushButton(tr("⟳ Portları Yenile"), this);
    m_refreshBtn->setObjectName("sidebarSmallBtn");
    refreshLayout->addWidget(m_refreshBtn);
    refreshLayout->addStretch();
    layout->addWidget(refreshRow);
    refreshRow->setVisible(false);

    layout->addSpacing(6);
    layout->addWidget(makeSep(this));
    layout->addSpacing(6);

    // ── Last model section ────────────────────────────────────────────────
    layout->addWidget(makeSectionLabel("SON MODEL", this));

    m_modelNameLabel = makeValueLabel("--", this);
    m_modelMetaLabel = makeValueLabel("--", this);
    layout->addWidget(m_modelNameLabel);
    layout->addWidget(m_modelMetaLabel);

    layout->addStretch();
}

void Sidebar::setupConnections()
{
    // User interactions → AppState writes
    connect(m_boardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Sidebar::onBoardComboChanged);
    connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Sidebar::onPortComboChanged);
    connect(m_baudCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Sidebar::onBaudComboChanged);
    connect(m_connectBtn, &QPushButton::clicked,
            this, &Sidebar::onConnectClicked);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &Sidebar::onRefreshClicked);

    // AppState → UI updates
    connect(m_state, &AppState::connectionChanged,
            this, &Sidebar::onConnectionChanged);
    connect(m_state, &AppState::activeBoardChanged,
            this, &Sidebar::onBoardStateChanged);
    connect(m_state, &AppState::lastModelChanged,
            this, &Sidebar::onLastModelChanged);

    // SerialManager → AppState connection bridge
    connect(m_serial, &SerialManager::connectionChanged,
            m_state, &AppState::setConnected);
}

// ── Population helpers ────────────────────────────────────────────────────

void Sidebar::populateBoards()
{
    m_boardCombo->blockSignals(true);
    m_boardCombo->clear();

    for (const BoardInfo &b : BoardPresets::all())
        m_boardCombo->addItem(b.name, QVariant::fromValue(b));

    AppSettings settings;
    for (const BoardInfo &b : settings.customBoards())
        m_boardCombo->addItem(b.name, QVariant::fromValue(b));

    m_boardCombo->insertSeparator(m_boardCombo->count());
    m_boardCombo->addItem(tr("+ Özel Kart Ekle…"), QVariant());

    // Select the current AppState board
    const QString current = m_state->activeBoard().name;
    for (int i = 0; i < m_boardCombo->count(); ++i) {
        if (m_boardCombo->itemText(i) == current) {
            m_boardCombo->setCurrentIndex(i);
            break;
        }
    }
    m_boardCombo->blockSignals(false);
}

void Sidebar::populatePorts()
{
    m_portCombo->blockSignals(true);
    m_portCombo->clear();
    m_stlinkPortName.clear();

    // ST-Link first
    const QSerialPortInfo stlink = SerialManager::findStLink();
    if (!stlink.isNull()) {
        m_stlinkPortName = stlink.portName();
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

    // Push initial value into AppState
    m_state->setActivePort(m_portCombo->currentData().toString());
    m_portCombo->blockSignals(false);
}

void Sidebar::populateBauds()
{
    m_baudCombo->blockSignals(true);
    m_baudCombo->clear();
    m_baudCombo->addItems({"9600", "38400", "115200", "230400", "921600"});
    m_baudCombo->setCurrentText("115200");
    m_baudCombo->blockSignals(false);
}

void Sidebar::refreshPorts()
{
    populatePorts();
}

void Sidebar::ensureBoardVisible(const BoardInfo &board)
{
    if (board.isNull())
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

    const int insertAt = qMax(0, m_boardCombo->count() - 1);
    m_boardCombo->blockSignals(true);
    m_boardCombo->insertItem(insertAt, board.name, QVariant::fromValue(board));
    m_boardCombo->setCurrentIndex(insertAt);
    m_boardCombo->blockSignals(false);
}

// ── User-action slots ─────────────────────────────────────────────────────

void Sidebar::onBoardComboChanged(int index)
{
    const QVariant v = m_boardCombo->itemData(index);
    if (!v.isValid()) {
        onAddCustomBoardClicked();
        return;
    }
    const BoardInfo board = v.value<BoardInfo>();
    m_state->setActiveBoard(board);
}

void Sidebar::onPortComboChanged(int index)
{
    m_state->setActivePort(m_portCombo->itemData(index).toString());
    startStLinkBoardProbe();
}

void Sidebar::onBaudComboChanged(int /*index*/)
{
    m_state->setActiveBaud(m_baudCombo->currentText().toInt());
}

void Sidebar::onConnectClicked()
{
    if (m_state->isConnected()) {
        m_serial->disconnectPort();
    } else {
        const QString port = m_state->activePort();
        const qint32  baud = m_state->activeBaud();
        if (port.isEmpty()) {
            m_statusLabel->setText(tr("⚠ Port seçilmedi"));
            return;
        }
        startStLinkBoardProbe();
        m_serial->connectToPort(port, baud);
    }
}

void Sidebar::onRefreshClicked()
{
    refreshPorts();
    startStLinkBoardProbe();
}

void Sidebar::onAddCustomBoardClicked()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Özel Kart Ekle"));
    dlg.setMinimumWidth(360);

    auto *layout = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout;
    auto *nameEdit = new QLineEdit(&dlg);
    auto *flashSpin = new QSpinBox(&dlg);
    auto *ramSpin = new QSpinBox(&dlg);
    auto *clockSpin = new QSpinBox(&dlg);

    nameEdit->setPlaceholderText("örn. STM32L4");
    flashSpin->setRange(1, 32768);
    flashSpin->setSuffix(" KB");
    flashSpin->setValue(512);
    ramSpin->setRange(1, 16384);
    ramSpin->setSuffix(" KB");
    ramSpin->setValue(128);
    clockSpin->setRange(1, 1000);
    clockSpin->setSuffix(" MHz");
    clockSpin->setValue(80);

    form->addRow(tr("Kart Adı  :"), nameEdit);
    form->addRow(tr("Flash     :"), flashSpin);
    form->addRow(tr("RAM       :"), ramSpin);
    form->addRow(tr("Saat Hızı :"), clockSpin);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) {
        ensureBoardVisible(m_state->activeBoard());
        return;
    }

    const QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Özel Kart"), tr("Kart adı boş olamaz."));
        ensureBoardVisible(m_state->activeBoard());
        return;
    }

    BoardInfo custom;
    custom.name = name;
    custom.flashKb = flashSpin->value();
    custom.ramKb = ramSpin->value();
    custom.clockMhz = clockSpin->value();
    custom.isPreset = false;

    AppSettings settings;
    settings.addCustomBoard(custom);

    ensureBoardVisible(custom);
    m_state->setActiveBoard(custom);
}

// ── AppState reaction slots ───────────────────────────────────────────────

void Sidebar::onConnectionChanged(bool connected, const QString &info)
{
    if (connected) {
        m_connectBtn->setText(tr("Bağlantıyı Kes"));
        const BoardInfo board = m_state->activeBoard();
        const QString boardName = !board.probeBoardName.isEmpty()
            ? board.probeBoardName
            : board.deviceName;
        const QString portText = !board.portName.isEmpty()
            ? board.portName
            : info.section(' ', 0, 0);
        const QString detail = boardName.isEmpty()
            ? info
            : QString("%1 (%2)").arg(portText, boardName);
        m_statusLabel->setText(tr("● Bağlı — ") + detail);
        m_statusLabel->setStyleSheet("color: #A6E3A1; font-weight: bold;");
        m_portCombo->setEnabled(false);
        m_baudCombo->setEnabled(false);
        m_refreshBtn->setEnabled(false);
    } else {
        m_connectBtn->setText(tr("Bağlan"));
        m_statusLabel->setText(tr("● Bağlantı Yok"));
        m_statusLabel->setStyleSheet("color: #6C7086;");
        m_portCombo->setEnabled(true);
        m_baudCombo->setEnabled(true);
        m_refreshBtn->setEnabled(true);
    }
}

void Sidebar::onBoardStateChanged(const BoardInfo &board)
{
    if (board.isNull()) return;
    ensureBoardVisible(board);
    if (m_boardNameLabel)
        m_boardNameLabel->setText(QString("Kart  : %1").arg(board.name));
    m_flashLabel->setText(QString("Flash : %1 KB").arg(board.flashKb));
    m_ramLabel->setText(  QString("RAM   : %1 KB").arg(board.ramKb));
    m_clockLabel->setText(QString("Hız   : %1 MHz").arg(board.clockMhz));
    if (!m_state->isConnected()) {
        const QString boardName = !board.probeBoardName.isEmpty()
            ? board.probeBoardName
            : board.deviceName;
        if (!board.portName.isEmpty() && !boardName.isEmpty()) {
            m_statusLabel->setText(QString("● %1 (%2)").arg(board.portName, boardName));
            m_statusLabel->setStyleSheet("color: #89B4FA; font-weight: bold;");
        }
    }
}

void Sidebar::onLastModelChanged(const QString &name, double infMs, quint8 acc)
{
    m_modelNameLabel->setText(name.isEmpty() ? "--" : name);
    if (infMs > 0.0 || acc > 0) {
        m_modelMetaLabel->setText(
            QString("%1 ms · %2%").arg(infMs, 0, 'f', 1).arg(acc));
    } else {
        m_modelMetaLabel->setText("--");
    }
}

void Sidebar::startStLinkBoardProbe()
{
    if (m_stlinkPortName.isEmpty())
        return;
    if (m_state->activePort() != m_stlinkPortName)
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

void Sidebar::onStLinkProbeFinished(int exitCode)
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

void Sidebar::applyDetectedStLinkBoard(const QString &probeOutput)
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
    m_state->setActiveBoard(board);

    const QString displayName = !boardName.isEmpty() ? boardName : deviceName;
    if (!displayName.isEmpty() && !m_stlinkPortName.isEmpty()) {
        for (int i = 0; i < m_portCombo->count(); ++i) {
            if (m_portCombo->itemData(i).toString() == m_stlinkPortName) {
                m_portCombo->setItemText(
                    i, QString("★ %1 (%2)").arg(m_stlinkPortName, displayName));
                break;
            }
        }
    }

    if (!m_state->isConnected() && !displayName.isEmpty()) {
        m_statusLabel->setText(QString("● %1 (%2)").arg(m_stlinkPortName, displayName));
        m_statusLabel->setStyleSheet("color: #89B4FA; font-weight: bold;");
    }
}
