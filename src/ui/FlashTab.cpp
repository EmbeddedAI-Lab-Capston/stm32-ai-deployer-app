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
#include <QFileDialog>
#include <QDateTime>

#include "modules/flash/FlashManager.h"

FlashTab::FlashTab(QWidget *parent)
    : QWidget(parent)
    , m_flashManager(new FlashManager(this))
{
    setupUi();
}

FlashTab::~FlashTab() = default;

void FlashTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // ── Model information form ─────────────────────────────────────────────
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

    modelForm->addRow(tr("Model Adı    :"), m_modelNameEdit);
    modelForm->addRow(tr("Mimari       :"), m_archCombo);
    modelForm->addRow(tr("Quantization :"), m_quantCombo);
    modelForm->addRow(tr(".hex / .bin  :"), fileRow);

    mainLayout->addWidget(modelBox);

    // ── Status row ─────────────────────────────────────────────────────────
    auto *statusRow    = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(4, 0, 4, 0);

    auto *targetLabel = new QLabel(tr("Hedef Kart:  <b>STM32F4</b>"), this);
    targetLabel->setObjectName("flashStatusLabel");

    m_stlinkLabel = new QLabel(tr("●  ST-Link: Bağlı"), this);
    m_stlinkLabel->setObjectName("stlinkConnected");

    statusLayout->addWidget(targetLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_stlinkLabel);

    mainLayout->addWidget(statusRow);

    // ── Flash button ───────────────────────────────────────────────────────
    auto *flashBtn = new QPushButton(tr("  Karta Yükle (Flash)  "), this);
    flashBtn->setObjectName("primaryButton");
    flashBtn->setMinimumHeight(44);
    mainLayout->addWidget(flashBtn);

    // ── Output area ────────────────────────────────────────────────────────
    auto *outputBox    = new QGroupBox(tr("Flash Çıktısı"), this);
    auto *outputLayout = new QVBoxLayout(outputBox);

    m_outputEdit = new QTextEdit(this);
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setObjectName("flashOutput");
    m_outputEdit->setMinimumHeight(200);
    m_outputEdit->append(
        "<span style='color:#6C7086;'>[Hazır — flash başlatmak için butona basın]</span>");

    outputLayout->addWidget(m_outputEdit);
    mainLayout->addWidget(outputBox, 1);

    // ── Connections ────────────────────────────────────────────────────────
    connect(browseBtn, &QPushButton::clicked, this, &FlashTab::onBrowseClicked);
    connect(flashBtn,  &QPushButton::clicked, this, &FlashTab::onFlashClicked);
    // TODO(asama-4): connect flashBtn to FlashManager::startFlash
}

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
    // TODO(asama-4): replace with real FlashManager::startFlash call
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    const QStringList lines = {
        QString("<span style='color:#6C7086;'>[%1]</span> "
                "<span style='color:#A6E3A1;'>&gt; STM32_Programmer_CLI v2.22.0</span>").arg(ts),
        QString("<span style='color:#6C7086;'>[%1]</span> "
                "<span style='color:#A6E3A1;'>&gt; Connecting to ST-LINK...</span>").arg(ts),
        QString("<span style='color:#6C7086;'>[%1]</span> "
                "<span style='color:#A6E3A1;'>&gt; Memory Programming...</span>").arg(ts),
        QString("<span style='color:#6C7086;'>[%1]</span> "
                "<span style='color:#A6E3A1;'>&gt; File download complete.</span>").arg(ts),
        QString("<span style='color:#6C7086;'>[%1]</span> "
                "<span style='color:#89B4FA;'>&gt; Programming done in 2.35s</span>").arg(ts),
    };
    for (const auto &line : lines)
        m_outputEdit->append(line);
}
