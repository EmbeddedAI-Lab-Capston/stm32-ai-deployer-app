#include "FlashTab.h"

#include <QVBoxLayout>
#include <QLabel>
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
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);

    auto *placeholder = new QLabel(
        tr("Model Yükleme & Flash\n\n"
           "Bu sekme .hex/.bin dosya seçimi ve STM32_Programmer_CLI\n"
           "aracılığıyla kart programlama işlemlerini yönetir.\n"
           "(Aşama 2'de geliştirilecek)"),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName("placeholderLabel");

    layout->addWidget(placeholder);
    setLayout(layout);
}
