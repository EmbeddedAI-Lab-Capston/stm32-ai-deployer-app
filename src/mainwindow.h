#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QTabWidget>

class AppState;
class Sidebar;
class BoardTab;
class FlashTab;
class MonitorTab;
class AnalysisTab;
class BenchmarkTab;
class SettingsDialog;
class FlashManager;
class SerialManager;
class ToolDetector;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onSettingsTriggered();
    void onAboutTriggered();
    void onExitTriggered();

private:
    void setupMenuBar();
    void setupCentralWidget();
    void setupStatusBar();
    void setupConnections();

    // ── Core state & managers ────────────────────────────────────────────
    AppState      *m_appState      = nullptr;
    SerialManager *m_serialManager = nullptr;
    FlashManager  *m_flashManager  = nullptr;
    ToolDetector  *m_toolDetector  = nullptr;

    // ── UI ───────────────────────────────────────────────────────────────
    Sidebar        *m_sidebar       = nullptr;
    QTabWidget     *m_tabWidget     = nullptr;
    BoardTab       *m_boardTab      = nullptr;
    FlashTab       *m_flashTab      = nullptr;
    MonitorTab     *m_monitorTab    = nullptr;
    BenchmarkTab   *m_benchmarkTab  = nullptr;
    AnalysisTab    *m_analysisTab   = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;

    // Status bar labels (updated via AppState signals)
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_boardLabel      = nullptr;
    QLabel *m_versionLabel    = nullptr;
};
