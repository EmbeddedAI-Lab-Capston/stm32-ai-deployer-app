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
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPixmap>
#include <QStringConverter>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarCategoryAxis>

#include "modules/analysis/AnalysisManager.h"

namespace {

QString tableCellText(const QTableWidget *table, int row, int column)
{
    const QTableWidgetItem *item = table->item(row, column);
    return item ? item->text() : QString();
}

QString headerText(const QTableWidget *table, int column)
{
    const QTableWidgetItem *item = table->horizontalHeaderItem(column);
    return item ? item->text() : QString();
}

QString csvEscape(QString text)
{
    text.replace('"', "\"\"");
    if (text.contains(',') || text.contains('"') ||
        text.contains('\n') || text.contains('\r')) {
        return '"' + text + '"';
    }
    return text;
}

QString exportTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
}

void drawTextInCell(QPainter &painter, const QRect &rect, const QString &text)
{
    painter.drawText(
        rect.adjusted(6, 0, -6, 0),
        Qt::AlignVCenter | Qt::AlignLeft,
        painter.fontMetrics().elidedText(text, Qt::ElideRight, rect.width() - 12));
}

}

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
    if (m_sessionTable->rowCount() == 0) {
        QMessageBox::warning(this, tr("CSV Export"),
            tr("Disa aktarilacak oturum verisi yok."));
        return;
    }

    const QString defaultPath = QDir::homePath() +
        QString("/stm32_ai_sessions_%1.csv").arg(exportTimestamp());
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("CSV Export"),
        defaultPath,
        tr("CSV Files (*.csv);;All Files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("CSV Export"),
            tr("Dosya yazilamadi:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF);

    QStringList headers;
    for (int col = 0; col < m_sessionTable->columnCount(); ++col)
        headers << csvEscape(headerText(m_sessionTable, col));
    out << headers.join(',') << '\n';

    for (int row = 0; row < m_sessionTable->rowCount(); ++row) {
        QStringList cells;
        for (int col = 0; col < m_sessionTable->columnCount(); ++col)
            cells << csvEscape(tableCellText(m_sessionTable, row, col));
        out << cells.join(',') << '\n';
    }

    QMessageBox::information(this, tr("CSV Export"),
        tr("CSV dosyasi olusturuldu:\n%1").arg(path));
}

void AnalysisTab::onExportPdfClicked()
{
    if (m_sessionTable->rowCount() == 0) {
        QMessageBox::warning(this, tr("PDF Rapor"),
            tr("Rapora eklenecek oturum verisi yok."));
        return;
    }

    const QString defaultPath = QDir::homePath() +
        QString("/stm32_ai_report_%1.pdf").arg(exportTimestamp());
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("PDF Rapor"),
        defaultPath,
        tr("PDF Files (*.pdf);;All Files (*)"));
    if (path.isEmpty())
        return;

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(14, 14, 14, 14), QPageLayout::Millimeter);
    writer.setResolution(96);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        QMessageBox::critical(this, tr("PDF Rapor"),
            tr("PDF dosyasi olusturulamadi."));
        return;
    }

    const QRect pageRect = writer.pageLayout().paintRectPixels(writer.resolution());
    int y = pageRect.top();

    QFont titleFont = painter.font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor("#111827"));
    painter.drawText(QRect(pageRect.left(), y, pageRect.width(), 32),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("STM32 AI Deployer - Analiz Raporu"));
    y += 38;

    QFont metaFont = painter.font();
    metaFont.setPointSize(9);
    metaFont.setBold(false);
    painter.setFont(metaFont);
    painter.setPen(QColor("#4B5563"));
    painter.drawText(QRect(pageRect.left(), y, pageRect.width(), 22),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("Olusturma zamani: %1")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    y += 34;

    const int columns = m_sessionTable->columnCount();
    const int rowHeight = 30;
    const int tableWidth = pageRect.width();
    const int colWidth = tableWidth / columns;

    auto drawHeader = [&]() {
        painter.setBrush(QColor("#1F2937"));
        painter.setPen(Qt::NoPen);
        painter.drawRect(pageRect.left(), y, tableWidth, rowHeight);

        QFont headerFont = painter.font();
        headerFont.setBold(true);
        painter.setFont(headerFont);
        painter.setPen(Qt::white);

        for (int col = 0; col < columns; ++col) {
            const QRect cell(pageRect.left() + col * colWidth, y,
                             col == columns - 1 ? tableWidth - col * colWidth : colWidth,
                             rowHeight);
            drawTextInCell(painter, cell, headerText(m_sessionTable, col));
        }

        y += rowHeight;
        headerFont.setBold(false);
        painter.setFont(headerFont);
    };

    drawHeader();

    for (int row = 0; row < m_sessionTable->rowCount(); ++row) {
        if (y + rowHeight > pageRect.bottom()) {
            writer.newPage();
            y = pageRect.top();
            drawHeader();
        }

        painter.setBrush(row % 2 == 0 ? QColor("#F9FAFB") : QColor("#FFFFFF"));
        painter.setPen(QColor("#D1D5DB"));
        painter.drawRect(pageRect.left(), y, tableWidth, rowHeight);

        for (int col = 0; col < columns; ++col) {
            const QRect cell(pageRect.left() + col * colWidth, y,
                             col == columns - 1 ? tableWidth - col * colWidth : colWidth,
                             rowHeight);
            painter.setPen(QColor("#D1D5DB"));
            painter.drawRect(cell);
            painter.setPen(QColor("#111827"));
            drawTextInCell(painter, cell, tableCellText(m_sessionTable, row, col));
        }
        y += rowHeight;
    }

    y += 28;
    const QPixmap chartPixmap = m_chartView->grab();
    if (!chartPixmap.isNull()) {
        if (y + 160 > pageRect.bottom()) {
            writer.newPage();
            y = pageRect.top();
        }

        QFont sectionFont = painter.font();
        sectionFont.setPointSize(12);
        sectionFont.setBold(true);
        painter.setFont(sectionFont);
        painter.setPen(QColor("#111827"));
        painter.drawText(QRect(pageRect.left(), y, pageRect.width(), 26),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         tr("Karsilastirma Grafigi"));
        y += 32;

        const QPixmap scaledChart = chartPixmap.scaledToWidth(
            pageRect.width(), Qt::SmoothTransformation);
        const int drawHeight = qMin(scaledChart.height(), pageRect.bottom() - y);
        painter.drawPixmap(QRect(pageRect.left(), y, pageRect.width(), drawHeight),
                           scaledChart,
                           QRect(0, 0, scaledChart.width(), drawHeight));
    }

    painter.end();

    QMessageBox::information(this, tr("PDF Rapor"),
        tr("PDF raporu olusturuldu:\n%1").arg(path));
}
