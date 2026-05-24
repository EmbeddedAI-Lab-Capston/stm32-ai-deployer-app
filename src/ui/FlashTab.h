#pragma once

#include <QWidget>
#include "modules/flash/FlashManager.h"
#include "modules/flash/PipelineConfig.h"
#include "modules/flash/XCubeAIRunner.h"
#include "core/AppState.h"

class QButtonGroup;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;
class QRadioButton;

class FlashTab : public QWidget
{
    Q_OBJECT

public:
    explicit FlashTab(AppState *state, QWidget *parent = nullptr);
    ~FlashTab() override;

    // Called by MainWindow after construction — wires up FlashManager signals
    void initialize(FlashManager *manager);

    // Re-reads AppSettings and updates CLI status labels + manager paths
    void refreshCliStatus();
    void refreshXCubeAIStatus();

signals:
    void pipelineModelFlashed(const PipelineConfig &config);

private slots:
    // Hex firmware panel
    void onBrowseClicked();
    void onHexFileSelected(const QString &path);
    void onFlashClicked();
    void onCancelClicked();
    void onSettingsClicked();
    void onClearOutputClicked();

    // AI model panel
    void onAIModelBrowseClicked();
    void onAIOutputDirClicked();
    void onGenerateClicked();
    void onSourceModeChanged(int id, bool checked);
    void onPipelineWizardClicked();

    void showGenerationResult(const XCubeAIResult &result);
    void showNextStepDialog();

    void appendOutputLine(const QString &line);
    void appendErrorLine(const QString &line);
    void showSuccessBanner(const FlashConfig &config);

    // AppState reactions
    void onBoardChanged(const BoardInfo &board);
    void onConnectionChanged(bool connected, const QString &info);

private:
    void setupUi();

    AppState      *m_appState     = nullptr;
    FlashManager  *m_flashManager = nullptr;
    XCubeAIRunner *m_xcubeRunner  = nullptr;

    // ── Source selection ───────────────────────────────────────────────────
    QRadioButton *m_hexRadio   = nullptr;
    QRadioButton *m_modelRadio = nullptr;

    // ── Hex firmware panel ─────────────────────────────────────────────────
    QWidget     *m_hexPanel       = nullptr;
    QLabel      *m_cliStatusLabel = nullptr;

    QLineEdit   *m_modelNameEdit  = nullptr;
    QComboBox   *m_archCombo      = nullptr;
    QComboBox   *m_quantCombo     = nullptr;
    QLineEdit   *m_filePathEdit   = nullptr;
    QLabel      *m_fileInfoLabel  = nullptr;
    QCheckBox   *m_simModeCheck   = nullptr;

    QPushButton *m_flashBtn  = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    // ── AI model panel ─────────────────────────────────────────────────────
    QWidget     *m_aiPanel          = nullptr;
    QLabel      *m_xcubeStatusLabel = nullptr;

    QLineEdit   *m_aiModelPathEdit = nullptr;
    QComboBox   *m_aiQuantCombo    = nullptr;
    QLineEdit   *m_aiOutputDirEdit = nullptr;

    QPushButton *m_generateBtn   = nullptr;
    QPushButton *m_aiCancelBtn   = nullptr;

    // Result buttons (shown after successful generation)
    QPushButton *m_openDirBtn  = nullptr;
    QPushButton *m_nextStepBtn = nullptr;

    // Pipeline wizard button (shown at top of source selection)
    QPushButton *m_wizardBtn   = nullptr;

    // ── Shared widgets ─────────────────────────────────────────────────────
    QLabel       *m_stlinkLabel  = nullptr;
    QLabel       *m_targetLabel  = nullptr;
    QProgressBar *m_progressBar  = nullptr;
    QTextEdit    *m_outputEdit   = nullptr;
};
