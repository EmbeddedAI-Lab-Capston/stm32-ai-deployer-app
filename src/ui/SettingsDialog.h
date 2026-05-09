#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

// Settings dialog for all external tool paths.
// Displayed on first launch when STM32_Programmer_CLI is not configured.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

private slots:
    // STM32 Programmer CLI
    void onBrowseClicked();
    void onTestCliClicked();

    // X-CUBE-AI CLI
    void onBrowseXCubeAIClicked();
    void onTestXCubeAIClicked();

    // arm-none-eabi-gcc
    void onBrowseGccClicked();
    void onTestGccClicked();

    // make
    void onBrowseMakeClicked();
    void onTestMakeClicked();

    // Scan all tools automatically
    void onScanAllClicked();

    void onSaveClicked();
    void onCancelClicked();

private:
    void setupUi();
    void loadCurrentValues();

    // Returns a rich-text status label: green tick or red cross
    QString makeStatusText(const QString &versionOrEmpty) const;

    // STM32 Programmer CLI
    QLineEdit   *m_cliPathEdit      = nullptr;
    QPushButton *m_browseButton     = nullptr;
    QPushButton *m_testButton       = nullptr;
    QLabel      *m_cliStatus        = nullptr;

    // X-CUBE-AI CLI
    QLineEdit   *m_xcubePathEdit    = nullptr;
    QPushButton *m_xcubeBrowseBtn   = nullptr;
    QPushButton *m_xcubeTestBtn     = nullptr;
    QLabel      *m_xcubeStatus      = nullptr;

    // arm-none-eabi-gcc
    QLineEdit   *m_gccPathEdit      = nullptr;
    QPushButton *m_gccBrowseBtn     = nullptr;
    QPushButton *m_gccTestBtn       = nullptr;
    QLabel      *m_gccStatus        = nullptr;

    // make
    QLineEdit   *m_makePathEdit     = nullptr;
    QPushButton *m_makeBrowseBtn    = nullptr;
    QPushButton *m_makeTestBtn      = nullptr;
    QLabel      *m_makeStatus       = nullptr;

    QPushButton *m_scanAllBtn   = nullptr;
    QPushButton *m_saveButton   = nullptr;
    QPushButton *m_cancelButton = nullptr;
};
