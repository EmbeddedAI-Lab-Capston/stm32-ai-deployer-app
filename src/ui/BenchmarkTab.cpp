#include "BenchmarkTab.h"

#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include "core/AppSettings.h"
#include "modules/flash/XCubeAIRunner.h"
#include "modules/serial/SerialManager.h"

BenchmarkTab::BenchmarkTab(AppState *state, SerialManager *serial, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_serial(serial)
{
    setupUi();

    connect(m_state, &AppState::activeBoardChanged,
            this, &BenchmarkTab::onBoardChanged);
    connect(m_state, &AppState::activePortChanged,
            this, &BenchmarkTab::onPortChanged);
    connect(m_state, &AppState::activeBaudChanged,
            this, &BenchmarkTab::onBaudChanged);
    connect(m_serial, &SerialManager::benchReceived,
            this, &BenchmarkTab::onBenchReceived);

    onBoardChanged(m_state->activeBoard());
    onPortChanged(m_state->activePort());
    onBaudChanged(m_state->activeBaud());
    resetMetrics();
    loadModelReportMetrics();
}

BenchmarkTab::~BenchmarkTab()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void BenchmarkTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    auto *targetBox = new QGroupBox(tr("Bağlı Kart"), this);
    auto *targetForm = new QFormLayout(targetBox);
    m_boardLabel = new QLabel("--", this);
    m_portLabel = new QLabel("--", this);
    targetForm->addRow(tr("Kart :"), m_boardLabel);
    targetForm->addRow(tr("Port :"), m_portLabel);
    mainLayout->addWidget(targetBox);

    auto *configBox = new QGroupBox(tr("Benchmark Ayarları"), this);
    auto *configForm = new QFormLayout(configBox);

    auto *modelRow = new QHBoxLayout;
    m_modelPathEdit = new QLineEdit(this);
    m_modelPathEdit->setReadOnly(true);
    m_modelPathEdit->setEnabled(false);
    m_modelPathEdit->setText(tr("Kartta yüklü model"));
    m_modelPathEdit->setPlaceholderText(tr("Benchmark için .tflite / .h5 / .onnx model seçin"));
    auto *browseBtn = new QPushButton(tr("Gözat"), this);
    browseBtn->setFixedWidth(80);
    browseBtn->setEnabled(false);
    modelRow->addWidget(m_modelPathEdit);
    modelRow->addWidget(browseBtn);

    m_batchSpin = new QSpinBox(this);
    m_batchSpin->setRange(1, 10000);
    m_batchSpin->setValue(20);

    m_rangeMinSpin = new QDoubleSpinBox(this);
    m_rangeMinSpin->setRange(-100000.0, 100000.0);
    m_rangeMinSpin->setDecimals(4);
    m_rangeMinSpin->setValue(0.0);

    m_rangeMaxSpin = new QDoubleSpinBox(this);
    m_rangeMaxSpin->setRange(-100000.0, 100000.0);
    m_rangeMaxSpin->setDecimals(4);
    m_rangeMaxSpin->setValue(1.0);

    auto *rangeRow = new QHBoxLayout;
    rangeRow->addWidget(m_rangeMinSpin);
    rangeRow->addWidget(new QLabel("→", this));
    rangeRow->addWidget(m_rangeMaxSpin);

    m_seedSpin = new QSpinBox(this);
    m_seedSpin->setRange(0, 2147483647);
    m_seedSpin->setValue(1234);

    m_saveCsvCheck = new QCheckBox(tr("IO verilerini CSV olarak kaydet"), this);
    m_saveCsvCheck->setVisible(false);

    configForm->addRow(tr("Model :"), modelRow);
    configForm->addRow(tr("Örnek sayısı :"), m_batchSpin);
    configForm->addRow(tr("Rastgele aralık :"), rangeRow);
    configForm->addRow(tr("Seed :"), m_seedSpin);
    configForm->addRow(QString(), m_saveCsvCheck);
    mainLayout->addWidget(configBox);

    auto *metricsBox = new QGroupBox(tr("Metrikler"), this);
    auto *metricsForm = new QFormLayout(metricsBox);
    m_modelLabel = new QLabel("--", this);
    m_inferenceLabel = new QLabel("--", this);
    m_ramLabel = new QLabel("--", this);
    m_flashLabel = new QLabel("--", this);
    m_maccLabel = new QLabel("--", this);
    m_statusLabel = new QLabel(tr("Hazır"), this);
    metricsForm->addRow(tr("Model :"), m_modelLabel);
    metricsForm->addRow(tr("Inference time :"), m_inferenceLabel);
    metricsForm->addRow(tr("RAM / Activations :"), m_ramLabel);
    metricsForm->addRow(tr("Flash / Weights :"), m_flashLabel);
    metricsForm->addRow(tr("MACC :"), m_maccLabel);
    metricsForm->addRow(tr("Durum :"), m_statusLabel);
    mainLayout->addWidget(metricsBox);

    auto *actionRow = new QHBoxLayout;
    m_runBtn = new QPushButton(tr("Benchmark Başlat"), this);
    m_runBtn->setObjectName("primaryButton");
    m_cancelBtn = new QPushButton(tr("İptal"), this);
    m_cancelBtn->setObjectName("dangerBtn");
    m_cancelBtn->setVisible(false);
    actionRow->addWidget(m_runBtn, 1);
    actionRow->addWidget(m_cancelBtn);
    mainLayout->addLayout(actionRow);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(180);
    mainLayout->addWidget(m_output, 1);

    connect(browseBtn, &QPushButton::clicked,
            this, &BenchmarkTab::onBrowseModelClicked);
    connect(m_runBtn, &QPushButton::clicked,
            this, &BenchmarkTab::onRunClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &BenchmarkTab::onCancelClicked);
}

QString BenchmarkTab::xcubeCliPath() const
{
    AppSettings settings;
    QString path = settings.xcubeAICliPath();
    if (path.isEmpty()) {
        path = XCubeAIRunner::detectCliPath();
        if (!path.isEmpty())
            settings.setXCubeAICliPath(path);
    }
    return path;
}

void BenchmarkTab::onBrowseModelClicked()
{
    AppSettings settings;
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Benchmark Modeli Seç"),
        settings.lastModelDir(),
        tr("AI Model (*.tflite *.h5 *.onnx);;TensorFlow Lite (*.tflite);;Keras (*.h5);;ONNX (*.onnx);;Tüm Dosyalar (*.*)"));
    if (path.isEmpty())
        return;

    settings.setLastModelDir(QFileInfo(path).absolutePath());
    m_modelPathEdit->setText(path);
}

void BenchmarkTab::onRunClicked()
{
    if (m_serial) {
        const QString port = m_state->activePort();
        if (port.isEmpty()) {
            m_statusLabel->setText(tr("Aktif COM port yok"));
            return;
        }
        appendLog(tr("UART bağlantısı açılıyor..."));
        m_statusLabel->setText(tr("Bağlanıyor..."));
        if (m_state->isConnected())
            m_serial->disconnectPort();
        m_serial->connectToPort(port, m_state->activeBaud());
        QTimer::singleShot(1200, this, &BenchmarkTab::sendBenchmarkCommand);
        return;
    }

    sendBenchmarkCommand();
}

void BenchmarkTab::startBenchmarkProcess()
{
    const QString modelPath = m_modelPathEdit->text().trimmed();
    if (modelPath.isEmpty() || !QFile::exists(modelPath)) {
        m_statusLabel->setText(tr("Model dosyası seçilmedi"));
        return;
    }

    const QString cliPath = xcubeCliPath();
    if (cliPath.isEmpty() || (QFileInfo(cliPath).isAbsolute() && !QFile::exists(cliPath))) {
        m_statusLabel->setText(tr("stedgeai bulunamadı"));
        return;
    }

    const QString port = m_state->activePort();
    if (port.isEmpty()) {
        m_statusLabel->setText(tr("Aktif COM port yok"));
        return;
    }

    if (m_process)
        m_process->deleteLater();
    m_process = new QProcess(this);

    const QString workspace = QDir::cleanPath(QDir::currentPath() + "/st_ai_benchmark_ws");
    const QString output = QDir::cleanPath(QDir::currentPath() + "/st_ai_benchmark_output");
    QDir().mkpath(workspace);
    QDir().mkpath(output);

    QStringList args = {
        QStringLiteral("validate"),
        QStringLiteral("--model"), modelPath,
        QStringLiteral("--target"), QStringLiteral("stm32"),
        QStringLiteral("--mode"), QStringLiteral("target"),
        QStringLiteral("--desc"),
        QString("serial:%1:%2").arg(port).arg(m_state->activeBaud()),
        QStringLiteral("--batch-size"), QString::number(m_batchSpin->value()),
        QStringLiteral("--range"),
        QString::number(m_rangeMinSpin->value(), 'g', 8),
        QString::number(m_rangeMaxSpin->value(), 'g', 8),
        QStringLiteral("--seed"), QString::number(m_seedSpin->value()),
        QStringLiteral("--workspace"), workspace,
        QStringLiteral("--output"), output,
        QStringLiteral("--no-check"),
    };
    if (m_saveCsvCheck->isChecked())
        args << QStringLiteral("--save-csv");

    resetMetrics();
    loadModelReportMetrics();
    m_output->clear();
    appendLog(QString("> %1 %2").arg(cliPath, args.join(' ')));
    m_statusLabel->setText(tr("Çalışıyor..."));
    m_progress->setVisible(true);
    m_runBtn->setEnabled(false);
    m_cancelBtn->setVisible(true);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &BenchmarkTab::onReadyRead);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &BenchmarkTab::onReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                onProcessFinished(exitCode);
            });
    m_process->start(cliPath, args);
}

void BenchmarkTab::sendBenchmarkCommand()
{
    if (!m_serial || !m_state->isConnected()) {
        m_statusLabel->setText(tr("UART bağlantısı yok"));
        return;
    }

    resetMetrics();
    m_output->clear();
    const int minMilli = qRound(m_rangeMinSpin->value() * 1000.0);
    const int maxMilli = qRound(m_rangeMaxSpin->value() * 1000.0);
    const QByteArray command = QString("BENCH %1 %2 %3 %4")
        .arg(m_batchSpin->value())
        .arg(minMilli)
        .arg(maxMilli)
        .arg(m_seedSpin->value())
        .toUtf8();

    appendLog("> " + QString::fromUtf8(command));
    m_statusLabel->setText(tr("Kart benchmark çalıştırıyor..."));
    m_progress->setVisible(true);
    m_runBtn->setEnabled(false);
    m_cancelBtn->setVisible(true);
    m_serial->writeLine(command);

    QTimer::singleShot(10000, this, [this]() {
        if (!m_runBtn->isEnabled()) {
            m_progress->setVisible(false);
            m_runBtn->setEnabled(true);
            m_cancelBtn->setVisible(false);
            m_statusLabel->setText(tr("Benchmark yanıtı alınamadı"));
            appendLog(tr("Not: Kart cevap vermediyse Pipeline Wizard ile firmware'i yeniden üretip flash edin. "
                         "Yeni firmware BENCH UART komutunu destekler."));
        }
    });
}

void BenchmarkTab::onCancelClicked()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
    m_progress->setVisible(false);
    m_runBtn->setEnabled(true);
    m_cancelBtn->setVisible(false);
    m_statusLabel->setText(tr("İptal edildi"));
}

void BenchmarkTab::onReadyRead()
{
    if (!m_process)
        return;
    const QString text = QString::fromLocal8Bit(m_process->readAllStandardOutput()
                                                + m_process->readAllStandardError());
    appendLog(text);
    parseMetrics(text);
}

void BenchmarkTab::onProcessFinished(int exitCode)
{
    if (m_process)
        onReadyRead();

    m_progress->setVisible(false);
    m_runBtn->setEnabled(true);
    m_cancelBtn->setVisible(false);

    const QString logText = m_output->toPlainText();
    if (logText.contains(QStringLiteral("Invalid firmware"), Qt::CaseInsensitive)) {
        m_statusLabel->setText(tr("Kartta AI validation firmware yok"));
        appendLog(tr("Not: stedgeai target validation, kartta ST.AI validation/AI runner firmware'i bekler. "
                     "Pipeline ile yüklenen normal inference firmware'i bu protokolü konuşmaz."));
    } else if (logText.contains(QStringLiteral("Access is denied"), Qt::CaseInsensitive)) {
        m_statusLabel->setText(tr("COM port başka işlem tarafından kullanılıyor"));
    } else {
        m_statusLabel->setText(exitCode == 0 ? tr("Tamamlandı") : tr("Başarısız"));
    }
}

void BenchmarkTab::appendLog(const QString &text)
{
    if (text.isEmpty())
        return;
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    if (!text.endsWith('\n'))
        m_output->insertPlainText("\n");
    m_output->moveCursor(QTextCursor::End);
}

void BenchmarkTab::parseMetrics(const QString &text)
{
    const auto setIf = [](QLabel *label, const QRegularExpression &re, const QString &text) {
        const auto match = re.match(text);
        if (match.hasMatch())
            label->setText(match.captured(1).trimmed());
    };

    setIf(m_maccLabel, QRegularExpression("macc\\s*:\\s*([^\\r\\n]+)", QRegularExpression::CaseInsensitiveOption), text);
    setIf(m_ramLabel, QRegularExpression("(?:activations|ram \\(total\\))\\s*:\\s*([^\\r\\n]+)", QRegularExpression::CaseInsensitiveOption), text);
    setIf(m_flashLabel, QRegularExpression("(?:weights|flash|rom)\\s*(?:\\([^)]*\\))?\\s*:\\s*([^\\r\\n]+)", QRegularExpression::CaseInsensitiveOption), text);
    setIf(m_inferenceLabel, QRegularExpression("(?:inference|duration|elapsed).*?:\\s*([^\\r\\n]*?(?:ms|us|s))", QRegularExpression::CaseInsensitiveOption), text);
}

void BenchmarkTab::loadModelReportMetrics()
{
    AppSettings settings;
    const QString reportPath = QDir(settings.lastOutputDir()).filePath(
        QStringLiteral("xcubeai_output/network_c_info.json"));

    QFile file(reportPath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    quint64 macc = 0;
    const QJsonArray graphs = root.value(QStringLiteral("graphs")).toArray();
    for (const QJsonValue &graphValue : graphs) {
        const QJsonArray nodes = graphValue.toObject()
            .value(QStringLiteral("nodes")).toArray();
        for (const QJsonValue &nodeValue : nodes)
            macc += static_cast<quint64>(nodeValue.toObject()
                .value(QStringLiteral("macc")).toInteger());
    }

    const QJsonArray tools = root.value(QStringLiteral("environment")).toObject()
        .value(QStringLiteral("tools")).toArray();
    if (!tools.isEmpty()) {
        const QJsonObject inputModel = tools.first().toObject()
            .value(QStringLiteral("input_model")).toObject();
        const QString modelName = inputModel.value(QStringLiteral("name")).toString();
        const qint64 weightsBytes = inputModel.value(QStringLiteral("size")).toInteger();

        if (!modelName.isEmpty() && m_modelLabel->text() == QStringLiteral("--"))
            m_modelLabel->setText(modelName);
        if (weightsBytes > 0)
            m_flashLabel->setText(QString("%1 B (%2 KiB) weights")
                                      .arg(weightsBytes)
                                      .arg(weightsBytes / 1024.0, 0, 'f', 2));
    }

    if (macc > 0)
        m_maccLabel->setText(QLocale().toString(macc));
}

void BenchmarkTab::resetMetrics()
{
    m_modelLabel->setText(m_state->lastModelName().isEmpty()
                              ? QStringLiteral("--")
                              : m_state->lastModelName());
    m_inferenceLabel->setText("--");
    m_ramLabel->setText("--");
    m_flashLabel->setText("--");
    m_maccLabel->setText("--");
}

void BenchmarkTab::onBenchReceived(const BenchData &data)
{
    m_progress->setVisible(false);
    m_runBtn->setEnabled(true);
    m_cancelBtn->setVisible(false);

    const QString modelName = data.model.isEmpty()
        ? m_state->lastModelName()
        : data.model;
    if (!modelName.isEmpty())
        m_modelLabel->setText(modelName);
    loadModelReportMetrics();

    m_inferenceLabel->setText(
        QString("avg %1 us | min %2 us | max %3 us")
            .arg(data.avg_us)
            .arg(data.min_us)
            .arg(data.max_us));
    m_ramLabel->setText(QString("%1 B activations, %2 B free")
                            .arg(data.ram_b)
                            .arg(data.free_ram_b));
    m_statusLabel->setText(tr("Tamamlandı"));
    appendLog(QString("BENCH result: model=%1 samples=%2 avg=%3us min=%4us max=%5us ram=%6B free=%7B label=%8 card=%9")
                  .arg(modelName.isEmpty() ? QStringLiteral("--") : modelName)
                  .arg(data.samples)
                  .arg(data.avg_us)
                  .arg(data.min_us)
                  .arg(data.max_us)
                  .arg(data.ram_b)
                  .arg(data.free_ram_b)
                  .arg(data.label, data.card));
}

void BenchmarkTab::onBoardChanged(const BoardInfo &board)
{
    m_boardLabel->setText(board.isNull() ? "--" : board.name);
}

void BenchmarkTab::onPortChanged(const QString &port)
{
    m_portLabel->setText(QString("%1 @ %2").arg(port.isEmpty() ? "--" : port)
                         .arg(m_state->activeBaud()));
}

void BenchmarkTab::onBaudChanged(qint32 baud)
{
    m_portLabel->setText(QString("%1 @ %2").arg(m_state->activePort().isEmpty()
                                                    ? "--"
                                                    : m_state->activePort())
                         .arg(baud));
}
