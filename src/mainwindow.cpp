#include "mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QMessageBox>
#include <QTimer>
#include <QStyle>
#include <QFile>

#include "ui/BoardTab.h"
#include "ui/FlashTab.h"
#include "ui/MonitorTab.h"
#include "ui/AnalysisTab.h"
#include "ui/SettingsDialog.h"
#include "core/AppSettings.h"
#include "modules/flash/FlashManager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("STM32 AI Deployer");
    setMinimumSize(1100, 700);
    resize(1280, 800);

    setupMenuBar();
    setupCentralWidget();
    setupStatusBar();

    AppSettings settings;
    if (settings.programmerCliPath().isEmpty()) {
        QTimer::singleShot(200, this, &MainWindow::onSettingsTriggered);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("Dosya"));
    QAction *exitAction = fileMenu->addAction(tr("Çıkış"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExitTriggered);

    QMenu *toolsMenu = menuBar()->addMenu(tr("Araçlar"));
    QAction *settingsAction = toolsMenu->addAction(tr("Ayarlar"));
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsTriggered);

    QMenu *helpMenu = menuBar()->addMenu(tr("Yardım"));
    QAction *aboutAction = helpMenu->addAction(tr("Hakkında"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAboutTriggered);
}

QFrame *MainWindow::createSidebarSeparator()
{
    auto *sep = new QFrame(m_sidebar);
    sep->setFrameShape(QFrame::HLine);
    sep->setObjectName("sidebarSeparator");
    return sep;
}

void MainWindow::setupSidebar()
{
    m_sidebar = new QFrame;
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(220);
    m_sidebar->setFrameShape(QFrame::NoFrame);

    auto *layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, 10, 0, 10);
    layout->setSpacing(2);

    auto *titleLabel = new QLabel("STM32 AI Deployer", m_sidebar);
    titleLabel->setObjectName("sidebarTitle");
    layout->addWidget(titleLabel);

    auto *versionLabel = new QLabel("v0.1.0", m_sidebar);
    versionLabel->setObjectName("sidebarVersionLabel");
    layout->addWidget(versionLabel);

    layout->addSpacing(6);
    layout->addWidget(createSidebarSeparator());
    layout->addSpacing(6);

    // Connection section
    auto *connSection = new QLabel("BAĞLANTI", m_sidebar);
    connSection->setObjectName("sidebarSectionLabel");
    layout->addWidget(connSection);

    m_sbConnLabel = new QLabel("● Bağlantı Yok", m_sidebar);
    m_sbConnLabel->setObjectName("sidebarConnLabel");
    layout->addWidget(m_sbConnLabel);

    m_sbPortLabel = new QLabel("Port : --", m_sidebar);
    m_sbPortLabel->setObjectName("sidebarValueLabel");
    layout->addWidget(m_sbPortLabel);

    m_sbBaudLabel = new QLabel("Baud : 115200", m_sidebar);
    m_sbBaudLabel->setObjectName("sidebarValueLabel");
    layout->addWidget(m_sbBaudLabel);

    layout->addSpacing(6);
    layout->addWidget(createSidebarSeparator());
    layout->addSpacing(6);

    // Active board section
    auto *boardSection = new QLabel("AKTİF KART", m_sidebar);
    boardSection->setObjectName("sidebarSectionLabel");
    layout->addWidget(boardSection);

    m_sbBoardLabel = new QLabel("STM32F4", m_sidebar);
    m_sbBoardLabel->setObjectName("sidebarBoardLabel");
    layout->addWidget(m_sbBoardLabel);

    m_sbFlashLabel = new QLabel("Flash: 1024 KB", m_sidebar);
    m_sbFlashLabel->setObjectName("sidebarValueLabel");
    layout->addWidget(m_sbFlashLabel);

    m_sbRamLabel = new QLabel("RAM  : 192 KB", m_sidebar);
    m_sbRamLabel->setObjectName("sidebarValueLabel");
    layout->addWidget(m_sbRamLabel);

    layout->addSpacing(6);
    layout->addWidget(createSidebarSeparator());
    layout->addSpacing(6);

    // Last session section
    auto *sessionSection = new QLabel("SON OTURUM", m_sidebar);
    sessionSection->setObjectName("sidebarSectionLabel");
    layout->addWidget(sessionSection);

    m_sbSessionLabel = new QLabel("MLP_INT8", m_sidebar);
    m_sbSessionLabel->setObjectName("sidebarValueLabel");
    layout->addWidget(m_sbSessionLabel);

    m_sbMetricLabel = new QLabel("8.2 ms · 96%", m_sidebar);
    m_sbMetricLabel->setObjectName("sidebarValueLabel");
    layout->addWidget(m_sbMetricLabel);

    layout->addStretch();
}

void MainWindow::setupCentralWidget()
{
    auto *container = new QWidget(this);
    auto *hLayout = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);

    setupSidebar();
    hLayout->addWidget(m_sidebar);

    m_tabWidget = new QTabWidget(container);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setDocumentMode(false);

    m_boardTab    = new BoardTab(this);
    m_flashTab    = new FlashTab(this);
    m_monitorTab  = new MonitorTab(this);
    m_analysisTab = new AnalysisTab(this);

    // Create FlashManager, detect CLI path, initialize FlashTab
    m_flashManager = new FlashManager(this);
    {
        AppSettings settings;
        QString cliPath = settings.programmerCliPath();
        if (cliPath.isEmpty() || !QFile::exists(cliPath)) {
            cliPath = FlashManager::detectCliPath();
            if (!cliPath.isEmpty())
                settings.setProgrammerCliPath(cliPath);
        }
        m_flashManager->setCliPath(cliPath);
    }
    m_flashTab->initialize(m_flashManager);

    connect(m_flashManager, &FlashManager::flashFinished,
            this, [this](bool success, const FlashConfig &config) {
                if (success) {
                    m_sbSessionLabel->setText(config.modelName);
                    m_sbMetricLabel->setText(
                        config.architecture + " / " + config.quantization);
                    // TODO(asama-5): Oturumu SQLite'a kaydet
                }
            });

    auto *s = m_tabWidget->style();
    m_tabWidget->addTab(m_boardTab,
        s->standardIcon(QStyle::SP_ComputerIcon),       tr("Kart Yönetimi"));
    m_tabWidget->addTab(m_flashTab,
        s->standardIcon(QStyle::SP_ArrowUp),            tr("Model & Flash"));
    m_tabWidget->addTab(m_monitorTab,
        s->standardIcon(QStyle::SP_FileDialogDetailedView), tr("UART Monitör"));

    connect(m_monitorTab, &MonitorTab::connectionStatusChanged,
            this, &MainWindow::onConnectionStatusChanged);
    connect(m_monitorTab, &MonitorTab::inferenceMetricUpdated,
            this, &MainWindow::onInferenceMetricUpdated);
    connect(m_boardTab, &BoardTab::boardChanged,
            this, &MainWindow::onBoardChanged);
    m_tabWidget->addTab(m_analysisTab,
        s->standardIcon(QStyle::SP_FileDialogInfoView), tr("Analiz"));

    hLayout->addWidget(m_tabWidget, 1);
    setCentralWidget(container);
}

void MainWindow::setupStatusBar()
{
    m_connectionLabel = new QLabel(tr("  ● Bağlantı Yok  "));
    m_connectionLabel->setObjectName("statusConnectionLabel");

    m_boardLabel = new QLabel(tr("  Kart: STM32F4  "));
    m_boardLabel->setObjectName("statusBoardLabel");

    m_versionLabel = new QLabel(tr("  STM32 AI Deployer v0.1.0  "));
    m_versionLabel->setObjectName("statusVersionLabel");

    statusBar()->addWidget(m_connectionLabel);
    statusBar()->addWidget(m_boardLabel);
    statusBar()->addPermanentWidget(m_versionLabel);
}

void MainWindow::onSettingsTriggered()
{
    if (!m_settingsDialog)
        m_settingsDialog = new SettingsDialog(this);

    if (m_settingsDialog->exec() == QDialog::Accepted) {
        // Propagate new CLI path to FlashManager + FlashTab status label
        if (m_flashManager) {
            AppSettings settings;
            m_flashManager->setCliPath(settings.programmerCliPath());
        }
        if (m_flashTab)
            m_flashTab->refreshCliStatus();
    }
}

void MainWindow::onAboutTriggered()
{
    QMessageBox::about(this,
        tr("STM32 AI Deployer Hakkında"),
        tr("<b>STM32 AI Deployer</b> v0.1.0<br><br>"
           "STM32 serisi kartlara AI modeli yükleme ve<br>"
           "gerçek zamanlı metrik izleme uygulaması.<br><br>"
           "Marmara Üniversitesi Bilgisayar Mühendisliği<br>"
           "Bitirme Projesi — 2025-2026<br><br>"
           "Muhammet Ali Şeker<br>"
           "Furkan Talha Kasım<br>"
           "Kadir Mert Abatay"));
}

void MainWindow::onExitTriggered()
{
    close();
}

void MainWindow::onConnectionStatusChanged(bool connected, const QString &info)
{
    if (connected) {
        const QString port = info.section('@', 0, 0).trimmed();
        const QString baud = info.section('@', 1).trimmed();
        m_sbConnLabel->setText(tr("● Bağlı"));
        m_sbConnLabel->setStyleSheet("color: #A6E3A1; font-weight: bold;");
        m_sbPortLabel->setText(tr("Port : ") + port);
        m_sbBaudLabel->setText(tr("Baud : ") + baud);
        m_connectionLabel->setText(tr("  ● Bağlı — ") + info + tr("  "));
        m_connectionLabel->setStyleSheet("color: #A6E3A1;");
    } else {
        m_sbConnLabel->setText(tr("● Bağlantı Yok"));
        m_sbConnLabel->setStyleSheet("color: #F38BA8;");
        m_sbPortLabel->setText(tr("Port : --"));
        m_sbBaudLabel->setText(tr("Baud : 115200"));
        m_connectionLabel->setText(tr("  ● Bağlantı Yok  "));
        m_connectionLabel->setStyleSheet("color: #F38BA8;");
    }
}

void MainWindow::onInferenceMetricUpdated(const QString &model, double ms, int accPct)
{
    m_sbSessionLabel->setText(model);
    m_sbMetricLabel->setText(
        QString("%1 ms · %2%").arg(ms, 0, 'f', 1).arg(accPct));
}

void MainWindow::onBoardChanged(const QString &name, int flashKb, int ramKb, int clockMhz)
{
    m_sbBoardLabel->setText(name);
    m_sbFlashLabel->setText(QString("Flash: %1 KB").arg(flashKb));
    m_sbRamLabel->setText(QString("RAM  : %1 KB").arg(ramKb));
    m_boardLabel->setText(QString("  Kart: %1  ").arg(name));
    Q_UNUSED(clockMhz)
}
