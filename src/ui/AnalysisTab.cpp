#include "AnalysisTab.h"

#include <QVBoxLayout>
#include <QLabel>
#include "modules/analysis/AnalysisManager.h"

AnalysisTab::AnalysisTab(QWidget *parent)
    : QWidget(parent)
    , m_analysisManager(new AnalysisManager(this))
{
    setupUi();
}

AnalysisTab::~AnalysisTab() = default;

void AnalysisTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop);

    auto *placeholder = new QLabel(
        tr("Analiz & Karşılaştırma\n\n"
           "Bu sekme Qt Charts ile canlı metrik grafikleri,\n"
           "SQLite oturum kayıtları ve model karşılaştırma tablosunu içerir.\n"
           "(Aşama 4'te geliştirilecek)"),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName("placeholderLabel");

    layout->addWidget(placeholder);
    setLayout(layout);
}
