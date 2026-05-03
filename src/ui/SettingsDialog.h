#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;

// Dialog for application settings (Tools → Settings).
// Displayed on first launch when STM32_Programmer_CLI path is not configured.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

private slots:
    void onBrowseClicked();
    void onTestCliClicked();
    void onBrowseXCubeAIClicked();
    void onTestXCubeAIClicked();
    void onSaveClicked();
    void onCancelClicked();

private:
    void setupUi();
    void loadCurrentValues();

    // STM32 Programmer CLI
    QLineEdit   *m_cliPathEdit    = nullptr;
    QPushButton *m_browseButton   = nullptr;
    QPushButton *m_testButton     = nullptr;

    // X-CUBE-AI CLI
    QLineEdit   *m_xcubePathEdit      = nullptr;
    QPushButton *m_xcubeBrowseButton  = nullptr;
    QPushButton *m_xcubeTestButton    = nullptr;

    QPushButton *m_saveButton   = nullptr;
    QPushButton *m_cancelButton = nullptr;
};
