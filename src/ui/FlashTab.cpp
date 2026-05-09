#include "FlashTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QScrollBar>
#include <QScrollArea>
#include <QFrame>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

#include "core/AppSettings.h"
#include "core/AppState.h"
#include "SettingsDialog.h"
#include "PipelineWizard.h"

FlashTab::FlashTab(AppState *state, QWidget *parent)
    : QWidget(parent)
    , m_appState(state)
    , m_xcubeRunner(new XCubeAIRunner(this))
{
    setupUi();

    connect(m_appState, &AppState::activeBoardChanged,
            this, &FlashTab::onBoardChanged);
    connect(m_appState, &AppState::connectionChanged,
            this, &FlashTab::onConnectionChanged);

    onBoardChanged(m_appState->activeBoard());
    onConnectionChanged(m_appState->isConnected(), m_appState->connectionInfo());
}

FlashTab::~FlashTab() = default;

void FlashTab::initialize(FlashManager *manager)
{
    m_flashManager = manager;

    connect(m_flashManager, &FlashManager::outputLine,
            this, &FlashTab::appendOutputLine);
    connect(m_flashManager, &FlashManager::errorLine,
            this, &FlashTab::appendErrorLine);
    connect(m_flashManager, &FlashManager::progressChanged,
            m_progressBar, &QProgressBar::setValue);

    connect(m_flashManager, &FlashManager::flashStarted,
            this, [this](const FlashConfig &) {
                m_flashBtn->setEnabled(false);
                m_flashBtn->setText(tr("Yükleniyor..."));
                m_cancelBtn->setVisible(true);
                m_progressBar->setValue(0);
                m_progressBar->setVisible(true);
                m_outputEdit->clear();
            });

    connect(m_flashManager, &FlashManager::flashFinished,
            this, [this](bool success, const FlashConfig &config) {
                m_flashBtn->setEnabled(true);
                m_flashBtn->setText(tr("  Karta Yükle (Flash)  "));
                m_cancelBtn->setVisible(false);

                if (success) {
                    m_progressBar->setValue(100);
                    showSuccessBanner(config);
                    // TODO(asama-5): Oturumu SQLite'a kaydet
                } else {
                    m_progressBar->setVisible(false);
                }
            });

    refreshCliStatus();
    refreshXCubeAIStatus();
}

void FlashTab::refreshCliStatus()
{
    AppSettings settings;
    QString path = settings.programmerCliPath();

    if (path.isEmpty() || !QFile::exists(path)) {
        const QString detected = FlashManager::detectCliPath();
        if (!detected.isEmpty()) {
            settings.setProgrammerCliPath(detected);
            path = detected;
        }
    }

    if (m_flashManager)
        m_flashManager->setCliPath(path);

    if (!path.isEmpty() && QFile::exists(path)) {
        m_cliStatusLabel->setText(tr("✓ CLI hazır"));
        m_cliStatusLabel->setStyleSheet("color: #A6E3A1;");
    } else {
        m_cliStatusLabel->setText(tr("⚠ CLI bulunamadı — Ayarlar'dan yolu girin"));
        m_cliStatusLabel->setStyleSheet("color: #F9E2AF;");
    }
}

void FlashTab::refreshXCubeAIStatus()
{
    AppSettings settings;
    QString path = settings.xcubeAICliPath();

    if (path.isEmpty()) {
        path = XCubeAIRunner::detectCliPath();
        if (!path.isEmpty())
            settings.setXCubeAICliPath(path);
    }

    m_xcubeRunner->setCliPath(path);

    const bool available = !path.isEmpty() &&
                           (path == "stm32ai" || QFile::exists(path));

    if (available) {
        m_xcubeStatusLabel->setText(tr("✓ stedgeai hazır"));
        m_xcubeStatusLabel->setStyleSheet("color: #A6E3A1;");
        m_modelRadio->setEnabled(true);
    } else {
        m_xcubeStatusLabel->setText(
            tr("⚠ stedgeai bulunamadı — Ayarlar menüsünden yolu girin"));
        m_xcubeStatusLabel->setStyleSheet("color: #F9E2AF;");
        // Keep AI option accessible so the user can still try after configuring
    }
}

// ── setupUi ────────────────────────────────────────────────────────────────

void FlashTab::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    rootLayout->addWidget(scrollArea);

    auto *contentWidget = new QWidget(scrollArea);
    scrollArea->setWidget(contentWidget);

    auto *mainLayout = new QVBoxLayout(contentWidget);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // ── Source selection ───────────────────────────────────────────────────
    auto *sourceBox    = new QGroupBox(tr("Firmware Kaynağı"), this);
    auto *sourceLayout = new QVBoxLayout(sourceBox);
    sourceLayout->setSpacing(6);

    m_hexRadio   = new QRadioButton(tr("Hazır Firmware (.hex / .bin)"), this);
    m_modelRadio = new QRadioButton(tr("AI Modelinden Üret (.tflite / .h5)"), this);
    m_hexRadio->setChecked(true);

    auto *srcGroup = new QButtonGroup(this);
    srcGroup->addButton(m_hexRadio,   0);
    srcGroup->addButton(m_modelRadio, 1);

    sourceLayout->addWidget(m_hexRadio);
    sourceLayout->addWidget(m_modelRadio);

    // Pipeline Wizard — end-to-end .tflite → compile → flash
    m_wizardBtn = new QPushButton(
        tr("   Pipeline Wizard  (.tflite  → Derle  → Flash)"), this);
    m_wizardBtn->setObjectName("primaryButton");
    m_wizardBtn->setMinimumHeight(36);
    m_wizardBtn->setToolTip(
        tr("Model, sensör ve pin bilgilerini girerek\n"
           "otomatik derleme ve flash pipeline'ını başlatır."));
    sourceLayout->addWidget(m_wizardBtn);

    mainLayout->addWidget(sourceBox);

    connect(srcGroup, &QButtonGroup::idToggled,
            this, &FlashTab::onSourceModeChanged);

    // ── Hex firmware panel ─────────────────────────────────────────────────
    m_hexPanel = new QWidget(this);
    auto *hexPanelLayout = new QVBoxLayout(m_hexPanel);
    hexPanelLayout->setContentsMargins(0, 0, 0, 0);
    hexPanelLayout->setSpacing(8);

    // CLI status row
    auto *cliRow    = new QWidget(m_hexPanel);
    auto *cliLayout = new QHBoxLayout(cliRow);
    cliLayout->setContentsMargins(4, 0, 4, 0);
    cliLayout->setSpacing(8);

    m_cliStatusLabel = new QLabel(tr("● CLI durumu kontrol ediliyor..."), this);
    m_cliStatusLabel->setStyleSheet("color: #6C7086;");

    auto *settingsBtn = new QPushButton(tr("Ayarlar"), m_hexPanel);
    settingsBtn->setFixedWidth(80);

    cliLayout->addWidget(m_cliStatusLabel, 1);
    cliLayout->addWidget(settingsBtn);
    hexPanelLayout->addWidget(cliRow);

    // Model info form
    auto *modelBox  = new QGroupBox(tr("Model Bilgileri"), m_hexPanel);
    auto *modelForm = new QFormLayout(modelBox);
    modelForm->setSpacing(10);
    modelForm->setHorizontalSpacing(12);
    modelForm->setRowWrapPolicy(QFormLayout::DontWrapRows);
    modelForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    modelForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_modelNameEdit = new QLineEdit(m_hexPanel);
    m_modelNameEdit->setPlaceholderText(tr("örn. MLP_INT8_HAR"));

    m_archCombo = new QComboBox(m_hexPanel);
    m_archCombo->addItems({"MLP", "1D CNN", "LSTM", "TC-ResNet", tr("Özel")});

    m_quantCombo = new QComboBox(m_hexPanel);
    m_quantCombo->addItems({"INT8", "Dynamic Q", "Float32"});

    auto *fileRow    = new QWidget(m_hexPanel);
    auto *fileLayout = new QHBoxLayout(fileRow);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->setSpacing(6);

    m_filePathEdit = new QLineEdit(m_hexPanel);
    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setPlaceholderText(tr("Firmware dosyası seçin (.hex / .bin)"));

    auto *browseBtn = new QPushButton(tr("Gözat"), m_hexPanel);
    browseBtn->setFixedWidth(80);

    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(browseBtn);

    // File info label — shown after a file is selected
    m_fileInfoLabel = new QLabel(m_hexPanel);
    m_fileInfoLabel->setTextFormat(Qt::RichText);
    m_fileInfoLabel->setVisible(false);

    m_simModeCheck = new QCheckBox(
        tr("Simülasyon Modu  (gerçek flash atılmaz)"), m_hexPanel);
    m_simModeCheck->setChecked(true);

    // Format hint note
    auto *formatNote = new QLabel(
        "<small style='color:#6C7086;'>"
        "Desteklenen: "
        "<span style='color:#A6E3A1;'>.hex</span> · "
        "<span style='color:#89B4FA;'>.elf</span> · "
        "<span style='color:#F9E2AF;'>.bin</span>"
        "&nbsp;&nbsp;|&nbsp;&nbsp;"
        "STM32CubeIDE çıktısı genellikle <b>.elf</b> formatındadır."
        "</small>",
        m_hexPanel);
    formatNote->setTextFormat(Qt::RichText);

    modelForm->addRow(tr("Model Adı    :"), m_modelNameEdit);
    modelForm->addRow(tr("Mimari       :"), m_archCombo);
    modelForm->addRow(tr("Quantization :"), m_quantCombo);
    modelForm->addRow(tr("Firmware     :"), fileRow);
    modelForm->addRow(QString(), m_fileInfoLabel);
    modelForm->addRow(QString(), formatNote);
    modelForm->addRow(QString(), m_simModeCheck);

    hexPanelLayout->addWidget(modelBox);

    // Flash action row
    auto *hexActionRow    = new QWidget(m_hexPanel);
    auto *hexActionLayout = new QHBoxLayout(hexActionRow);
    hexActionLayout->setContentsMargins(0, 0, 0, 0);
    hexActionLayout->setSpacing(8);

    m_flashBtn = new QPushButton(tr("  Karta Yükle (Flash)  "), m_hexPanel);
    m_flashBtn->setObjectName("primaryButton");
    m_flashBtn->setMinimumHeight(44);

    m_cancelBtn = new QPushButton(tr("İptal"), m_hexPanel);
    m_cancelBtn->setObjectName("dangerBtn");
    m_cancelBtn->setFixedWidth(80);
    m_cancelBtn->setVisible(false);

    hexActionLayout->addWidget(m_flashBtn, 1);
    hexActionLayout->addWidget(m_cancelBtn);
    hexPanelLayout->addWidget(hexActionRow);

    mainLayout->addWidget(m_hexPanel);

    // ── AI model panel ─────────────────────────────────────────────────────
    m_aiPanel = new QWidget(this);
    m_aiPanel->setVisible(false);
    auto *aiPanelLayout = new QVBoxLayout(m_aiPanel);
    aiPanelLayout->setContentsMargins(0, 0, 0, 0);
    aiPanelLayout->setSpacing(8);

    auto *aiBox  = new QGroupBox(tr("AI Model Bilgileri"), m_aiPanel);
    auto *aiForm = new QFormLayout(aiBox);
    aiForm->setSpacing(10);
    aiForm->setHorizontalSpacing(12);
    aiForm->setRowWrapPolicy(QFormLayout::DontWrapRows);
    aiForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    aiForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Model file
    auto *aiFileRow    = new QWidget(m_aiPanel);
    auto *aiFileLayout = new QHBoxLayout(aiFileRow);
    aiFileLayout->setContentsMargins(0, 0, 0, 0);
    aiFileLayout->setSpacing(6);

    m_aiModelPathEdit = new QLineEdit(m_aiPanel);
    m_aiModelPathEdit->setReadOnly(true);
    m_aiModelPathEdit->setPlaceholderText(tr("model.tflite veya model.h5"));

    auto *aiModelBrowseBtn = new QPushButton(tr("Gözat"), m_aiPanel);
    aiModelBrowseBtn->setFixedWidth(80);

    aiFileLayout->addWidget(m_aiModelPathEdit);
    aiFileLayout->addWidget(aiModelBrowseBtn);

    // Quantization
    m_aiQuantCombo = new QComboBox(m_aiPanel);
    m_aiQuantCombo->addItems({"Float32", "INT8", "Dynamic Q"});
    m_aiQuantCombo->setCurrentText("INT8");

    // Output directory
    auto *aiOutRow    = new QWidget(m_aiPanel);
    auto *aiOutLayout = new QHBoxLayout(aiOutRow);
    aiOutLayout->setContentsMargins(0, 0, 0, 0);
    aiOutLayout->setSpacing(6);

    m_aiOutputDirEdit = new QLineEdit(m_aiPanel);
    m_aiOutputDirEdit->setPlaceholderText(tr("Boş bırakılırsa otomatik oluşturulur"));

    auto *aiOutDirBtn = new QPushButton(tr("Seç"), m_aiPanel);
    aiOutDirBtn->setFixedWidth(80);

    aiOutLayout->addWidget(m_aiOutputDirEdit);
    aiOutLayout->addWidget(aiOutDirBtn);

    // X-CUBE-AI CLI status
    m_xcubeStatusLabel = new QLabel(tr("● stedgeai durumu kontrol ediliyor..."), this);
    m_xcubeStatusLabel->setStyleSheet("color: #6C7086;");

    aiForm->addRow(tr("Model Dosyası :"), aiFileRow);
    aiForm->addRow(tr("Quantization  :"), m_aiQuantCombo);
    aiForm->addRow(tr("Çıktı Klasörü :"), aiOutRow);
    aiForm->addRow(tr("CLI Durumu    :"), m_xcubeStatusLabel);

    aiPanelLayout->addWidget(aiBox);

    // Generate action row
    auto *genActionRow    = new QWidget(m_aiPanel);
    auto *genActionLayout = new QHBoxLayout(genActionRow);
    genActionLayout->setContentsMargins(0, 0, 0, 0);
    genActionLayout->setSpacing(8);

    m_generateBtn = new QPushButton(tr("  X-CUBE-AI ile C Kodu Üret  "), m_aiPanel);
    m_generateBtn->setObjectName("primaryButton");
    m_generateBtn->setMinimumHeight(44);

    m_aiCancelBtn = new QPushButton(tr("İptal"), m_aiPanel);
    m_aiCancelBtn->setObjectName("dangerBtn");
    m_aiCancelBtn->setFixedWidth(80);
    m_aiCancelBtn->setVisible(false);

    genActionLayout->addWidget(m_generateBtn, 1);
    genActionLayout->addWidget(m_aiCancelBtn);
    aiPanelLayout->addWidget(genActionRow);

    mainLayout->addWidget(m_aiPanel);

    // ── Shared: status row ─────────────────────────────────────────────────
    auto *statusRow    = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(4, 0, 4, 0);

    m_targetLabel = new QLabel(tr("Hedef Kart:  <b>STM32F4</b>"), this);
    m_targetLabel->setObjectName("flashStatusLabel");

    m_stlinkLabel = new QLabel(tr("●  ST-Link: Bilinmiyor"), this);
    m_stlinkLabel->setObjectName("stlinkDisconnected");

    statusLayout->addWidget(m_targetLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_stlinkLabel);
    mainLayout->addWidget(statusRow);

    // ── Shared: progress bar ───────────────────────────────────────────────
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // ── Shared: output area ────────────────────────────────────────────────
    auto *outputBox    = new QGroupBox(tr("Çıktı Terminali"), this);
    auto *outputLayout = new QVBoxLayout(outputBox);

    auto *outputToolRow    = new QWidget(outputBox);
    auto *outputToolLayout = new QHBoxLayout(outputToolRow);
    outputToolLayout->setContentsMargins(0, 0, 0, 4);

    auto *clearOutputBtn = new QPushButton(tr("Temizle"), outputBox);
    clearOutputBtn->setFixedWidth(80);
    outputToolLayout->addStretch();
    outputToolLayout->addWidget(clearOutputBtn);
    outputLayout->addWidget(outputToolRow);

    m_outputEdit = new QTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setObjectName("flashOutput");
    m_outputEdit->setMinimumHeight(180);
    m_outputEdit->append(
        "<span style='color:#6C7086;'>"
        "[Hazır — işlem başlatmak için butona basın]"
        "</span>");
    outputLayout->addWidget(m_outputEdit);

    // Result buttons row (hidden until generation succeeds)
    auto *resultRow    = new QWidget(outputBox);
    auto *resultLayout = new QHBoxLayout(resultRow);
    resultLayout->setContentsMargins(0, 4, 0, 0);
    resultLayout->setSpacing(8);

    m_openDirBtn = new QPushButton(tr("Klasörü Aç"), resultRow);
    m_openDirBtn->setVisible(false);

    m_nextStepBtn = new QPushButton(tr("Sonraki Adımı Göster"), resultRow);
    m_nextStepBtn->setVisible(false);

    resultLayout->addWidget(m_openDirBtn);
    resultLayout->addWidget(m_nextStepBtn);
    resultLayout->addStretch();
    outputLayout->addWidget(resultRow);

    mainLayout->addWidget(outputBox, 1);

    // ── Signal connections ─────────────────────────────────────────────────
    connect(m_wizardBtn,       &QPushButton::clicked, this, &FlashTab::onPipelineWizardClicked);
    connect(browseBtn,         &QPushButton::clicked, this, &FlashTab::onBrowseClicked);
    connect(m_flashBtn,        &QPushButton::clicked, this, &FlashTab::onFlashClicked);
    connect(m_cancelBtn,       &QPushButton::clicked, this, &FlashTab::onCancelClicked);
    connect(settingsBtn,       &QPushButton::clicked, this, &FlashTab::onSettingsClicked);
    connect(clearOutputBtn,    &QPushButton::clicked, this, &FlashTab::onClearOutputClicked);

    connect(aiModelBrowseBtn,  &QPushButton::clicked, this, &FlashTab::onAIModelBrowseClicked);
    connect(aiOutDirBtn,       &QPushButton::clicked, this, &FlashTab::onAIOutputDirClicked);
    connect(m_generateBtn,     &QPushButton::clicked, this, &FlashTab::onGenerateClicked);
    connect(m_aiCancelBtn,     &QPushButton::clicked, this, [this]() { m_xcubeRunner->cancel(); });

    // XCubeAIRunner signals
    connect(m_xcubeRunner, &XCubeAIRunner::outputLine,
            this, &FlashTab::appendOutputLine);
    connect(m_xcubeRunner, &XCubeAIRunner::errorLine,
            this, &FlashTab::appendErrorLine);
    connect(m_xcubeRunner, &XCubeAIRunner::progressChanged,
            m_progressBar, &QProgressBar::setValue);

    connect(m_xcubeRunner, &XCubeAIRunner::started,
            this, [this]() {
                m_generateBtn->setEnabled(false);
                m_generateBtn->setText(tr("Üretiliyor..."));
                m_aiCancelBtn->setVisible(true);
                m_progressBar->setValue(0);
                m_progressBar->setVisible(true);
                m_outputEdit->clear();
                m_openDirBtn->setVisible(false);
                m_nextStepBtn->setVisible(false);
            });

    connect(m_xcubeRunner, &XCubeAIRunner::finished,
            this, [this](const XCubeAIResult &result) {
                m_generateBtn->setEnabled(true);
                m_generateBtn->setText(tr("  X-CUBE-AI ile C Kodu Üret  "));
                m_aiCancelBtn->setVisible(false);

                if (result.success) {
                    m_progressBar->setValue(100);
                    showGenerationResult(result);
                } else {
                    m_progressBar->setVisible(false);
                    appendErrorLine(tr("✗ C kodu üretilemedi."));
                    if (!result.errorMessage.isEmpty())
                        appendErrorLine(result.errorMessage);
                }
            });
}

// ── Slots: source mode ─────────────────────────────────────────────────────

void FlashTab::onSourceModeChanged(int id, bool checked)
{
    if (!checked) return;
    m_hexPanel->setVisible(id == 0);
    m_aiPanel->setVisible(id == 1);
}

void FlashTab::onPipelineWizardClicked()
{
    auto *wizard = new PipelineWizard(m_appState, this);
    wizard->setAttribute(Qt::WA_DeleteOnClose);
    wizard->exec();
}

// ── Slots: hex firmware panel ──────────────────────────────────────────────

void FlashTab::onBrowseClicked()
{
    AppSettings settings;
    const QString lastDir = settings.lastFirmwareDir();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Firmware Dosyası Seç"),
        lastDir,
        tr("Firmware Files (*.hex *.bin *.elf);;"
           "Intel HEX (*.hex);;"
           "ELF (*.elf);;"
           "Binary (*.bin);;"
           "All Files (*)"));

    if (!path.isEmpty()) {
        settings.setLastFirmwareDir(QFileInfo(path).absolutePath());
        onHexFileSelected(path);
    }
}

void FlashTab::onHexFileSelected(const QString &path)
{
    m_filePathEdit->setText(path);

    const QFileInfo fi(path);
    const QString ext  = fi.suffix().toUpper();
    const QString size = QString::number(fi.size() / 1024.0, 'f', 1) + " KB";

    QString color, note;
    if (ext == "HEX") {
        color = "#A6E3A1";
        note  = tr("Intel HEX — adres bilgisi içeriyor");
    } else if (ext == "ELF") {
        color = "#89B4FA";
        note  = tr("ELF — adres ve sembol bilgisi içeriyor");
    } else if (ext == "BIN") {
        color = "#F9E2AF";
        note  = tr("Binary — flash adresi 0x08000000 kullanılacak");
    } else {
        color = "#CDD6F4";
        note  = tr("Bilinmeyen format");
    }

    m_fileInfoLabel->setText(
        QString("<span style='color:%1;'>%2 · %3 · %4</span>")
            .arg(color, ext, size, note));
    m_fileInfoLabel->setVisible(true);
}

void FlashTab::onFlashClicked()
{
    if (!m_flashManager) return;

    FlashConfig config;
    config.modelName      = m_modelNameEdit->text().trimmed();
    config.architecture   = m_archCombo->currentText();
    config.quantization   = m_quantCombo->currentText();
    config.hexPath        = m_filePathEdit->text().trimmed();
    config.simulationMode = m_simModeCheck->isChecked();
    config.targetBoard    = m_appState->activeBoard().isNull()
                                ? "STM32F4"
                                : m_appState->activeBoard().name;

    if (config.simulationMode && config.modelName.isEmpty())
        config.modelName = m_archCombo->currentText() + "_INT8";

    m_flashManager->flash(config);
}

void FlashTab::onCancelClicked()
{
    if (m_flashManager)
        m_flashManager->cancel();
}

void FlashTab::onSettingsClicked()
{
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        refreshCliStatus();
        refreshXCubeAIStatus();
    }
}

void FlashTab::onClearOutputClicked()
{
    m_outputEdit->clear();
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_openDirBtn->setVisible(false);
    m_nextStepBtn->setVisible(false);
}

// ── Slots: AI model panel ──────────────────────────────────────────────────

void FlashTab::onAIModelBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("AI Model Seç"),
        QDir::homePath(),
        tr("AI Modeller (*.tflite *.h5 *.keras);;"
           "TFLite (*.tflite);;"
           "Keras (*.h5 *.keras);;Tüm Dosyalar (*)"));
    if (!path.isEmpty())
        m_aiModelPathEdit->setText(path);
}

void FlashTab::onAIOutputDirClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Çıktı Klasörü Seç"));
    if (!dir.isEmpty())
        m_aiOutputDirEdit->setText(dir);
}

void FlashTab::onGenerateClicked()
{
    const QString modelPath = m_aiModelPathEdit->text().trimmed();

    if (modelPath.isEmpty()) {
        appendErrorLine(tr("Model dosyası seçilmedi."));
        return;
    }
    if (!QFile::exists(modelPath)) {
        appendErrorLine(tr("Dosya bulunamadı: ") + modelPath);
        return;
    }

    QString outputDir = m_aiOutputDirEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        outputDir = QFileInfo(modelPath).absolutePath() + "/xcubeai_output";
        m_aiOutputDirEdit->setText(outputDir);
    }

    const QString board = m_appState->activeBoard().isNull()
                              ? "STM32F4"
                              : m_appState->activeBoard().name;
    const QString quant = m_aiQuantCombo->currentText();

    m_xcubeRunner->generate(modelPath, board, quant, outputDir);
}

// ── Generation result ──────────────────────────────────────────────────────

void FlashTab::showGenerationResult(const XCubeAIResult &result)
{
    const QString fileList = result.generatedFiles.isEmpty()
                                 ? tr("(dosya listesi alınamadı)")
                                 : result.generatedFiles.join(", ");

    const QString banner =
        QString(
            "┌─────────────────────────────────────────┐\n"
            "│  ✓ C KODU ÜRETİLDİ                     │\n"
            "│                                         │\n"
            "│  Dosyalar : %1\n"
            "│                                         │\n"
            "│  Flash    : %2 KB                       │\n"
            "│  RAM      : %3 KB                       │\n"
            "│  MACC     : %4                          │\n"
            "│                                         │\n"
            "│  Sonraki adım: STM32CubeIDE'de derle   │\n"
            "└─────────────────────────────────────────┘")
            .arg(fileList.leftJustified(31))
            .arg(result.flashKb)
            .arg(result.ramKb, 0, 'f', 1)
            .arg(result.macc);

    appendOutputLine(banner);

    // Show result action buttons
    m_openDirBtn->setVisible(true);
    m_openDirBtn->disconnect();
    connect(m_openDirBtn, &QPushButton::clicked,
            this, [result]() {
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(result.outputDir));
            });

    m_nextStepBtn->setVisible(true);
    m_nextStepBtn->disconnect();
    connect(m_nextStepBtn, &QPushButton::clicked,
            this, &FlashTab::showNextStepDialog);
}

void FlashTab::showNextStepDialog()
{
    QMessageBox dlg(this);
    dlg.setWindowTitle(tr("Sonraki Adım — CubeIDE ile Derleme"));
    dlg.setIcon(QMessageBox::Information);
    dlg.setText(
        tr("<b>C kodu hazır. Şimdi ne yapmalısınız?</b><br><br>"
           "1. STM32CubeIDE'yi açın<br>"
           "2. Projenize <b>network.c</b> ve <b>network.h</b> "
           "dosyalarını ekleyin<br>"
           "3. X-CUBE-AI middleware'ini projeye dahil edin<br>"
           "4. <b>Release</b> modda derleyin<br>"
           "5. Üretilen <b>.hex</b> dosyasını bu uygulamaya "
           "geri getirin<br><br>"
           "Derleme tamamlandığında:<br>"
           "Kaynak olarak <b>'Hazır Firmware'</b> seçin "
           "ve .hex dosyasını yükleyin."));
    dlg.setStandardButtons(QMessageBox::Ok);
    dlg.exec();
}

// ── Output helpers ─────────────────────────────────────────────────────────

void FlashTab::appendOutputLine(const QString &line)
{
    QString color = "#CDD6F4";

    if (line.contains("complete", Qt::CaseInsensitive)   ||
        line.contains("successful", Qt::CaseInsensitive) ||
        line.contains("verified", Qt::CaseInsensitive)   ||
        line.contains("Done", Qt::CaseInsensitive)       ||
        line.startsWith("✓")) {
        color = "#A6E3A1";
    } else if (line.contains("Error", Qt::CaseSensitive)   ||
               line.contains("failed", Qt::CaseInsensitive) ||
               line.startsWith("✗")) {
        color = "#F38BA8";
    } else if (line.contains("Warning", Qt::CaseSensitive)) {
        color = "#F9E2AF";
    } else if (line.startsWith(">")) {
        color = "#89B4FA";
    } else if (line.contains("%") || line.contains("Progress")) {
        color = "#CBA6F7";
    }

    m_outputEdit->append(
        QString("<span style='color:%1;'>%2</span>")
            .arg(color, line.toHtmlEscaped()));

    m_outputEdit->verticalScrollBar()->setValue(
        m_outputEdit->verticalScrollBar()->maximum());
}

void FlashTab::appendErrorLine(const QString &line)
{
    m_outputEdit->append(
        QString("<span style='color:#F38BA8;'>%1</span>")
            .arg(line.toHtmlEscaped()));
    m_outputEdit->verticalScrollBar()->setValue(
        m_outputEdit->verticalScrollBar()->maximum());
}

void FlashTab::showSuccessBanner(const FlashConfig &config)
{
    const QString banner =
        QString(
            "┌─────────────────────────────────────┐\n"
            "│  ✓  FLASH BAŞARILI                  │\n"
            "│  Model : %1│\n"
            "│  Kart  : %2│\n"
            "│  Tip   : %3 / %4\n"
            "└─────────────────────────────────────┘")
            .arg(config.modelName.leftJustified(22),
                 config.targetBoard.leftJustified(21),
                 config.architecture,
                 config.quantization);

    appendOutputLine(banner);
}

// ── AppState reactions ─────────────────────────────────────────────────────

void FlashTab::onBoardChanged(const BoardInfo &board)
{
    if (board.isNull()) return;
    m_targetLabel->setText(
        QString("Hedef Kart:  <b>%1</b>").arg(board.name));
}

void FlashTab::onConnectionChanged(bool connected, const QString &/*info*/)
{
    if (connected) {
        m_stlinkLabel->setText(tr("●  ST-Link: Bağlı"));
        m_stlinkLabel->setStyleSheet("color: #A6E3A1;");
    } else {
        m_stlinkLabel->setText(tr("●  ST-Link: Bağlı Değil"));
        m_stlinkLabel->setStyleSheet("color: #F38BA8;");
    }
}
