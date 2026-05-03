#include "BoardTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QMessageBox>

#include "modules/board/BoardManager.h"

namespace {
struct BoardPreset { QString name; int flashKb; int ramKb; int clockMhz; };
const BoardPreset kPresets[] = {
    { "STM32F4", 1024,  192,  168 },
    { "STM32H7", 2048, 1024,  480 },
    { "STM32N6", 4096, 4096,  800 },
};
}

BoardTab::BoardTab(QWidget *parent)
    : QWidget(parent)
    , m_boardManager(new BoardManager(this))
{
    setupUi();
}

BoardTab::~BoardTab() = default;

void BoardTab::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // ── Card selection ─────────────────────────────────────────────────────
    auto *selBox = new QGroupBox(tr("Kart Seçimi"), this);
    auto *selLayout = new QHBoxLayout(selBox);
    selLayout->setSpacing(10);

    m_boardGroup = new QButtonGroup(this);
    m_boardGroup->setExclusive(true);

    auto makeCardBtn = [&](const QString &name, const QString &chip, int id) -> QPushButton * {
        auto *btn = new QPushButton(name + "\n" + chip, this);
        btn->setCheckable(true);
        btn->setObjectName("boardBtn");
        btn->setMinimumSize(150, 64);
        m_boardGroup->addButton(btn, id);
        return btn;
    };

    m_btnF4 = makeCardBtn("STM32F4", "168 MHz · 1 MB Flash", 0);
    m_btnH7 = makeCardBtn("STM32H7", "480 MHz · 2 MB Flash", 1);
    m_btnN6 = makeCardBtn("STM32N6", "800 MHz · 4 MB Flash", 2);
    m_btnF4->setChecked(true);

    selLayout->addWidget(m_btnF4);
    selLayout->addWidget(m_btnH7);
    selLayout->addWidget(m_btnN6);
    selLayout->addStretch();

    auto *addCustomBtn = new QPushButton(tr("＋  Özel Kart Ekle"), this);
    selLayout->addWidget(addCustomBtn);

    mainLayout->addWidget(selBox);

    // ── Bottom: info panel + sensor config ────────────────────────────────
    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->setSpacing(12);

    // Card info
    auto *infoBox = new QGroupBox(tr("Seçili Kart Bilgileri"), this);
    auto *infoForm = new QFormLayout(infoBox);
    infoForm->setSpacing(10);
    infoForm->setLabelAlignment(Qt::AlignRight);

    auto makeVal = [&](QLabel *&lbl, const QString &text) -> QLabel * {
        lbl = new QLabel(text, this);
        lbl->setObjectName("infoValue");
        return lbl;
    };

    infoForm->addRow(tr("Model :"), makeVal(m_infoModel,  "STM32F4"));
    infoForm->addRow(tr("Flash :"), makeVal(m_infoFlash,  "1024 KB"));
    infoForm->addRow(tr("RAM   :"), makeVal(m_infoRam,    "192 KB"));
    infoForm->addRow(tr("Hız   :"), makeVal(m_infoClock,  "168 MHz"));

    m_infoStatus = new QLabel(tr("● Uyumlu"), this);
    m_infoStatus->setObjectName("statusOk");
    infoForm->addRow(tr("Durum :"), m_infoStatus);

    bottomLayout->addWidget(infoBox, 1);

    // Sensor config
    auto *sensorBox = new QGroupBox(tr("Sensör Konfigürasyonu"), this);
    auto *sensorVLayout = new QVBoxLayout(sensorBox);
    sensorVLayout->setSpacing(8);

    auto *sensorForm = new QFormLayout;
    m_sensorCombo = new QComboBox(this);
    m_sensorCombo->addItem(tr("Seçiniz..."));
    m_sensorCombo->addItem("MPU-6050  (I²C)");
    m_sensorCombo->addItem("BME280  (I²C)");
    m_sensorCombo->addItem("PDM Mikrofon");
    m_sensorCombo->addItem(tr("Özel"));
    sensorForm->addRow(tr("Sensör :"), m_sensorCombo);
    sensorVLayout->addLayout(sensorForm);

    // Dynamic pin fields — hidden until sensor is selected
    m_pinWidget = new QWidget(this);
    auto *pinForm = new QFormLayout(m_pinWidget);
    pinForm->setContentsMargins(0, 4, 0, 0);
    pinForm->setSpacing(8);

    m_pin1Label = new QLabel("SDA Pin :", this);
    m_pin1Edit  = new QLineEdit(this);
    m_pin1Edit->setPlaceholderText("PB6");

    m_pin2Label = new QLabel("SCL Pin :", this);
    m_pin2Edit  = new QLineEdit(this);
    m_pin2Edit->setPlaceholderText("PB7");

    pinForm->addRow(m_pin1Label, m_pin1Edit);
    pinForm->addRow(m_pin2Label, m_pin2Edit);
    m_pinWidget->setVisible(false);
    sensorVLayout->addWidget(m_pinWidget);

    sensorVLayout->addStretch();

    auto *saveConfigBtn = new QPushButton(tr("Konfigürasyonu Kaydet"), this);
    saveConfigBtn->setObjectName("primaryButton");
    sensorVLayout->addWidget(saveConfigBtn);

    bottomLayout->addWidget(sensorBox, 1);
    mainLayout->addLayout(bottomLayout, 1);

    // ── Connections ────────────────────────────────────────────────────────
    connect(m_boardGroup,  &QButtonGroup::idClicked,
            this, &BoardTab::onBoardSelected);
    connect(m_sensorCombo, &QComboBox::currentIndexChanged,
            this, &BoardTab::onSensorChanged);
    connect(addCustomBtn,  &QPushButton::clicked,
            this, &BoardTab::onAddCustomBoardClicked);
    // TODO(asama-5): connect saveConfigBtn to Database::saveConfig
}

void BoardTab::onBoardSelected(int id)
{
    updateBoardInfo(id);
}

void BoardTab::updateBoardInfo(int idx)
{
    if (idx < 0 || idx > 2) return;
    const auto &p = kPresets[idx];
    m_infoModel->setText(p.name);
    m_infoFlash->setText(QString::number(p.flashKb) + " KB");
    m_infoRam->setText(QString::number(p.ramKb) + " KB");
    m_infoClock->setText(QString::number(p.clockMhz) + " MHz");
    // TODO(asama-3): compare against loaded model RAM requirement
    m_infoStatus->setText(tr("● Uyumlu"));
    m_infoStatus->setObjectName("statusOk");
    m_infoStatus->style()->unpolish(m_infoStatus);
    m_infoStatus->style()->polish(m_infoStatus);

    emit boardChanged(p.name, p.flashKb, p.ramKb, p.clockMhz);
}

void BoardTab::onSensorChanged(int index)
{
    m_pinWidget->setVisible(index > 0);
    switch (index) {
    case 1: // MPU-6050
        m_pin1Label->setText("SDA Pin :"); m_pin1Edit->setPlaceholderText("PB6");
        m_pin2Label->setText("SCL Pin :"); m_pin2Edit->setPlaceholderText("PB7");
        break;
    case 2: // BME280
        m_pin1Label->setText("SDA Pin :"); m_pin1Edit->setPlaceholderText("PB6");
        m_pin2Label->setText("SCL Pin :"); m_pin2Edit->setPlaceholderText("PB7");
        break;
    case 3: // PDM Mic
        m_pin1Label->setText("DATA Pin :"); m_pin1Edit->setPlaceholderText("PA7");
        m_pin2Label->setText("CLK  Pin :"); m_pin2Edit->setPlaceholderText("PA5");
        break;
    case 4: // Custom
        m_pin1Label->setText("Pin 1 :"); m_pin1Edit->setPlaceholderText("PA0");
        m_pin2Label->setText("Pin 2 :"); m_pin2Edit->setPlaceholderText("PA1");
        break;
    default: break;
    }
}

void BoardTab::onAddCustomBoardClicked()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Özel Kart Ekle"));
    dlg->setMinimumWidth(360);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto *layout  = new QVBoxLayout(dlg);
    auto *form    = new QFormLayout;
    auto *nameEdit  = new QLineEdit(dlg);
    auto *flashSpin = new QSpinBox(dlg);
    auto *ramSpin   = new QSpinBox(dlg);
    auto *clockSpin = new QSpinBox(dlg);

    nameEdit->setPlaceholderText("örn. STM32L4");
    flashSpin->setRange(64, 32768); flashSpin->setSuffix(" KB"); flashSpin->setValue(512);
    ramSpin->setRange(8, 16384);    ramSpin->setSuffix(" KB");   ramSpin->setValue(128);
    clockSpin->setRange(1, 1000);   clockSpin->setSuffix(" MHz"); clockSpin->setValue(80);

    form->addRow(tr("Kart Adı  :"), nameEdit);
    form->addRow(tr("Flash     :"), flashSpin);
    form->addRow(tr("RAM       :"), ramSpin);
    form->addRow(tr("Saat Hızı :"), clockSpin);
    layout->addLayout(form);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dlg);
    layout->addWidget(btns);
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() == QDialog::Accepted) {
        // TODO(asama-5): save to SQLite via Database
        const QString name = nameEdit->text().isEmpty() ? tr("Yeni Kart") : nameEdit->text();
        QMessageBox::information(this, tr("Kart Eklendi"),
            tr("'%1' kaydedildi.\nVeritabanı bağlantısı Aşama 5'te eklenecek.").arg(name));
    }
}
