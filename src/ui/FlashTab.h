#pragma once

#include <QWidget>

class FlashManager;
class QLineEdit;
class QComboBox;
class QTextEdit;
class QLabel;

class FlashTab : public QWidget
{
    Q_OBJECT

public:
    explicit FlashTab(QWidget *parent = nullptr);
    ~FlashTab() override;

private slots:
    void onBrowseClicked();
    void onFlashClicked();

private:
    void setupUi();

    FlashManager *m_flashManager = nullptr;

    QLineEdit *m_modelNameEdit = nullptr;
    QComboBox *m_archCombo     = nullptr;
    QComboBox *m_quantCombo    = nullptr;
    QLineEdit *m_filePathEdit  = nullptr;
    QTextEdit *m_outputEdit    = nullptr;
    QLabel    *m_stlinkLabel   = nullptr;
};
