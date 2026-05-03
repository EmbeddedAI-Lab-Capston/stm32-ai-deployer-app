#include "Sidebar.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFrame>
#include <QSerialPortInfo>

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

    m_flashLabel = makeValueLabel("Flash : --", this);
    m_ramLabel   = makeValueLabel("RAM   : --", this);
    m_clockLabel = makeValueLabel("Hız   : --", this);
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

    m_baudCombo = new QComboBox(this);
    m_baudCombo->setObjectName("sidebarCombo");
    layout->addWidget(m_baudCombo);
    populateBauds();

    m_connectBtn = new QPushButton(tr("Bağlan"), this);
    m_connectBtn->setObjectName("primaryButton");
    layout->addWidget(m_connectBtn);

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

    // ST-Link first
    const QSerialPortInfo stlink = SerialManager::findStLink();
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

// ── User-action slots ─────────────────────────────────────────────────────

void Sidebar::onBoardComboChanged(int index)
{
    const QVariant v = m_boardCombo->itemData(index);
    if (!v.isValid()) {
        // "Özel Kart Ekle…" — TODO(asama-5): open custom board dialog
        // For now reset to the previously active board
        populateBoards();
        return;
    }
    const BoardInfo board = v.value<BoardInfo>();
    m_state->setActiveBoard(board);
}

void Sidebar::onPortComboChanged(int index)
{
    m_state->setActivePort(m_portCombo->itemData(index).toString());
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
        m_serial->connectToPort(port, baud);
    }
}

void Sidebar::onRefreshClicked()
{
    refreshPorts();
}

// ── AppState reaction slots ───────────────────────────────────────────────

void Sidebar::onConnectionChanged(bool connected, const QString &info)
{
    if (connected) {
        m_connectBtn->setText(tr("Bağlantıyı Kes"));
        m_statusLabel->setText(tr("● Bağlı — ") + info);
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
    m_flashLabel->setText(QString("Flash : %1 KB").arg(board.flashKb));
    m_ramLabel->setText(  QString("RAM   : %1 KB").arg(board.ramKb));
    m_clockLabel->setText(QString("Hız   : %1 MHz").arg(board.clockMhz));
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
