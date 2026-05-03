#pragma once

#include <QWidget>
#include "modules/flash/FlashManager.h"
#include "core/AppState.h"

class QLineEdit;
class QComboBox;
class QTextEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;

class FlashTab : public QWidget
{
    Q_OBJECT

public:
    explicit FlashTab(AppState *state, QWidget *parent = nullptr);
    ~FlashTab() override;

    // Called by MainWindow after construction — wires up FlashManager signals
    void initialize(FlashManager *manager);

    // Re-reads AppSettings and updates CLI status label + manager path
    void refreshCliStatus();

private slots:
    void onBrowseClicked();
    void onFlashClicked();
    void onCancelClicked();
    void onSettingsClicked();
    void onClearOutputClicked();

    void appendOutputLine(const QString &line);
    void appendErrorLine(const QString &line);
    void showSuccessBanner(const FlashConfig &config);

    // AppState reactions
    void onBoardChanged(const BoardInfo &board);
    void onConnectionChanged(bool connected, const QString &info);

private:
    void setupUi();

    AppState     *m_appState     = nullptr;
    FlashManager *m_flashManager = nullptr;

    // CLI status bar
    QLabel      *m_cliStatusLabel = nullptr;

    // Model info form
    QLineEdit   *m_modelNameEdit = nullptr;
    QComboBox   *m_archCombo     = nullptr;
    QComboBox   *m_quantCombo    = nullptr;
    QLineEdit   *m_filePathEdit  = nullptr;
    QCheckBox   *m_simModeCheck  = nullptr;

    // Status row
    QLabel      *m_stlinkLabel  = nullptr;
    QLabel      *m_targetLabel  = nullptr;

    // Action row
    QProgressBar *m_progressBar = nullptr;
    QPushButton  *m_flashBtn    = nullptr;
    QPushButton  *m_cancelBtn   = nullptr;

    // Output
    QTextEdit   *m_outputEdit   = nullptr;
};
