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

#include "core/AppState.h"
#include "core/AppSettings.h"
#include "ui/Sidebar.h"
#include "ui/BoardTab.h"
#include "ui/FlashTab.h"
#include "ui/MonitorTab.h"
#include "ui/BenchmarkTab.h"
#include "ui/AnalysisTab.h"
#include "ui/SettingsDialog.h"
#include "modules/serial/SerialManager.h"
#include "modules/flash/FlashManager.h"
#include "core/ToolDetector.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("STM32 AI Deployer");
    setMinimumSize(1100, 700);
    resize(1280, 800);

    // ── Core objects (created before UI) ──────────────────────────────────
    m_appState      = new AppState(this);
    m_serialManager = new SerialManager(this);
    m_flashManager  = new FlashManager(this);

    // CLI path
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

    setupMenuBar();
    setupCentralWidget();
    setupStatusBar();
    setupConnections();

    AppSettings settings;
    if (settings.programmerCliPath().isEmpty()) {
        QTimer::singleShot(200, this, &MainWindow::onSettingsTriggered);
    }

    // First-launch tool auto-detection
    if (!settings.toolsAutoDetected()) {
        m_toolDetector = new ToolDetector(this);

        connect(m_toolDetector, &ToolDetector::toolFound,
                this, [this](const ToolInfo &info) {
                    // Save each found tool path to the appropriate setting
                    AppSettings s;
                    if (info.key == "tools/gcc_path")
                        s.setGccPath(info.path);
                    else if (info.key == "tools/make_path")
                        s.setMakePath(info.path);
                    else if (info.key == "programmer/cli_path")
                        s.setProgrammerCliPath(info.path);
                    else if (info.key == "tools/xcubeai_cli_path")
                        s.setXCubeAICliPath(info.path);

                    statusBar()->showMessage(
                        QString("✓ %1 bulundu").arg(info.name), 3000);
                });

        connect(m_toolDetector, &ToolDetector::detectionFinished,
                this, [this](const QList<ToolInfo> &) {
                    AppSettings s;
                    s.setToolsAutoDetected(true);
                    if (m_flashTab) {
                        m_flashTab->refreshCliStatus();
                        m_flashTab->refreshXCubeAIStatus();
                    }
                });

        m_toolDetector->detectAll();
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

void MainWindow::setupCentralWidget()
{
    auto *container = new QWidget(this);
    auto *hLayout   = new QHBoxLayout(container);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);

    // ── Sidebar ───────────────────────────────────────────────────────────
    m_sidebar = new Sidebar(m_appState, m_serialManager, container);
    hLayout->addWidget(m_sidebar);

    // ── Tab widget ────────────────────────────────────────────────────────
    m_tabWidget = new QTabWidget(container);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setDocumentMode(false);

    m_boardTab    = new BoardTab(m_appState, m_serialManager, this);
    m_flashTab    = new FlashTab(m_appState, this);
    m_monitorTab  = new MonitorTab(m_appState, m_serialManager, this);
    m_benchmarkTab = new BenchmarkTab(m_appState, m_serialManager, this);
    m_analysisTab = new AnalysisTab(this);

    m_flashTab->initialize(m_flashManager);

    auto *s = m_tabWidget->style();
    m_tabWidget->addTab(m_boardTab,
        s->standardIcon(QStyle::SP_ComputerIcon),
        tr("Kart Seçimi ve Bilgileri"));
    m_tabWidget->addTab(m_flashTab,
        s->standardIcon(QStyle::SP_ArrowUp),
        tr("Model & Flash"));
    m_tabWidget->addTab(m_monitorTab,
        s->standardIcon(QStyle::SP_FileDialogDetailedView),
        tr("UART Monitör"));
    m_tabWidget->addTab(m_benchmarkTab,
        s->standardIcon(QStyle::SP_ComputerIcon),
        tr("Kart Benchmark Testleri"));
    m_tabWidget->addTab(m_analysisTab,
        s->standardIcon(QStyle::SP_FileDialogInfoView),
        tr("Analiz"));

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

void MainWindow::setupConnections()
{
    auto applyBoardFromWire = [this](const QString &cardName,
                                     int flashKb,
                                     int ramKb,
                                     int clockMhz) {
        const QString name = cardName.trimmed();
        if (name.isEmpty())
            return;

        BoardInfo board = BoardPresets::find(name);

        if (board.isNull()) {
            AppSettings settings;
            for (const BoardInfo &custom : settings.customBoards()) {
                if (custom.name.compare(name, Qt::CaseInsensitive) == 0) {
                    board = custom;
                    break;
                }
            }

            if (board.isNull()) {
                board.name = name;
                board.isPreset = false;
            }
        }

        if (flashKb > 0)
            board.flashKb = flashKb;
        if (ramKb > 0)
            board.ramKb = ramKb;
        if (clockMhz > 0)
            board.clockMhz = clockMhz;

        if (!board.isPreset) {
            AppSettings settings;
            settings.addCustomBoard(board);
        }

        m_appState->setActiveBoard(board);
    };

    // AppState → status bar
    connect(m_appState, &AppState::connectionChanged,
            this, [this](bool connected, const QString &info) {
                if (connected) {
                    m_connectionLabel->setText(tr("  ● Bağlı — ") + info + tr("  "));
                    m_connectionLabel->setStyleSheet("color: #A6E3A1;");
                } else {
                    m_connectionLabel->setText(tr("  ● Bağlantı Yok  "));
                    m_connectionLabel->setStyleSheet("color: #F38BA8;");
                }
            });

    connect(m_appState, &AppState::activeBoardChanged,
            this, [this](const BoardInfo &board) {
                m_boardLabel->setText(
                    QString("  Kart: %1  ").arg(board.name));
            });

    // FlashManager → AppState (son model)
    connect(m_flashManager, &FlashManager::flashFinished,
            this, [this](bool success, const FlashConfig &config) {
                if (success) {
                    m_appState->setLastModel(config.modelName, 0, 0);
                    // TODO(asama-5): inf ms ve accuracy UART'tan gelecek
                }
            });

    connect(m_serialManager, &SerialManager::connectionChanged,
            this, [this](bool connected, const QString &) {
                if (connected)
                    QTimer::singleShot(700, m_serialManager, &SerialManager::requestBoardInfo);
            });

    connect(m_serialManager, &SerialManager::bootReceived,
            this, [this](const BootData &boot) {
                if (!boot.model.isEmpty())
                    m_appState->setLastModel(boot.model, 0.0, 0);

                auto applyBoard = [this](const QString &cardName,
                                         int flashKb,
                                         int ramKb,
                                         int clockMhz) {
                    const QString name = cardName.trimmed();
                    if (name.isEmpty())
                        return;

                    BoardInfo board = BoardPresets::find(name);
                    if (board.isNull()) {
                        AppSettings settings;
                        for (const BoardInfo &custom : settings.customBoards()) {
                            if (custom.name.compare(name, Qt::CaseInsensitive) == 0) {
                                board = custom;
                                break;
                            }
                        }
                        if (board.isNull()) {
                            board.name = name;
                            board.isPreset = false;
                        }
                    }

                    if (flashKb > 0) board.flashKb = flashKb;
                    if (ramKb > 0) board.ramKb = ramKb;
                    if (clockMhz > 0) board.clockMhz = clockMhz;

                    if (!board.isPreset) {
                        AppSettings settings;
                        settings.addCustomBoard(board);
                    }

                    m_appState->setActiveBoard(board);
                };

                applyBoard(boot.card,
                           static_cast<int>(boot.flash_kb),
                           static_cast<int>(boot.ram_kb),
                           static_cast<int>(boot.clock_mhz));
                if (boot.baud > 0)
                    m_appState->setActiveBaud(static_cast<qint32>(boot.baud));
            });

    connect(m_serialManager, &SerialManager::inferenceReceived,
            this, [applyBoardFromWire](const InferenceData &data) {
                applyBoardFromWire(data.card, 0, 0, 0);
            });

    // Refresh CLI status after settings change
    // (handled in onSettingsTriggered)
}

void MainWindow::onSettingsTriggered()
{
    if (!m_settingsDialog)
        m_settingsDialog = new SettingsDialog(this);

    if (m_settingsDialog->exec() == QDialog::Accepted) {
        AppSettings settings;
        m_flashManager->setCliPath(settings.programmerCliPath());
        if (m_flashTab) {
            m_flashTab->refreshCliStatus();
            m_flashTab->refreshXCubeAIStatus();
        }
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
