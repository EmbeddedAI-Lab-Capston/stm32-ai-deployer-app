#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QTabWidget>

class BoardTab;
class FlashTab;
class MonitorTab;
class AnalysisTab;
class SettingsDialog;
class AppSettings;

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

    QTabWidget     *m_tabWidget       = nullptr;
    BoardTab       *m_boardTab        = nullptr;
    FlashTab       *m_flashTab        = nullptr;
    MonitorTab     *m_monitorTab      = nullptr;
    AnalysisTab    *m_analysisTab     = nullptr;
    SettingsDialog *m_settingsDialog  = nullptr;

    // Status bar widgets
    QLabel *m_connectionLabel = nullptr;
    QLabel *m_boardLabel      = nullptr;
    QLabel *m_portLabel       = nullptr;
};
