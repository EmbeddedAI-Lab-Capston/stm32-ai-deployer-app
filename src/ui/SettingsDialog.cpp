#include "SettingsDialog.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "core/AppSettings.h"
#include "core/ToolDetector.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Ayarlar — Araç Yolları"));
    setMinimumWidth(580);
    setModal(true);
    setupUi();
    loadCurrentValues();
}

SettingsDialog::~SettingsDialog() = default;

// ── UI layout ─────────────────────────────────────────────────────────────

static QHBoxLayout *makeToolRow(QLineEdit *edit,
                                QPushButton *browseBtn,
                                QPushButton *testBtn)
{
    auto *row = new QHBoxLayout;
    row->addWidget(edit);
    row->addWidget(browseBtn);
    row->addWidget(testBtn);
    return row;
}

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(14);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ── Section header helper ──────────────────────────────────────────────
    auto addSection = [&](const QString &text) {
        auto *lbl = new QLabel(text, this);
        lbl->setObjectName("settingsSectionLabel");
        mainLayout->addWidget(lbl);
    };

    auto addStatusLabel = [&](QLabel *&lbl) -> QFormLayout * {
        auto *form = new QFormLayout;
        form->setSpacing(6);
        lbl = new QLabel(this);
        lbl->setTextFormat(Qt::RichText);
        return form;
    };

    // ── STM32 Programmer CLI ───────────────────────────────────────────────
    addSection(tr("STM32 Programlayıcı (STM32_Programmer_CLI)"));
    auto *progForm = new QFormLayout;
    progForm->setSpacing(6);

    m_cliPathEdit = new QLineEdit(this);
    m_cliPathEdit->setPlaceholderText(tr("C:\\...\\STM32_Programmer_CLI.exe"));
    m_browseButton = new QPushButton(tr("Gözat..."), this);
    m_browseButton->setFixedWidth(80);
    m_testButton = new QPushButton(tr("Test Et"), this);
    m_testButton->setFixedWidth(80);
    m_cliStatus = new QLabel(this);
    m_cliStatus->setTextFormat(Qt::RichText);

    progForm->addRow(tr("CLI Yolu:"), makeToolRow(m_cliPathEdit, m_browseButton, m_testButton));
    progForm->addRow(QString(), m_cliStatus);
    mainLayout->addLayout(progForm);

    connect(m_browseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseClicked);
    connect(m_testButton,   &QPushButton::clicked, this, &SettingsDialog::onTestCliClicked);

    // ── X-CUBE-AI CLI ─────────────────────────────────────────────────────
    addSection(tr("X-CUBE-AI / ST Edge AI (stedgeai)"));
    auto *xcubeForm = new QFormLayout;
    xcubeForm->setSpacing(6);

    m_xcubePathEdit = new QLineEdit(this);
    m_xcubePathEdit->setPlaceholderText(tr("stedgeai  veya  C:\\...\\stedgeai.exe"));
    m_xcubeBrowseBtn = new QPushButton(tr("Gözat..."), this);
    m_xcubeBrowseBtn->setFixedWidth(80);
    m_xcubeTestBtn = new QPushButton(tr("Test Et"), this);
    m_xcubeTestBtn->setFixedWidth(80);
    m_xcubeStatus = new QLabel(this);
    m_xcubeStatus->setTextFormat(Qt::RichText);

    auto *xcubeHint = new QLabel(
        tr("X-CUBE-AI v10+:\n"
           "STM32Cube/Repository/Packs/STMicroelectronics/X-CUBE-AI/<ver>/Utilities/windows/stedgeai.exe"),
        this);
    xcubeHint->setStyleSheet("color: #6C7086; font-size: 11px;");

    xcubeForm->addRow(tr("CLI Yolu:"), makeToolRow(m_xcubePathEdit, m_xcubeBrowseBtn, m_xcubeTestBtn));
    xcubeForm->addRow(QString(), m_xcubeStatus);
    xcubeForm->addRow(QString(), xcubeHint);
    mainLayout->addLayout(xcubeForm);

    connect(m_xcubeBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseXCubeAIClicked);
    connect(m_xcubeTestBtn,   &QPushButton::clicked, this, &SettingsDialog::onTestXCubeAIClicked);

    // ── arm-none-eabi-gcc ─────────────────────────────────────────────────
    addSection(tr("ARM GCC Derleyici (arm-none-eabi-gcc)"));
    auto *gccForm = new QFormLayout;
    gccForm->setSpacing(6);

    m_gccPathEdit = new QLineEdit(this);
    m_gccPathEdit->setPlaceholderText(tr("arm-none-eabi-gcc  veya  C:\\...\\arm-none-eabi-gcc.exe"));
    m_gccBrowseBtn = new QPushButton(tr("Gözat..."), this);
    m_gccBrowseBtn->setFixedWidth(80);
    m_gccTestBtn = new QPushButton(tr("Test Et"), this);
    m_gccTestBtn->setFixedWidth(80);
    m_gccStatus = new QLabel(this);
    m_gccStatus->setTextFormat(Qt::RichText);

    gccForm->addRow(tr("GCC Yolu:"), makeToolRow(m_gccPathEdit, m_gccBrowseBtn, m_gccTestBtn));
    gccForm->addRow(QString(), m_gccStatus);
    mainLayout->addLayout(gccForm);

    connect(m_gccBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseGccClicked);
    connect(m_gccTestBtn,   &QPushButton::clicked, this, &SettingsDialog::onTestGccClicked);

    // ── make ──────────────────────────────────────────────────────────────
    addSection(tr("Make (GNU Make)"));
    auto *makeForm = new QFormLayout;
    makeForm->setSpacing(6);

    m_makePathEdit = new QLineEdit(this);
    m_makePathEdit->setPlaceholderText(tr("make  veya  C:\\...\\make.exe"));
    m_makeBrowseBtn = new QPushButton(tr("Gözat..."), this);
    m_makeBrowseBtn->setFixedWidth(80);
    m_makeTestBtn = new QPushButton(tr("Test Et"), this);
    m_makeTestBtn->setFixedWidth(80);
    m_makeStatus = new QLabel(this);
    m_makeStatus->setTextFormat(Qt::RichText);

    makeForm->addRow(tr("Make Yolu:"), makeToolRow(m_makePathEdit, m_makeBrowseBtn, m_makeTestBtn));
    makeForm->addRow(QString(), m_makeStatus);
    mainLayout->addLayout(makeForm);

    connect(m_makeBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseMakeClicked);
    connect(m_makeTestBtn,   &QPushButton::clicked, this, &SettingsDialog::onTestMakeClicked);

    mainLayout->addStretch();

    // ── Buttons ───────────────────────────────────────────────────────────
    auto *btnLayout = new QHBoxLayout;

    m_scanAllBtn = new QPushButton(tr("Tümünü Tara"), this);
    m_scanAllBtn->setObjectName("primaryButton");
    connect(m_scanAllBtn, &QPushButton::clicked, this, &SettingsDialog::onScanAllClicked);

    m_cancelButton = new QPushButton(tr("İptal"), this);
    m_saveButton   = new QPushButton(tr("Kaydet"), this);
    m_saveButton->setObjectName("primaryButton");
    m_saveButton->setDefault(true);

    connect(m_saveButton,   &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);

    btnLayout->addWidget(m_scanAllBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelButton);
    btnLayout->addWidget(m_saveButton);
    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
}

void SettingsDialog::loadCurrentValues()
{
    AppSettings s;
    m_cliPathEdit->setText(s.programmerCliPath());
    m_xcubePathEdit->setText(s.xcubeAICliPath());
    m_gccPathEdit->setText(s.gccPath());
    m_makePathEdit->setText(s.makePath());
}

QString SettingsDialog::makeStatusText(const QString &versionOrEmpty) const
{
    if (versionOrEmpty.isEmpty())
        return QStringLiteral("<span style='color:#F38BA8;'>✗ Bulunamadı / Çalışmıyor</span>");
    return QString("<span style='color:#A6E3A1;'>✓ %1</span>")
        .arg(versionOrEmpty.left(50));
}

// ── STM32 Programmer CLI ───────────────────────────────────────────────────

void SettingsDialog::onBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("STM32_Programmer_CLI.exe Seç"), {},
        tr("Çalıştırılabilir (*.exe);;Tüm Dosyalar (*)"));
    if (!path.isEmpty()) m_cliPathEdit->setText(path);
}

void SettingsDialog::onTestCliClicked()
{
    const QString path = m_cliPathEdit->text().trimmed();
    if (path.isEmpty() || !QFile::exists(path)) {
        m_cliStatus->setText(makeStatusText({}));
        return;
    }
    const QString ver = ToolDetector::queryVersion(path, {"--version"});
    m_cliStatus->setText(makeStatusText(ver));
}

// ── X-CUBE-AI CLI ─────────────────────────────────────────────────────────

void SettingsDialog::onBrowseXCubeAIClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("stedgeai.exe Seç"), {},
        tr("Çalıştırılabilir (*.exe);;Tüm Dosyalar (*)"));
    if (!path.isEmpty()) m_xcubePathEdit->setText(path);
}

void SettingsDialog::onTestXCubeAIClicked()
{
    QString path = m_xcubePathEdit->text().trimmed();
    if (path.isEmpty()) path = "stedgeai";
    const QString ver = ToolDetector::queryVersion(path, {"--version"});
    m_xcubeStatus->setText(makeStatusText(ver));
}

// ── ARM GCC ───────────────────────────────────────────────────────────────

void SettingsDialog::onBrowseGccClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("arm-none-eabi-gcc.exe Seç"), {},
        tr("Çalıştırılabilir (*.exe);;Tüm Dosyalar (*)"));
    if (!path.isEmpty()) m_gccPathEdit->setText(path);
}

void SettingsDialog::onTestGccClicked()
{
    QString path = m_gccPathEdit->text().trimmed();
    if (path.isEmpty()) path = "arm-none-eabi-gcc";
    const QString ver = ToolDetector::queryVersion(path, {"--version"});
    m_gccStatus->setText(makeStatusText(ver));
}

// ── Make ──────────────────────────────────────────────────────────────────

void SettingsDialog::onBrowseMakeClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("make.exe Seç"), {},
        tr("Çalıştırılabilir (*.exe);;Tüm Dosyalar (*)"));
    if (!path.isEmpty()) m_makePathEdit->setText(path);
}

void SettingsDialog::onTestMakeClicked()
{
    QString path = m_makePathEdit->text().trimmed();
    if (path.isEmpty()) path = "make";
    const QString ver = ToolDetector::queryVersion(path, {"--version"});
    m_makeStatus->setText(makeStatusText(ver));
}

// ── Scan all ──────────────────────────────────────────────────────────────

void SettingsDialog::onScanAllClicked()
{
    m_scanAllBtn->setEnabled(false);
    m_scanAllBtn->setText(tr("Taranıyor..."));
    QCoreApplication::processEvents();

    struct Entry {
        QString       name;
        QString     (*detect)();
        QLineEdit    *edit;
        QLabel       *status;
    };

    const QList<Entry> entries = {
        { "STM32_Programmer_CLI", &ToolDetector::detectStm32Programmer,
          m_cliPathEdit,   m_cliStatus   },
        { "stedgeai",            &ToolDetector::detectXCubeAI,
          m_xcubePathEdit, m_xcubeStatus },
        { "arm-none-eabi-gcc",   &ToolDetector::detectGcc,
          m_gccPathEdit,   m_gccStatus   },
        { "make",                &ToolDetector::detectMake,
          m_makePathEdit,  m_makeStatus  },
    };

    for (const Entry &e : entries) {
        const QString path = e.detect();
        if (!path.isEmpty()) {
            e.edit->setText(path);
            const QString ver = ToolDetector::queryVersion(path);
            e.status->setText(makeStatusText(ver));
        } else {
            e.status->setText(makeStatusText({}));
        }
        QCoreApplication::processEvents();
    }

    m_scanAllBtn->setEnabled(true);
    m_scanAllBtn->setText(tr("Tümünü Tara"));
}

// ── Save / Cancel ──────────────────────────────────────────────────────────

void SettingsDialog::onSaveClicked()
{
    AppSettings s;
    s.setProgrammerCliPath(m_cliPathEdit->text().trimmed());
    s.setXCubeAICliPath(m_xcubePathEdit->text().trimmed());
    s.setGccPath(m_gccPathEdit->text().trimmed());
    s.setMakePath(m_makePathEdit->text().trimmed());
    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}
