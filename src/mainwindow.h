#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QTabWidget>
#include <QFrame>

class BoardTab;
class FlashTab;
class MonitorTab;
class AnalysisTab;
class SettingsDialog;

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

    void onConnectionStatusChanged(bool connected, const QString &info);
    void onInferenceMetricUpdated(const QString &model, double ms, int accPct);

private:
    void setupMenuBar();
    void setupCentralWidget();
    void setupSidebar();
    void setupStatusBar();
    QFrame *createSidebarSeparator();

    QTabWidget     *m_tabWidget      = nullptr;
    BoardTab       *m_boardTab       = nullptr;
    FlashTab       *m_flashTab       = nullptr;
    MonitorTab     *m_monitorTab     = nullptr;
    AnalysisTab    *m_analysisTab    = nullptr;
    SettingsDialog *m_settingsDialog = nullptr;

    // Sidebar
    QFrame *m_sidebar = nullptr;

    // Sidebar value labels (updated in Phase 3+)
    QLabel *m_sbConnLabel    = nullptr;
    QLabel *m_sbPortLabel    = nullptr;
    QLabel *m_sbBaudLabel    = nullptr;
    QLabel *m_sbBoardLabel   = nullptr;
    QLabel *m_sbFlashLabel   = nullptr;
    QLabel *m_sbRamLabel     = nullptr;
    QLabel *m_sbSessionLabel = nullptr;
    QLabel *m_sbMetricLabel  = nullptr;

    // Status bar
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_boardLabel      = nullptr;
    QLabel *m_versionLabel    = nullptr;
};
