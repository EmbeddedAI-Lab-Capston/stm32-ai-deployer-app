#pragma once

#include <QWidget>

#include "core/AppState.h"
#include "modules/serial/PacketParser.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QProgressBar;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QProcess;
class SerialManager;

class BenchmarkTab : public QWidget
{
    Q_OBJECT

public:
    explicit BenchmarkTab(AppState *state, SerialManager *serial, QWidget *parent = nullptr);
    ~BenchmarkTab() override;

private slots:
    void onBrowseModelClicked();
    void onRunClicked();
    void onCancelClicked();
    void onReadyRead();
    void onProcessFinished(int exitCode);
    void onBenchReceived(const BenchData &data);
    void onBoardChanged(const BoardInfo &board);
    void onPortChanged(const QString &port);
    void onBaudChanged(qint32 baud);

private:
    void setupUi();
    void startBenchmarkProcess();
    void sendBenchmarkCommand();
    void appendLog(const QString &text);
    void parseMetrics(const QString &text);
    void loadModelReportMetrics();
    QString xcubeCliPath() const;
    void resetMetrics();

    AppState *m_state = nullptr;
    SerialManager *m_serial = nullptr;
    QProcess *m_process = nullptr;

    QLineEdit *m_modelPathEdit = nullptr;
    QSpinBox *m_batchSpin = nullptr;
    QDoubleSpinBox *m_rangeMinSpin = nullptr;
    QDoubleSpinBox *m_rangeMaxSpin = nullptr;
    QSpinBox *m_seedSpin = nullptr;
    QCheckBox *m_saveCsvCheck = nullptr;

    QLabel *m_boardLabel = nullptr;
    QLabel *m_portLabel = nullptr;
    QLabel *m_inferenceLabel = nullptr;
    QLabel *m_modelLabel = nullptr;
    QLabel *m_ramLabel = nullptr;
    QLabel *m_flashLabel = nullptr;
    QLabel *m_maccLabel = nullptr;
    QLabel *m_statusLabel = nullptr;

    QPushButton *m_runBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QProgressBar *m_progress = nullptr;
    QPlainTextEdit *m_output = nullptr;
};
