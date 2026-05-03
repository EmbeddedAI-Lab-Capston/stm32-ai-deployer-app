#pragma once

#include <QWidget>

class BoardManager;
class QButtonGroup;
class QPushButton;
class QGroupBox;
class QLabel;
class QComboBox;
class QLineEdit;

class BoardTab : public QWidget
{
    Q_OBJECT

public:
    explicit BoardTab(QWidget *parent = nullptr);
    ~BoardTab() override;

signals:
    void boardChanged(const QString &name, int flashKb, int ramKb, int clockMhz);

private slots:
    void onBoardSelected(int id);
    void onSensorChanged(int index);
    void onAddCustomBoardClicked();

private:
    void setupUi();
    void updateBoardInfo(int boardIndex);

    BoardManager *m_boardManager = nullptr;

    QButtonGroup *m_boardGroup = nullptr;
    QPushButton  *m_btnF4      = nullptr;
    QPushButton  *m_btnH7      = nullptr;
    QPushButton  *m_btnN6      = nullptr;

    QLabel *m_infoModel  = nullptr;
    QLabel *m_infoFlash  = nullptr;
    QLabel *m_infoRam    = nullptr;
    QLabel *m_infoClock  = nullptr;
    QLabel *m_infoStatus = nullptr;

    QComboBox *m_sensorCombo = nullptr;
    QWidget   *m_pinWidget   = nullptr;
    QLabel    *m_pin1Label   = nullptr;
    QLabel    *m_pin2Label   = nullptr;
    QLineEdit *m_pin1Edit    = nullptr;
    QLineEdit *m_pin2Edit    = nullptr;
};
