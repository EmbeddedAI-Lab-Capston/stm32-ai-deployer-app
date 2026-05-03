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
#include <QScrollBar>
#include <QFileDialog>
#include <QFile>
#include <QProcess>
#include <QMessageBox>

#include "core/AppSettings.h"
#include "SettingsDialog.h"

FlashTab::FlashTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

FlashTab::~FlashTab() = default;

void FlashTab::initialize(FlashManager *manager)
{
    m_flashManager = manager;

    // Wire manager signals to UI
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

void FlashTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // ── CLI status row ─────────────────────────────────────────────────────
    auto *cliRow    = new QWidget(this);
    auto *cliLayout = new QHBoxLayout(cliRow);
    cliLayout->setContentsMargins(4, 0, 4, 0);
    cliLayout->setSpacing(8);

    m_cliStatusLabel = new QLabel(tr("● CLI durumu kontrol ediliyor..."), this);
    m_cliStatusLabel->setStyleSheet("color: #6C7086;");

    auto *settingsBtn = new QPushButton(tr("Ayarlar"), this);
    settingsBtn->setFixedWidth(80);

    cliLayout->addWidget(m_cliStatusLabel, 1);
    cliLayout->addWidget(settingsBtn);
    mainLayout->addWidget(cliRow);

    // ── Model info form ────────────────────────────────────────────────────
    auto *modelBox  = new QGroupBox(tr("Model Bilgileri"), this);
    auto *modelForm = new QFormLayout(modelBox);
    modelForm->setSpacing(10);
    modelForm->setLabelAlignment(Qt::AlignRight);

    m_modelNameEdit = new QLineEdit(this);
    m_modelNameEdit->setPlaceholderText(tr("örn. MLP_INT8_HAR"));

    m_archCombo = new QComboBox(this);
    m_archCombo->addItems({"MLP", "1D CNN", "LSTM", "TC-ResNet", tr("Özel")});

    m_quantCombo = new QComboBox(this);
    m_quantCombo->addItems({"INT8", "Dynamic Q", "Float32"});

    auto *fileRow    = new QWidget(this);
    auto *fileLayout = new QHBoxLayout(fileRow);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->setSpacing(6);

    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setReadOnly(true);
    m_filePathEdit->setPlaceholderText(tr("Firmware dosyası seçin (.hex / .bin)"));

    auto *browseBtn = new QPushButton(tr("Gözat"), this);
    browseBtn->setFixedWidth(80);

    fileLayout->addWidget(m_filePathEdit);
    fileLayout->addWidget(browseBtn);

    m_simModeCheck = new QCheckBox(tr("Simülasyon Modu  (gerçek flash atılmaz)"), this);
    m_simModeCheck->setChecked(true); // default: checked since no .hex yet

    modelForm->addRow(tr("Model Adı    :"), m_modelNameEdit);
    modelForm->addRow(tr("Mimari       :"), m_archCombo);
    modelForm->addRow(tr("Quantization :"), m_quantCombo);
    modelForm->addRow(tr(".hex / .bin  :"), fileRow);
    modelForm->addRow(QString(), m_simModeCheck);

    mainLayout->addWidget(modelBox);

    // ── Status row ─────────────────────────────────────────────────────────
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

    // ── Progress bar ───────────────────────────────────────────────────────
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // ── Action row ─────────────────────────────────────────────────────────
    auto *actionRow    = new QWidget(this);
    auto *actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);

    m_flashBtn = new QPushButton(tr("  Karta Yükle (Flash)  "), this);
    m_flashBtn->setObjectName("primaryButton");
    m_flashBtn->setMinimumHeight(44);

    m_cancelBtn = new QPushButton(tr("İptal"), this);
    m_cancelBtn->setObjectName("dangerBtn");
    m_cancelBtn->setFixedWidth(80);
    m_cancelBtn->setVisible(false);

    actionLayout->addWidget(m_flashBtn, 1);
    actionLayout->addWidget(m_cancelBtn);
    mainLayout->addWidget(actionRow);

    // ── Output area ────────────────────────────────────────────────────────
    auto *outputBox    = new QGroupBox(tr("Flash Çıktısı"), this);
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
        "[Hazır — flash başlatmak için butona basın]"
        "</span>");

    outputLayout->addWidget(m_outputEdit);
    mainLayout->addWidget(outputBox, 1);

    // ── Connections ────────────────────────────────────────────────────────
    connect(browseBtn,     &QPushButton::clicked, this, &FlashTab::onBrowseClicked);
    connect(m_flashBtn,    &QPushButton::clicked, this, &FlashTab::onFlashClicked);
    connect(m_cancelBtn,   &QPushButton::clicked, this, &FlashTab::onCancelClicked);
    connect(settingsBtn,   &QPushButton::clicked, this, &FlashTab::onSettingsClicked);
    connect(clearOutputBtn,&QPushButton::clicked, this, &FlashTab::onClearOutputClicked);
}

// ── Slots ──────────────────────────────────────────────────────────────────

void FlashTab::onBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Firmware Dosyası Seç"), {},
        tr("Firmware Files (*.hex *.bin);;All Files (*)"));
    if (!path.isEmpty())
        m_filePathEdit->setText(path);
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
    // TODO(asama-5): Read active board from BoardTab / AppSettings
    config.targetBoard = AppSettings().lastBoard().isEmpty()
                             ? "STM32F4"
                             : AppSettings().lastBoard();

    // In sim mode, fill in a placeholder model name if empty
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
    if (dlg.exec() == QDialog::Accepted)
        refreshCliStatus();
}

void FlashTab::onClearOutputClicked()
{
    m_outputEdit->clear();
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
}

void FlashTab::appendOutputLine(const QString &line)
{
    QString color = "#CDD6F4";

    if (line.contains("complete", Qt::CaseInsensitive)  ||
        line.contains("successful", Qt::CaseInsensitive)||
        line.contains("verified", Qt::CaseInsensitive)  ||
        line.startsWith("✓")) {
        color = "#A6E3A1";
    } else if (line.contains("Error", Qt::CaseSensitive) ||
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
