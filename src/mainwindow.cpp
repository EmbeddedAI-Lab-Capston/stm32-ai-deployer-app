#include "mainwindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QLabel>
#include <QTabWidget>
#include <QMessageBox>
#include <QTimer>

#include "ui/BoardTab.h"
#include "ui/FlashTab.h"
#include "ui/MonitorTab.h"
#include "ui/AnalysisTab.h"
#include "ui/SettingsDialog.h"
#include "core/AppSettings.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("STM32 AI Deployer");
    setMinimumSize(1024, 700);
    resize(1280, 800);

    setupMenuBar();
    setupCentralWidget();
    setupStatusBar();

    // Show settings dialog on first launch if CLI path is not set
    AppSettings settings;
    if (settings.programmerCliPath().isEmpty()) {
        QTimer::singleShot(200, this, &MainWindow::onSettingsTriggered);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenuBar()
{
    // Dosya menu
    QMenu *fileMenu = menuBar()->addMenu(tr("Dosya"));
    QAction *exitAction = fileMenu->addAction(tr("Çıkış"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onExitTriggered);

    // Araçlar menu
    QMenu *toolsMenu = menuBar()->addMenu(tr("Araçlar"));
    QAction *settingsAction = toolsMenu->addAction(tr("Ayarlar"));
    settingsAction->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsTriggered);

    // Yardım menu
    QMenu *helpMenu = menuBar()->addMenu(tr("Yardım"));
    QAction *aboutAction = helpMenu->addAction(tr("Hakkında"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAboutTriggered);
}

void MainWindow::setupCentralWidget()
{
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::North);
    m_tabWidget->setDocumentMode(false);

    m_boardTab    = new BoardTab(this);
    m_flashTab    = new FlashTab(this);
    m_monitorTab  = new MonitorTab(this);
    m_analysisTab = new AnalysisTab(this);

    m_tabWidget->addTab(m_boardTab,    tr("Kart Yönetimi"));
    m_tabWidget->addTab(m_flashTab,    tr("Model & Flash"));
    m_tabWidget->addTab(m_monitorTab,  tr("UART Monitör"));
    m_tabWidget->addTab(m_analysisTab, tr("Analiz"));

    setCentralWidget(m_tabWidget);
}

void MainWindow::setupStatusBar()
{
    m_connectionLabel = new QLabel(tr("  Bağlantı yok  "));
    m_connectionLabel->setObjectName("statusConnectionLabel");

    m_portLabel = new QLabel(tr("  Port: —  "));
    m_portLabel->setObjectName("statusPortLabel");

    m_boardLabel = new QLabel(tr("  Kart: —  "));
    m_boardLabel->setObjectName("statusBoardLabel");

    statusBar()->addWidget(m_connectionLabel);
    statusBar()->addWidget(m_portLabel);
    statusBar()->addPermanentWidget(m_boardLabel);
}

void MainWindow::onSettingsTriggered()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
    }
    m_settingsDialog->exec();
}

void MainWindow::onAboutTriggered()
{
    QMessageBox::about(this,
        tr("STM32 AI Deployer Hakkında"),
        tr("<b>STM32 AI Deployer</b> v1.0.0<br><br>"
           "STM32 serisi kartlara AI modeli yükleme ve<br>"
           "gerçek zamanlı metrik izleme uygulaması.<br><br>"
           "Marmara Üniversitesi Bilgisayar Mühendisliği<br>"
           "Bitirme Projesi — 2025<br><br>"
           "Muhammet Ali Şeker<br>"
           "Furkan Talha Kasım<br>"
           "Kadir Mert Abatay"));
}

void MainWindow::onExitTriggered()
{
    close();
}
