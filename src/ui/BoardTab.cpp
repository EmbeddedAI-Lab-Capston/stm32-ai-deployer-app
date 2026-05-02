#include "BoardTab.h"

#include <QVBoxLayout>
#include <QLabel>
#include "modules/board/BoardManager.h"

BoardTab::BoardTab(QWidget *parent)
    : QWidget(parent)
    , m_boardManager(new BoardManager(this))
{
    setupUi();
}

BoardTab::~BoardTab() = default;

void BoardTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);

    auto *placeholder = new QLabel(
        tr("Kart Yönetimi\n\n"
           "Bu sekme kart presetlerini ve özel kart konfigürasyonlarını yönetir.\n"
           "(Aşama 2'de geliştirilecek)"),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName("placeholderLabel");

    layout->addWidget(placeholder);
    setLayout(layout);
}
