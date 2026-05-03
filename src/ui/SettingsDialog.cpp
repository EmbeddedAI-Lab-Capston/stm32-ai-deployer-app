#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QProcess>
#include <QFile>
#include <QMessageBox>
#include "core/AppSettings.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Ayarlar"));
    setMinimumWidth(560);
    setModal(true);

    setupUi();
    loadCurrentValues();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ── Section: STM32 Programmer CLI ──────────────────────────────────────
    auto *progLabel = new QLabel(tr("STM32 Programlayıcı"), this);
    progLabel->setObjectName("settingsSectionLabel");
    mainLayout->addWidget(progLabel);

    auto *progForm = new QFormLayout;
    progForm->setSpacing(8);

    auto *cliRow = new QHBoxLayout;
    m_cliPathEdit = new QLineEdit(this);
    m_cliPathEdit->setPlaceholderText(
        tr("C:\\...\\STM32_Programmer_CLI.exe"));
    m_cliPathEdit->setMinimumWidth(360);

    m_browseButton = new QPushButton(tr("Gözat..."), this);
    m_browseButton->setFixedWidth(80);
    connect(m_browseButton, &QPushButton::clicked,
            this, &SettingsDialog::onBrowseClicked);

    m_testButton = new QPushButton(tr("Test Et"), this);
    m_testButton->setFixedWidth(80);
    connect(m_testButton, &QPushButton::clicked,
            this, &SettingsDialog::onTestCliClicked);

    cliRow->addWidget(m_cliPathEdit);
    cliRow->addWidget(m_browseButton);
    cliRow->addWidget(m_testButton);
    progForm->addRow(tr("CLI Yolu:"), cliRow);
    mainLayout->addLayout(progForm);

    // ── Section: X-CUBE-AI CLI ─────────────────────────────────────────────
    auto *xcubeLabel = new QLabel(tr("X-CUBE-AI / ST Edge AI (stedgeai)"), this);
    xcubeLabel->setObjectName("settingsSectionLabel");
    mainLayout->addWidget(xcubeLabel);

    auto *xcubeForm = new QFormLayout;
    xcubeForm->setSpacing(8);

    auto *xcubeRow = new QHBoxLayout;
    m_xcubePathEdit = new QLineEdit(this);
    m_xcubePathEdit->setPlaceholderText(
        tr("stedgeai  veya  C:\\...\\stedgeai.exe"));
    m_xcubePathEdit->setMinimumWidth(360);

    m_xcubeBrowseButton = new QPushButton(tr("Gözat..."), this);
    m_xcubeBrowseButton->setFixedWidth(80);
    connect(m_xcubeBrowseButton, &QPushButton::clicked,
            this, &SettingsDialog::onBrowseXCubeAIClicked);

    m_xcubeTestButton = new QPushButton(tr("Test Et"), this);
    m_xcubeTestButton->setFixedWidth(80);
    connect(m_xcubeTestButton, &QPushButton::clicked,
            this, &SettingsDialog::onTestXCubeAIClicked);

    xcubeRow->addWidget(m_xcubePathEdit);
    xcubeRow->addWidget(m_xcubeBrowseButton);
    xcubeRow->addWidget(m_xcubeTestButton);
    xcubeForm->addRow(tr("CLI Yolu:"), xcubeRow);

    auto *xcubeHint = new QLabel(
        tr("X-CUBE-AI v10+:\n"
           "STM32Cube/Repository/Packs/STMicroelectronics/X-CUBE-AI/<ver>/Utilities/windows/stedgeai.exe"),
        this);
    xcubeHint->setStyleSheet("color: #6C7086; font-size: 11px;");
    xcubeForm->addRow(QString(), xcubeHint);

    mainLayout->addLayout(xcubeForm);
    mainLayout->addStretch();

    // ── Dialog buttons ─────────────────────────────────────────────────────
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_saveButton = new QPushButton(tr("Kaydet"), this);
    m_saveButton->setObjectName("primaryButton");
    m_saveButton->setDefault(true);
    connect(m_saveButton, &QPushButton::clicked,
            this, &SettingsDialog::onSaveClicked);

    m_cancelButton = new QPushButton(tr("İptal"), this);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &SettingsDialog::onCancelClicked);

    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_saveButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void SettingsDialog::loadCurrentValues()
{
    AppSettings settings;
    m_cliPathEdit->setText(settings.programmerCliPath());
    m_xcubePathEdit->setText(settings.xcubeAICliPath());
}

// ── STM32 Programmer CLI ───────────────────────────────────────────────────

void SettingsDialog::onBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("STM32_Programmer_CLI.exe Seç"),
        {},
        tr("Çalıştırılabilir Dosya (*.exe);;Tüm Dosyalar (*)"));
    if (!path.isEmpty())
        m_cliPathEdit->setText(path);
}

void SettingsDialog::onTestCliClicked()
{
    const QString path = m_cliPathEdit->text().trimmed();
    if (path.isEmpty() || !QFile::exists(path)) {
        QMessageBox::warning(this, tr("Hata"),
            tr("Dosya bulunamadı:\n") + path);
        return;
    }

    QProcess proc;
    proc.start(path, {"--version"});
    proc.waitForFinished(5000);

    const QString all = QString::fromLocal8Bit(
        proc.readAllStandardOutput() + proc.readAllStandardError());

    if (all.contains("STM32CubeProgrammer", Qt::CaseInsensitive) ||
        all.contains("STM32_Programmer",    Qt::CaseInsensitive) ||
        proc.exitCode() == 0) {
        QMessageBox::information(this, tr("CLI Testi Başarılı"),
            tr("CLI çalışıyor.\n\n") + all.trimmed().left(400));
    } else {
        QMessageBox::warning(this, tr("CLI Testi"),
            tr("CLI çalıştı fakat beklenen çıktı gelmedi.\n\n")
            + all.trimmed().left(400));
    }
}

// ── X-CUBE-AI CLI ─────────────────────────────────────────────────────────

void SettingsDialog::onBrowseXCubeAIClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("stm32ai.exe Seç"),
        {},
        tr("Çalıştırılabilir Dosya (*.exe);;Tüm Dosyalar (*)"));
    if (!path.isEmpty())
        m_xcubePathEdit->setText(path);
}

void SettingsDialog::onTestXCubeAIClicked()
{
    QString path = m_xcubePathEdit->text().trimmed();
    if (path.isEmpty()) path = "stedgeai";

    QProcess proc;
    proc.start(path, {"--version"});
    proc.waitForFinished(3000);

    const QString all = QString::fromLocal8Bit(
        proc.readAllStandardOutput() + proc.readAllStandardError());

    if (all.contains("stedgeai", Qt::CaseInsensitive) ||
        all.contains("stm32ai",  Qt::CaseInsensitive) ||
        all.contains("ST Edge",  Qt::CaseInsensitive) ||
        proc.exitCode() == 0) {
        QMessageBox::information(this, tr("X-CUBE-AI Testi Başarılı"),
            tr("stedgeai CLI çalışıyor:\n\n") + all.trimmed().left(400));
    } else {
        QMessageBox::warning(this, tr("X-CUBE-AI Bulunamadı"),
            tr("stedgeai komutu çalıştırılamadı.\n\n"
               "X-CUBE-AI v10+ (ST Edge AI) için:\n"
               "STM32Cube/Repository/Packs/STMicroelectronics/"
               "X-CUBE-AI/<ver>/Utilities/windows/stedgeai.exe"));
    }
}

// ── Save / Cancel ──────────────────────────────────────────────────────────

void SettingsDialog::onSaveClicked()
{
    AppSettings settings;
    settings.setProgrammerCliPath(m_cliPathEdit->text().trimmed());
    settings.setXCubeAICliPath(m_xcubePathEdit->text().trimmed());
    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}
