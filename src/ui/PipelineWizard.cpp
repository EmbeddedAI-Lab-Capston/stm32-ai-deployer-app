#include "PipelineWizard.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWizardPage>

#include "core/AppSettings.h"
#include "core/AppState.h"
#include "modules/board/BoardPresets.h"
#include "modules/flash/PipelineRunner.h"

static QString sdkPatternForBoard(const QString &board)
{
    if (board.contains("H7", Qt::CaseInsensitive))
        return QStringLiteral("STM32Cube_FW_H7_*");
    if (board.contains("N6", Qt::CaseInsensitive))
        return QStringLiteral("STM32Cube_FW_N6_*");
    return QStringLiteral("STM32Cube_FW_F4_*");
}

static QString sdkTokenForBoard(const QString &board)
{
    if (board.contains("H7", Qt::CaseInsensitive))
        return QStringLiteral("STM32Cube_FW_H7_");
    if (board.contains("N6", Qt::CaseInsensitive))
        return QStringLiteral("STM32Cube_FW_N6_");
    return QStringLiteral("STM32Cube_FW_F4_");
}

// ══════════════════════════════════════════════════════════════════════════
//  Page 1 — Model selection
// ══════════════════════════════════════════════════════════════════════════

class ModelPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit ModelPage(PipelineConfig *cfg, QWidget *parent = nullptr)
        : QWizardPage(parent), m_cfg(cfg)
    {
        setTitle(tr("Adım 1 — AI Modeli"));
        setSubTitle(tr("Flash atılacak .tflite veya .h5 modelini seçin."));

        auto *layout = new QVBoxLayout(this);

        // File picker row
        auto *fileRow  = new QHBoxLayout;
        m_pathEdit     = new QLineEdit(this);
        m_pathEdit->setPlaceholderText(tr("model.tflite veya model.h5"));
        m_pathEdit->setReadOnly(true);
        auto *browseBtn = new QPushButton(tr("Gözat"), this);
        browseBtn->setFixedWidth(80);
        fileRow->addWidget(m_pathEdit);
        fileRow->addWidget(browseBtn);
        layout->addLayout(fileRow);

        // Model info group
        auto *infoBox  = new QGroupBox(tr("Model Bilgisi"), this);
        auto *infoForm = new QFormLayout(infoBox);
        m_archLabel    = new QLabel(tr("--"), infoBox);
        infoForm->addRow(tr("Mimari:"), m_archLabel);
        layout->addWidget(infoBox);

        // Quantization
        auto *qLabel = new QLabel(tr("Quantization:"), this);
        m_quantCombo = new QComboBox(this);
        m_quantCombo->addItems({"INT8", "Float32", "Dynamic Q"});
        layout->addWidget(qLabel);
        layout->addWidget(m_quantCombo);
        layout->addStretch();

        connect(browseBtn, &QPushButton::clicked, this, &ModelPage::onBrowse);
    }

    bool isComplete() const override
    {
        return !m_pathEdit->text().isEmpty()
            && QFile::exists(m_pathEdit->text());
    }

#if 0
    void initializePage() override
    {
        const QString model = m_cfg->modelPath.toLower();
        if (model.contains("bme") || model.contains("basinc") ||
            model.contains("basınc") || model.contains("basınç") ||
            model.contains("nem") || model.contains("sicaklik") ||
            model.contains("sıcaklık")) {
            const int idx = m_sensorCombo->findData("BME280");
            if (idx >= 0)
                m_sensorCombo->setCurrentIndex(idx);
        } else if (model.contains("mpu") || model.contains("ivme") ||
                   model.contains("imu") || model.contains("har")) {
            const int idx = m_sensorCombo->findData("MPU6050");
            if (idx >= 0)
                m_sensorCombo->setCurrentIndex(idx);
        } else if (model.contains("mic") || model.contains("ses") ||
                   model.contains("audio") || model.contains("kws")) {
            const int idx = m_sensorCombo->findData("PDM_MIC");
            if (idx >= 0)
                m_sensorCombo->setCurrentIndex(idx);
        }
    }

#endif
    bool validatePage() override
    {
        m_cfg->modelPath     = m_pathEdit->text();
        m_cfg->modelName     = QFileInfo(m_pathEdit->text()).baseName();
        m_cfg->architecture  = m_archLabel->text();
        m_cfg->quantization  = m_quantCombo->currentText();
        return true;
    }

private slots:
    void onBrowse()
    {
        AppSettings s;
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Model Seç"),
            s.lastModelDir(),
            tr("AI Modeller (*.tflite *.h5 *.keras);;"
               "TFLite (*.tflite);;Keras (*.h5 *.keras);;Tüm Dosyalar (*)"));
        if (path.isEmpty()) return;

        m_pathEdit->setText(path);
        s.setLastModelDir(QFileInfo(path).absolutePath());

        // Infer architecture from filename
        const QString upper = QFileInfo(path).baseName().toUpper();
        if      (upper.contains("MLP"))  m_archLabel->setText("MLP");
        else if (upper.contains("CNN"))  m_archLabel->setText("1D CNN");
        else if (upper.contains("LSTM")) m_archLabel->setText("LSTM");
        else if (upper.contains("KWS"))  m_archLabel->setText("KWS / LSTM");
        else                             m_archLabel->setText(tr("Bilinmiyor"));

        emit completeChanged();
    }

private:
    PipelineConfig *m_cfg;
    QLineEdit      *m_pathEdit;
    QLabel         *m_archLabel;
    QComboBox      *m_quantCombo;
};

// ══════════════════════════════════════════════════════════════════════════
//  Page 2 — Sensor configuration
// ══════════════════════════════════════════════════════════════════════════

class SensorPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SensorPage(PipelineConfig *cfg, QWidget *parent = nullptr)
        : QWizardPage(parent), m_cfg(cfg)
    {
        setTitle(tr("Adım 2 — Sensör Konfigürasyonu"));
        setSubTitle(tr("Kullanılan sensörü ve STM32 pin bağlantılarını girin."));

        auto *form = new QFormLayout(this);

        // Sensor combo
        m_sensorCombo = new QComboBox(this);
        m_sensorCombo->addItem(tr("MPU-6050  (İvmeölçer — HAR)"), "MPU6050");
        m_sensorCombo->addItem(tr("BME280    (Sıcaklık/Basınç/Nem)"), "BME280");
        m_sensorCombo->addItem(tr("PDM Mikrofon  (Ses — KWS)"),  "PDM_MIC");
        form->addRow(tr("Sensör:"), m_sensorCombo);

        // ── I2C panel ──────────────────────────────────────────────────────
        m_i2cBox  = new QGroupBox(tr("I2C Konfigürasyonu"), this);
        auto *i2cForm = new QFormLayout(m_i2cBox);

        m_i2cInst = new QComboBox(m_i2cBox);
        m_i2cInst->addItems({"I2C1", "I2C2", "I2C3"});

        m_sdaPort = new QLineEdit("GPIOB", m_i2cBox);
        m_sdaPin  = new QLineEdit("GPIO_PIN_7", m_i2cBox);
        m_sclPort = new QLineEdit("GPIOB", m_i2cBox);
        m_sclPin  = new QLineEdit("GPIO_PIN_6", m_i2cBox);
        m_i2cAddr = new QLineEdit("0x76", m_i2cBox);

        i2cForm->addRow(tr("I2C Instance:"), m_i2cInst);
        i2cForm->addRow(tr("SDA Port:"),     m_sdaPort);
        i2cForm->addRow(tr("SDA Pin:"),      m_sdaPin);
        i2cForm->addRow(tr("SCL Port:"),     m_sclPort);
        i2cForm->addRow(tr("SCL Pin:"),      m_sclPin);
        i2cForm->addRow(tr("I2C Adres:"),    m_i2cAddr);
        form->addRow(m_i2cBox);

        // ── SAI panel ──────────────────────────────────────────────────────
        m_saiBox  = new QGroupBox(tr("SAI/PDM Konfigürasyonu"), this);
        auto *saiForm = new QFormLayout(m_saiBox);
        m_saiBox->setVisible(false);

        m_saiInst  = new QComboBox(m_saiBox);
        m_saiInst->addItems({"SAI1", "SAI2"});
        m_clkPort  = new QLineEdit("GPIOB", m_saiBox);
        m_clkPin   = new QLineEdit("GPIO_PIN_5", m_saiBox);
        m_dataPort = new QLineEdit("GPIOB", m_saiBox);
        m_dataPin  = new QLineEdit("GPIO_PIN_3", m_saiBox);

        saiForm->addRow(tr("SAI Instance:"), m_saiInst);
        saiForm->addRow(tr("CLK Port:"),     m_clkPort);
        saiForm->addRow(tr("CLK Pin:"),      m_clkPin);
        saiForm->addRow(tr("DATA Port:"),    m_dataPort);
        saiForm->addRow(tr("DATA Pin:"),     m_dataPin);
        form->addRow(m_saiBox);

        connect(m_sensorCombo, &QComboBox::currentIndexChanged,
                this, &SensorPage::onSensorChanged);
    }

    void initializePage() override
    {
        const QString model = m_cfg->modelPath.toLower();
        if (model.contains("bme") || model.contains("basinc") ||
            model.contains("nem") || model.contains("sicaklik")) {
            const int idx = m_sensorCombo->findData("BME280");
            if (idx >= 0)
                m_sensorCombo->setCurrentIndex(idx);
        } else if (model.contains("mpu") || model.contains("ivme") ||
                   model.contains("imu") || model.contains("har")) {
            const int idx = m_sensorCombo->findData("MPU6050");
            if (idx >= 0)
                m_sensorCombo->setCurrentIndex(idx);
        } else if (model.contains("mic") || model.contains("ses") ||
                   model.contains("audio") || model.contains("kws")) {
            const int idx = m_sensorCombo->findData("PDM_MIC");
            if (idx >= 0)
                m_sensorCombo->setCurrentIndex(idx);
        }
    }

    bool validatePage() override
    {
        const QString sensor = m_sensorCombo->currentData().toString();
        m_cfg->sensorType   = sensor;

        if (sensor == "PDM_MIC") {
            m_cfg->protocol    = "SAI";
            m_cfg->saiInstance = m_saiInst->currentText();
            m_cfg->clkPort     = m_clkPort->text();
            m_cfg->clkPin      = m_clkPin->text();
            m_cfg->dataPort    = m_dataPort->text();
            m_cfg->dataPin     = m_dataPin->text();
        } else {
            m_cfg->protocol    = "I2C";
            m_cfg->i2cInstance = m_i2cInst->currentText();
            m_cfg->sdaPort     = m_sdaPort->text();
            m_cfg->sdaPin      = m_sdaPin->text();
            m_cfg->sclPort     = m_sclPort->text();
            m_cfg->sclPin      = m_sclPin->text();
            m_cfg->i2cAddress  = m_i2cAddr->text();
        }
        return true;
    }

private slots:
    void onSensorChanged(int idx)
    {
        const QString sensor = m_sensorCombo->itemData(idx).toString();
        const bool isI2C = (sensor != "PDM_MIC");
        m_i2cBox->setVisible(isI2C);
        m_saiBox->setVisible(!isI2C);

        if (sensor == "MPU6050") {
            m_i2cAddr->setText("0xD0");
        } else if (sensor == "BME280") {
            m_i2cAddr->setText("0x76");
        }
    }

private:
    PipelineConfig *m_cfg;

    QComboBox *m_sensorCombo;

    QGroupBox *m_i2cBox;
    QComboBox *m_i2cInst;
    QLineEdit *m_sdaPort, *m_sdaPin, *m_sclPort, *m_sclPin, *m_i2cAddr;

    QGroupBox *m_saiBox;
    QComboBox *m_saiInst;
    QLineEdit *m_clkPort, *m_clkPin, *m_dataPort, *m_dataPin;
};

// ══════════════════════════════════════════════════════════════════════════
//  Page 3 — Summary & tool validation
// ══════════════════════════════════════════════════════════════════════════

class SummaryPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SummaryPage(PipelineConfig *cfg, QWidget *parent = nullptr)
        : QWizardPage(parent), m_cfg(cfg)
    {
        setTitle(tr("Adım 3 — Özet ve Doğrulama"));
        setSubTitle(tr("Pipeline başlamadan önce tüm bilgileri kontrol edin."));

        auto *layout = new QVBoxLayout(this);

        // Tool status group
        auto *toolBox  = new QGroupBox(tr("Araç Durumu"), this);
        auto *toolForm = new QFormLayout(toolBox);

        m_gccLabel       = new QLabel(this);
        m_makeLabel      = new QLabel(this);
        m_xcubeLabel     = new QLabel(this);
        m_progLabel      = new QLabel(this);
        m_sdkLabel       = new QLabel(this);

        m_gccLabel->setTextFormat(Qt::RichText);
        m_makeLabel->setTextFormat(Qt::RichText);
        m_xcubeLabel->setTextFormat(Qt::RichText);
        m_progLabel->setTextFormat(Qt::RichText);
        m_sdkLabel->setTextFormat(Qt::RichText);
        m_sdkLabel->setWordWrap(true);
        m_sdkBrowseBtn = new QPushButton(tr("Seç"), this);
        m_sdkBrowseBtn->setFixedWidth(60);
        auto *sdkRow = new QHBoxLayout;
        sdkRow->addWidget(m_sdkLabel, 1);
        sdkRow->addWidget(m_sdkBrowseBtn);

        toolForm->addRow("arm-none-eabi-gcc:", m_gccLabel);
        toolForm->addRow("make:",              m_makeLabel);
        toolForm->addRow("stedgeai:",          m_xcubeLabel);
        toolForm->addRow("STM32_Prog_CLI:",    m_progLabel);
        toolForm->addRow("STM32Cube SDK:",     sdkRow);
        layout->addWidget(toolBox);

        // Output directory
        auto *outRow = new QHBoxLayout;
        m_outEdit    = new QLineEdit(this);
        m_outEdit->setPlaceholderText(tr("Derleme çıktı klasörü..."));
        auto *outBtn = new QPushButton(tr("Seç"), this);
        outBtn->setFixedWidth(60);
        outRow->addWidget(m_outEdit);
        outRow->addWidget(outBtn);
        layout->addWidget(new QLabel(tr("Çıktı Klasörü:"), this));
        layout->addLayout(outRow);
        layout->addStretch();

        connect(outBtn, &QPushButton::clicked, this, &SummaryPage::onChooseOutDir);
        connect(m_sdkBrowseBtn, &QPushButton::clicked, this, &SummaryPage::onChooseSdkDir);
        connect(m_outEdit, &QLineEdit::textChanged,
                this, &SummaryPage::completeChanged);
    }

    void initializePage() override
    {
        AppSettings s;

        auto mkLabel = [](const QString &path) -> QString {
            if (path.isEmpty() || (!QFile::exists(path) && path != "make" &&
                                    path != "arm-none-eabi-gcc" &&
                                    path != "STM32_Programmer_CLI" &&
                                    path != "stedgeai")) {
                return QStringLiteral("<span style='color:#F38BA8;'>✗ Bulunamadı</span>");
            }
            const QString name = QFileInfo(path).fileName().isEmpty()
                                     ? path : QFileInfo(path).fileName();
            return QString("<span style='color:#A6E3A1;'>✓ %1</span>").arg(name);
        };

        m_gccLabel->setText(mkLabel(s.gccPath()));
        m_makeLabel->setText(mkLabel(s.makePath()));
        m_xcubeLabel->setText(mkLabel(s.xcubeAICliPath()));
        m_progLabel->setText(mkLabel(s.programmerCliPath()));

        // Detect STM32Cube SDK
        m_detectedSdkPath = s.cubeSdkPath();
        const QString expectedSdkPattern = sdkPatternForBoard(m_cfg->targetBoard);
        const QString expectedSdkToken = sdkTokenForBoard(m_cfg->targetBoard);
        if (!m_detectedSdkPath.isEmpty()
            && !QDir(m_detectedSdkPath).dirName().contains(expectedSdkToken, Qt::CaseInsensitive)) {
            m_detectedSdkPath.clear();
        }
        if (m_detectedSdkPath.isEmpty() || !QDir(m_detectedSdkPath).exists()) {
            const QString repoBase = QDir::homePath() + "/STM32Cube/Repository";
            const QDir repo(repoBase);
            if (repo.exists()) {
                const QStringList fw = repo.entryList(
                    QStringList{expectedSdkPattern},
                    QDir::Dirs, QDir::Name | QDir::Reversed);
                if (!fw.isEmpty())
                    m_detectedSdkPath = QDir::cleanPath(repoBase + "/" + fw.first());
            }
        }
        if (!m_detectedSdkPath.isEmpty() && QDir(m_detectedSdkPath).exists()) {
            m_sdkLabel->setText(
                QString("<span style='color:#A6E3A1;'>✓ %1</span>")
                    .arg(QDir(m_detectedSdkPath).dirName()));
        } else {
            m_sdkLabel->setText(
                QStringLiteral("<span style='color:#F38BA8;'>✗ Bulunamadı — STM32CubeMX ile ")
                + expectedSdkPattern.toHtmlEscaped()
                + QStringLiteral(" paketini indirin</span>"));
        }

        // Pre-fill output dir
        if (m_outEdit->text().isEmpty()) {
            const QString base = s.lastOutputDir();
            m_outEdit->setText(base);
        }
    }

    bool isComplete() const override
    {
        return !m_outEdit->text().trimmed().isEmpty();
    }

    bool validatePage() override
    {
        AppSettings s;
        m_cfg->gccPath           = s.gccPath();
        m_cfg->makePath          = s.makePath();
        m_cfg->xcubeCliPath      = s.xcubeAICliPath();
        m_cfg->programmerCliPath = s.programmerCliPath();
        m_cfg->cubeSdkPath       = m_detectedSdkPath;
        m_cfg->outputDir         = m_outEdit->text().trimmed();
        s.setLastOutputDir(m_cfg->outputDir);
        if (!m_cfg->cubeSdkPath.isEmpty())
            s.setCubeSdkPath(m_cfg->cubeSdkPath);
        return true;
    }

private slots:
    void onChooseOutDir()
    {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Çıktı Klasörü Seç"), m_outEdit->text());
        if (!dir.isEmpty()) m_outEdit->setText(dir);
    }

    void onChooseSdkDir()
    {
        const QString startDir = m_detectedSdkPath.isEmpty()
            ? QDir::homePath() + "/STM32Cube/Repository"
            : m_detectedSdkPath;
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("STM32Cube SDK Klasörü Seç"), startDir);
        if (dir.isEmpty())
            return;

        m_detectedSdkPath = QDir::cleanPath(dir);
        updateSdkLabel();
        emit completeChanged();
    }

private:
    void updateSdkLabel()
    {
        const QString expectedSdkPattern = sdkPatternForBoard(m_cfg->targetBoard);
        const QString expectedSdkToken = sdkTokenForBoard(m_cfg->targetBoard);

        if (!m_detectedSdkPath.isEmpty() && QDir(m_detectedSdkPath).exists()) {
            const QString folderName = QDir(m_detectedSdkPath).dirName();
            const bool boardMatches = folderName.contains(expectedSdkToken, Qt::CaseInsensitive);
            const QString color = boardMatches ? QStringLiteral("#A6E3A1")
                                               : QStringLiteral("#FAB387");
            const QString prefix = boardMatches ? QStringLiteral("✓ ")
                                                : QStringLiteral("! ");
            const QString suffix = boardMatches
                ? QString()
                : QStringLiteral(" — hedef kart için beklenen: ")
                    + expectedSdkPattern.toHtmlEscaped();

            m_sdkLabel->setText(
                QString("<span style='color:%1;'>%2%3%4</span>")
                    .arg(color,
                         prefix,
                         folderName.toHtmlEscaped(),
                         suffix));
            return;
        }

        m_sdkLabel->setText(
            QStringLiteral("<span style='color:#F38BA8;'>✗ Bulunamadı — STM32CubeMX ile ")
            + expectedSdkPattern.toHtmlEscaped()
            + QStringLiteral(" paketini indirin veya Seç ile klasörü gösterin</span>"));
    }

    PipelineConfig *m_cfg;
    QLabel    *m_gccLabel, *m_makeLabel, *m_xcubeLabel, *m_progLabel, *m_sdkLabel;
    QPushButton *m_sdkBrowseBtn;
    QLineEdit *m_outEdit;
    QString    m_detectedSdkPath;
};

// ══════════════════════════════════════════════════════════════════════════
//  Page 4 — Build & flash (pipeline execution)
// ══════════════════════════════════════════════════════════════════════════

class BuildPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit BuildPage(PipelineConfig *cfg, QWidget *parent = nullptr)
        : QWizardPage(parent), m_cfg(cfg)
    {
        setTitle(tr("Adım 4 — Pipeline Çalışıyor"));
        setSubTitle(tr("Model derleniyor ve karta yükleniyor..."));
        setFinalPage(true);

        auto *layout = new QVBoxLayout(this);

        m_stageLabel = new QLabel(tr("Hazırlanıyor..."), this);
        m_stageLabel->setObjectName("stageLabel");

        m_progress = new QProgressBar(this);
        m_progress->setRange(0, 100);
        m_progress->setValue(0);

        m_log = new QPlainTextEdit(this);
        m_log->setReadOnly(true);
        m_log->setFont(QFont("Consolas", 9));
        m_log->setMinimumHeight(200);

        m_resultLabel = new QLabel(this);
        m_resultLabel->setTextFormat(Qt::RichText);
        m_resultLabel->setVisible(false);

        layout->addWidget(m_stageLabel);
        layout->addWidget(m_progress);
        layout->addWidget(m_log);
        layout->addWidget(m_resultLabel);
    }

    void initializePage() override
    {
        m_finished  = false;
        m_succeeded = false;
        m_log->clear();
        m_progress->setValue(0);
        m_resultLabel->setVisible(false);
        m_stageLabel->setText(tr("Pipeline başlatılıyor..."));

        // Populate board from AppState if available (set before wizard opens)
        // Then kick off the runner
        startPipeline();
    }

    bool isComplete() const override { return m_finished; }

private:
    void startPipeline()
    {
        m_runner = new PipelineRunner(this);

        connect(m_runner, &PipelineRunner::stageChanged,
                m_stageLabel, &QLabel::setText);
        connect(m_runner, &PipelineRunner::progressChanged,
                m_progress, &QProgressBar::setValue);
        connect(m_runner, &PipelineRunner::outputLine,
                this, &BuildPage::appendOutput);
        connect(m_runner, &PipelineRunner::errorLine,
                this, &BuildPage::appendError);
        connect(m_runner, &PipelineRunner::finished,
                this, &BuildPage::onFinished);

        m_runner->run(*m_cfg);
    }

private slots:
    void appendOutput(const QString &line)
    {
        QString color = "#CDD6F4";
        if (line.startsWith("✓") || line.contains("tamamlandı", Qt::CaseInsensitive))
            color = "#A6E3A1";
        else if (line.startsWith("┌") || line.startsWith("│") || line.startsWith("└"))
            color = "#A6E3A1";

        m_log->appendHtml(
            QString("<span style='color:%1;'>%2</span>")
                .arg(color, line.toHtmlEscaped()));
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    }

    void appendError(const QString &line)
    {
        m_log->appendHtml(
            QString("<span style='color:#F38BA8;'>%1</span>")
                .arg(line.toHtmlEscaped()));
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
    }

    void onFinished(bool success)
    {
        m_finished  = true;
        m_succeeded = success;
        m_resultLabel->setVisible(true);

        if (success) {
            m_stageLabel->setText(tr("Pipeline tamamlandı!"));
            m_resultLabel->setText(
                "<span style='color:#A6E3A1; font-size:13px; font-weight:bold;'>"
                "✓ Model başarıyla derlendi ve karta yüklendi."
                "</span>");
        } else {
            m_stageLabel->setText(tr("Pipeline başarısız."));
            m_resultLabel->setText(
                "<span style='color:#F38BA8; font-size:13px;'>"
                "✗ İşlem başarısız. Yukarıdaki log çıktısını inceleyin."
                "</span>");
        }

        emit completeChanged();
    }

private:
    PipelineConfig  *m_cfg;
    PipelineRunner  *m_runner    = nullptr;
    bool             m_finished  = false;
    bool             m_succeeded = false;

    QLabel        *m_stageLabel;
    QProgressBar  *m_progress;
    QPlainTextEdit *m_log;
    QLabel        *m_resultLabel;
};

// ══════════════════════════════════════════════════════════════════════════
//  Q_OBJECT in .cpp — AUTOMOC will pick these up
// ══════════════════════════════════════════════════════════════════════════

#include "PipelineWizard.moc"

// ══════════════════════════════════════════════════════════════════════════
//  PipelineWizard
// ══════════════════════════════════════════════════════════════════════════

PipelineWizard::PipelineWizard(AppState *state, QWidget *parent)
    : QWizard(parent)
    , m_appState(state)
{
    setWindowTitle(tr("Pipeline Wizard — .tflite → Kart"));
    setMinimumSize(600, 500);
    setWizardStyle(QWizard::ModernStyle);

    setOption(QWizard::NoBackButtonOnLastPage);
    setOption(QWizard::NoCancelButtonOnLastPage);

    // Populate board from AppState
    if (m_appState && !m_appState->activeBoard().isNull()) {
        const BoardInfo normalized = BoardPresets::find(m_appState->activeBoard().name);
        m_config.targetBoard = normalized.isNull()
            ? m_appState->activeBoard().name
            : normalized.name;
    } else {
        m_config.targetBoard = "STM32F4";
    }

    setupPages();
}

void PipelineWizard::setupPages()
{
    addPage(new ModelPage(&m_config, this));
    addPage(new SensorPage(&m_config, this));
    addPage(new SummaryPage(&m_config, this));
    addPage(new BuildPage(&m_config, this));
}
