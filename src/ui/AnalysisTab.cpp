#include "AnalysisTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QPainter>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>

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
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // ── Session history ────────────────────────────────────────────────────
    auto *historyBox    = new QGroupBox(tr("Oturum Geçmişi"), this);
    auto *historyLayout = new QVBoxLayout(historyBox);
    historyLayout->setSpacing(8);

    // Toolbar
    auto *toolRow    = new QWidget(this);
    auto *toolLayout = new QHBoxLayout(toolRow);
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->addStretch();

    auto *refreshBtn = new QPushButton(tr("⟳  Yenile"), this);
    auto *deleteBtn  = new QPushButton(tr("✕  Sil"), this);
    deleteBtn->setObjectName("dangerBtn");

    toolLayout->addWidget(refreshBtn);
    toolLayout->addWidget(deleteBtn);
    historyLayout->addWidget(toolRow);

    // Table
    m_sessionTable = new QTableWidget(0, 5, this);
    m_sessionTable->setObjectName("sessionTable");
    m_sessionTable->setHorizontalHeaderLabels(
        {tr("Tarih"), tr("Model"), tr("Kart"), tr("Inf (ms)"), tr("Doğruluk %")});
    m_sessionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sessionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sessionTable->setAlternatingRowColors(true);
    m_sessionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sessionTable->verticalHeader()->setVisible(false);
    m_sessionTable->setMinimumHeight(150);

    populateSampleData();
    historyLayout->addWidget(m_sessionTable);
    mainLayout->addWidget(historyBox);

    // ── Comparison chart ───────────────────────────────────────────────────
    auto *chartBox    = new QGroupBox(tr("Karşılaştırma Grafiği"), this);
    auto *chartLayout = new QVBoxLayout(chartBox);

    // Placeholder bar chart with sample data
    auto *set0 = new QBarSet(tr("Inf (ms)"));
    set0->setColor(QColor("#89B4FA"));
    *set0 << 8.2 << 7.1 << 4.8;

    auto *series = new QBarSeries;
    series->append(set0);

    auto *chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(tr("Veriler yüklenince grafik burada görünecek"));
    chart->setBackgroundBrush(QColor("#181825"));
    chart->setTitleBrush(QColor("#6C7086"));

    auto *axisX = new QBarCategoryAxis;
    axisX->append({"MLP_INT8", "CNN_INT8", "LSTM_INT8"});
    axisX->setLabelsColor(QColor("#CDD6F4"));
    axisX->setGridLineColor(QColor("#45475A"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis;
    axisY->setRange(0, 12);
    axisY->setTitleText(tr("ms"));
    axisY->setTitleBrush(QColor("#6C7086"));
    axisY->setLabelsColor(QColor("#CDD6F4"));
    axisY->setGridLineColor(QColor("#45475A"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setLabelColor(QColor("#CDD6F4"));
    chart->legend()->setAlignment(Qt::AlignBottom);

    m_chartView = new QChartView(chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(200);
    m_chartView->setObjectName("analysisChart");

    chartLayout->addWidget(m_chartView);
    mainLayout->addWidget(chartBox, 1);

    // ── Export buttons ─────────────────────────────────────────────────────
    auto *exportRow    = new QWidget(this);
    auto *exportLayout = new QHBoxLayout(exportRow);
    exportLayout->setContentsMargins(0, 0, 0, 0);
    exportLayout->setSpacing(8);

    auto *csvBtn = new QPushButton(tr("CSV Dışa Aktar"), this);
    auto *pdfBtn = new QPushButton(tr("PDF Rapor"), this);

    exportLayout->addWidget(csvBtn);
    exportLayout->addWidget(pdfBtn);
    exportLayout->addStretch();

    mainLayout->addWidget(exportRow);

    // ── Connections ────────────────────────────────────────────────────────
    connect(refreshBtn, &QPushButton::clicked, this, &AnalysisTab::onRefreshClicked);
    connect(deleteBtn,  &QPushButton::clicked, this, &AnalysisTab::onDeleteClicked);
    connect(csvBtn,     &QPushButton::clicked, this, &AnalysisTab::onExportCsvClicked);
    connect(pdfBtn,     &QPushButton::clicked, this, &AnalysisTab::onExportPdfClicked);
    // TODO(asama-5): connect refreshBtn to AnalysisManager::loadSessions
}

void AnalysisTab::populateSampleData()
{
    struct Row { const char *date, *model, *board, *inf, *acc; };
    const Row rows[] = {
        { "2025-01-15", "MLP_INT8",  "STM32F4", "8.2", "96" },
        { "2025-01-15", "CNN_INT8",  "STM32H7", "7.1", "97" },
        { "2025-01-16", "LSTM_INT8", "STM32N6", "4.8", "95" },
    };

    m_sessionTable->setRowCount(0);
    for (const auto &r : rows) {
        const int row = m_sessionTable->rowCount();
        m_sessionTable->insertRow(row);
        auto makeItem = [](const char *text) {
            auto *item = new QTableWidgetItem(QString::fromUtf8(text));
            item->setTextAlignment(Qt::AlignCenter);
            return item;
        };
        m_sessionTable->setItem(row, 0, makeItem(r.date));
        m_sessionTable->setItem(row, 1, makeItem(r.model));
        m_sessionTable->setItem(row, 2, makeItem(r.board));
        m_sessionTable->setItem(row, 3, makeItem(r.inf));
        m_sessionTable->setItem(row, 4, makeItem(r.acc));
    }
}

void AnalysisTab::onRefreshClicked()
{
    // TODO(asama-5): load from SQLite via AnalysisManager
    populateSampleData();
}

void AnalysisTab::onDeleteClicked()
{
    // TODO(asama-5): delete selected session from SQLite
    const int row = m_sessionTable->currentRow();
    if (row >= 0)
        m_sessionTable->removeRow(row);
}

void AnalysisTab::onExportCsvClicked()
{
    QMessageBox::information(this, tr("CSV Dışa Aktar"),
        tr("Yakında eklenecek (Aşama 7)."));
}

void AnalysisTab::onExportPdfClicked()
{
    QMessageBox::information(this, tr("PDF Rapor"),
        tr("Yakında eklenecek (Aşama 7)."));
}
