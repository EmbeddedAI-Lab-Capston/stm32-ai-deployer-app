#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QProcess>
#include <QFile>
#include <QMessageBox>
#include "core/AppSettings.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Ayarlar"));
    setMinimumWidth(520);
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

    // Section: STM32 Programmer CLI
    auto *sectionLabel = new QLabel(tr("STM32 Programlayıcı"), this);
    sectionLabel->setObjectName("settingsSectionLabel");
    mainLayout->addWidget(sectionLabel);

    auto *formLayout = new QFormLayout;
    formLayout->setSpacing(8);

    auto *cliRow = new QHBoxLayout;
    m_cliPathEdit = new QLineEdit(this);
    m_cliPathEdit->setPlaceholderText(tr("C:\\Program Files\\STMicroelectronics\\STM32Cube\\...\\STM32_Programmer_CLI.exe"));
    m_cliPathEdit->setMinimumWidth(360);

    m_browseButton = new QPushButton(tr("Gözat..."), this);
    m_browseButton->setFixedWidth(80);
    connect(m_browseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseClicked);

    m_testButton = new QPushButton(tr("Test Et"), this);
    m_testButton->setFixedWidth(80);
    connect(m_testButton, &QPushButton::clicked, this, &SettingsDialog::onTestCliClicked);

    cliRow->addWidget(m_cliPathEdit);
    cliRow->addWidget(m_browseButton);
    cliRow->addWidget(m_testButton);

    formLayout->addRow(tr("CLI Yolu:"), cliRow);
    mainLayout->addLayout(formLayout);

    mainLayout->addStretch();

    // Buttons
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_saveButton = new QPushButton(tr("Kaydet"), this);
    m_saveButton->setObjectName("primaryButton");
    m_saveButton->setDefault(true);
    connect(m_saveButton, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);

    m_cancelButton = new QPushButton(tr("İptal"), this);
    connect(m_cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);

    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_saveButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void SettingsDialog::loadCurrentValues()
{
    AppSettings settings;
    m_cliPathEdit->setText(settings.programmerCliPath());
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

    const QString out  = QString::fromLocal8Bit(proc.readAllStandardOutput());
    const QString err  = QString::fromLocal8Bit(proc.readAllStandardError());
    const QString all  = out + err;

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

void SettingsDialog::onBrowseClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("STM32_Programmer_CLI.exe Seç"),
        QString{},
        tr("Çalıştırılabilir Dosya (*.exe);;Tüm Dosyalar (*)")
    );

    if (!path.isEmpty()) {
        m_cliPathEdit->setText(path);
    }
}

void SettingsDialog::onSaveClicked()
{
    AppSettings settings;
    settings.setProgrammerCliPath(m_cliPathEdit->text().trimmed());
    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}
