#include "AnalysisTab.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringConverter>

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>

#include "core/AppSettings.h"
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
        text.contains('\n') || text.contains('\r'))
        return '"' + text + '"';
    return text;
}

QString exportTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
}

void drawTextInCell(QPainter &painter, const QRect &rect, const QString &text)
{
    painter.drawText(rect.adjusted(6, 0, -6, 0),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     painter.fontMetrics().elidedText(text, Qt::ElideRight, rect.width() - 12));
}

QLabel *makeMetric(const QString &title, const QString &value, QWidget *parent)
{
    auto *box = new QFrame(parent);
    box->setObjectName("metricCard");
    auto *layout = new QVBoxLayout(box);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(title, box);
    titleLabel->setObjectName("sidebarValueLabel");
    auto *valueLabel = new QLabel(value, box);
    valueLabel->setObjectName("metricValue");
    QFont f = valueLabel->font();
    f.setPointSize(16);
    f.setBold(true);
    valueLabel->setFont(f);

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return valueLabel;
}

QTableWidgetItem *makeItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QString modelTypeFromName(const QString &model)
{
    const QString lower = model.toLower();
    if (lower.contains("int8"))
        return QStringLiteral("INT8");
    if (lower.contains("float32") || lower.contains("fp32"))
        return QStringLiteral("Float32");
    if (lower.contains("dynamic"))
        return QStringLiteral("Dynamic Q");
    return QStringLiteral("--");
}

QString boardNameForRow(const QString &wireCard, const BoardInfo &board)
{
    if (!board.probeBoardName.isEmpty())
        return board.probeBoardName;
    if (!wireCard.isEmpty())
        return wireCard;
    return board.isNull() ? QStringLiteral("--") : board.name;
}

QString chipNameForRow(const BoardInfo &board)
{
    if (!board.deviceName.isEmpty())
        return board.deviceName;
    if (board.name.contains("H7", Qt::CaseInsensitive))
        return QStringLiteral("STM32H723ZG");
    if (board.name.contains("F4", Qt::CaseInsensitive))
        return QStringLiteral("STM32F407VG");
    if (board.name.contains("N6", Qt::CaseInsensitive))
        return QStringLiteral("STM32N657");
    return QStringLiteral("--");
}

QString cpuNameForRow(const BoardInfo &board)
{
    if (!board.deviceCpu.isEmpty())
        return board.deviceCpu;
    if (board.name.contains("H7", Qt::CaseInsensitive))
        return QStringLiteral("Cortex-M7");
    if (board.name.contains("F4", Qt::CaseInsensitive))
        return QStringLiteral("Cortex-M4");
    if (board.name.contains("N6", Qt::CaseInsensitive))
        return QStringLiteral("Cortex-M55/NPU");
    return QStringLiteral("--");
}

struct ModelReportDetails {
    QString sensor = QStringLiteral("--");
    QString weights = QStringLiteral("--");
    QString params = QStringLiteral("--");
    QString macc = QStringLiteral("--");
    QString input = QStringLiteral("--");
};

ModelReportDetails loadModelReportDetails()
{
    ModelReportDetails details;
    AppSettings settings;
    const QDir outputDir(settings.lastOutputDir());

    QFile config(outputDir.filePath(QStringLiteral("Inc/ai_config.h")));
    if (config.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString text = QString::fromUtf8(config.readAll());
        const auto match = QRegularExpression(
            QStringLiteral("AI_SENSOR_TYPE\\s+\"([^\"]+)\"")).match(text);
        if (match.hasMatch())
            details.sensor = match.captured(1);
    }

    QFile report(outputDir.filePath(QStringLiteral("xcubeai_output/network_c_info.json")));
    if (report.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(report.readAll());
        const QJsonArray tools = doc.object()
            .value(QStringLiteral("environment")).toObject()
            .value(QStringLiteral("tools")).toArray();
        if (!tools.isEmpty()) {
            const qint64 size = tools.first().toObject()
                .value(QStringLiteral("input_model")).toObject()
                .value(QStringLiteral("size")).toInteger();
            if (size > 0)
                details.weights = QString("%1 KiB").arg(size / 1024.0, 0, 'f', 2);
            const qint64 params = tools.first().toObject()
                .value(QStringLiteral("input_model")).toObject()
                .value(QStringLiteral("n_params")).toInteger();
            if (params > 0)
                details.params = QString::number(params);
        }

        quint64 macc = 0;
        const QJsonArray graphs = doc.object().value(QStringLiteral("graphs")).toArray();
        for (const QJsonValue &graphValue : graphs) {
            const QJsonArray nodes = graphValue.toObject().value(QStringLiteral("nodes")).toArray();
            for (const QJsonValue &nodeValue : nodes)
                macc += static_cast<quint64>(nodeValue.toObject()
                    .value(QStringLiteral("macc")).toInteger());
        }
        if (macc > 0)
            details.macc = QLocale().toString(macc);

        if (!graphs.isEmpty()) {
            const QJsonArray inputs = graphs.first().toObject()
                .value(QStringLiteral("original_inputs")).toArray();
            if (!inputs.isEmpty()) {
                const QJsonObject input = inputs.first().toObject();
                const QJsonArray shape = input.value(QStringLiteral("shape")).toArray();
                QStringList dims;
                for (const QJsonValue &dim : shape)
                    dims << QString::number(dim.toInt());
                if (!dims.isEmpty())
                    details.input = dims.join(QStringLiteral("x"));
            }
        }
    }

    return details;
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
    mainLayout->setSpacing(10);

    auto *toolbar = new QHBoxLayout;
    m_boardFilter = new QComboBox(this);
    m_boardFilter->addItems({tr("Tüm Kartlar"), "NUCLEO-H723ZG", "STM32F407 Discovery", "STM32N6570-DK"});
    m_modelFilter = new QComboBox(this);
    m_modelFilter->addItems({tr("Tüm Model Türleri"), "INT8", "Float32", "Dynamic Q"});
    auto *refreshBtn = new QPushButton(tr("Yenile"), this);
    auto *deleteBtn = new QPushButton(tr("Seçili Kaydı Sil"), this);
    deleteBtn->setObjectName("dangerBtn");
    m_flashCompiledBtn = new QPushButton(tr("Seçili Modeli Tekrar Yükle"), this);
    auto *csvBtn = new QPushButton(tr("CSV Dışa Aktar"), this);
    auto *pdfBtn = new QPushButton(tr("PDF Rapor"), this);

    toolbar->addWidget(new QLabel(tr("Kart:"), this));
    toolbar->addWidget(m_boardFilter);
    toolbar->addWidget(new QLabel(tr("Model:"), this));
    toolbar->addWidget(m_modelFilter);
    toolbar->addStretch();
    toolbar->addWidget(refreshBtn);
    toolbar->addWidget(deleteBtn);
    toolbar->addWidget(m_flashCompiledBtn);
    toolbar->addWidget(csvBtn);
    toolbar->addWidget(pdfBtn);
    mainLayout->addLayout(toolbar);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createAnalysisPage(QStringLiteral("benchmark")), tr("Benchmark Analizleri"));
    m_tabs->addTab(createAnalysisPage(QStringLiteral("simulation")), tr("Simülasyon Veri Analizleri"));
    m_tabs->addTab(createAnalysisPage(QStringLiteral("sensor")), tr("Gerçek Sensör Veri Analizleri"));
    m_tabs->addTab(createAnalysisPage(QStringLiteral("compiled")), tr("Derlenen Modeller"));
    mainLayout->addWidget(m_tabs, 1);
    loadPersistentRecords();

    connect(refreshBtn, &QPushButton::clicked, this, &AnalysisTab::onRefreshClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &AnalysisTab::onDeleteClicked);
    connect(m_flashCompiledBtn, &QPushButton::clicked, this, &AnalysisTab::onFlashCompiledClicked);
    connect(csvBtn, &QPushButton::clicked, this, &AnalysisTab::onExportCsvClicked);
    connect(pdfBtn, &QPushButton::clicked, this, &AnalysisTab::onExportPdfClicked);
    connect(m_boardFilter, &QComboBox::currentTextChanged, this, &AnalysisTab::applyFilters);
    connect(m_modelFilter, &QComboBox::currentTextChanged, this, &AnalysisTab::applyFilters);
    connect(m_tabs, &QTabWidget::currentChanged, this, &AnalysisTab::applyFilters);

    applyFilters();
}

QWidget *AnalysisTab::createAnalysisPage(const QString &kind)
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 10, 0, 0);
    layout->setSpacing(10);

    auto *summaryGrid = new QGridLayout;
    summaryGrid->setSpacing(8);

    QString chartTitle;
    QStringList chartCategories;
    QList<qreal> chartValues;
    QString resourceTitle;
    QList<qreal> ramValues;
    QList<qreal> flashValues;
    QList<qreal> weightValues;

    if (kind == "benchmark") {
        summaryGrid->addWidget(makeMetric(tr("Kayıtlı Test"), "18", page)->parentWidget(), 0, 0);
        summaryGrid->addWidget(makeMetric(tr("En İyi Ortalama"), "0.93 ms", page)->parentWidget(), 0, 1);
        summaryGrid->addWidget(makeMetric(tr("En Düşük RAM"), "6.44 KiB", page)->parentWidget(), 0, 2);
        summaryGrid->addWidget(makeMetric(tr("En Hızlı Kart"), "NUCLEO-H723ZG", page)->parentWidget(), 0, 3);
        chartTitle = tr("Benchmark Ortalama Inference Süresi");
        chartCategories = {"INT8-H7", "Float32-H7", "INT8-F4", "INT8-N6"};
        chartValues = {0.93, 2.84, 8.20, 0.61};
        resourceTitle = tr("RAM / Flash / Weights Karşılaştırması");
        ramValues = {6.44, 18.2, 3.0, 22.1};
        flashValues = {65.83, 63.97, 58.2, 128.4};
        weightValues = {6.70, 25.2, 12.4, 96.8};
    } else if (kind == "simulation") {
        summaryGrid->addWidget(makeMetric(tr("Simülasyon Oturumu"), "12", page)->parentWidget(), 0, 0);
        summaryGrid->addWidget(makeMetric(tr("Toplam Örnek"), "48,000", page)->parentWidget(), 0, 1);
        summaryGrid->addWidget(makeMetric(tr("Anomali Oranı"), "7.8%", page)->parentWidget(), 0, 2);
        summaryGrid->addWidget(makeMetric(tr("Kararlılık"), "98.4%", page)->parentWidget(), 0, 3);
        chartTitle = tr("Simülasyon Testlerinde Ortalama Güven");
        chartCategories = {"BME280", "MPU6050", "PDM MIC", "Karma"};
        chartValues = {96.1, 93.4, 89.8, 94.2};
        resourceTitle = tr("Simülasyon Oturumlarında Kaynak Kullanımı");
        ramValues = {6.44, 3.0, 22.1, 12.8};
        flashValues = {65.83, 58.2, 128.4, 74.6};
        weightValues = {6.70, 12.4, 96.8, 18.9};
    } else if (kind == "sensor") {
        summaryGrid->addWidget(makeMetric(tr("Saha Oturumu"), "9", page)->parentWidget(), 0, 0);
        summaryGrid->addWidget(makeMetric(tr("Sensör Örneği"), "21,640", page)->parentWidget(), 0, 1);
        summaryGrid->addWidget(makeMetric(tr("Ortalama Güven"), "91.6%", page)->parentWidget(), 0, 2);
        summaryGrid->addWidget(makeMetric(tr("Hata Kaydı"), "3", page)->parentWidget(), 0, 3);
        chartTitle = tr("Gerçek Sensör Verisinde Model Güveni");
        chartCategories = {"Normal", "Anomali", "Gürültülü", "Eksik Veri"};
        chartValues = {94.7, 88.2, 76.5, 69.3};
        resourceTitle = tr("Gerçek Sensör Testlerinde Kaynak Kullanımı");
        ramValues = {6.44, 6.44, 6.44, 6.44};
        flashValues = {65.83, 65.83, 65.83, 65.83};
        weightValues = {6.70, 6.70, 6.70, 6.70};
    } else {
        summaryGrid->addWidget(makeMetric(tr("Arşivlenen Model"), "4", page)->parentWidget(), 0, 0);
        summaryGrid->addWidget(makeMetric(tr("En Küçük Firmware"), "58.2 KiB", page)->parentWidget(), 0, 1);
        summaryGrid->addWidget(makeMetric(tr("En Düşük MACC"), "15,136", page)->parentWidget(), 0, 2);
        summaryGrid->addWidget(makeMetric(tr("Son Derleme"), "INT8-H7", page)->parentWidget(), 0, 3);
        chartTitle = tr("Derlenen Modellerde MACC");
        chartCategories = {"anomaly", "har_mlp", "kws_lstm", "float32"};
        chartValues = {15136, 22800, 90400, 15136};
        resourceTitle = tr("Derlenen Model Artefakt Boyutları");
        ramValues = {6.44, 3.0, 22.1, 18.2};
        flashValues = {65.83, 58.2, 128.4, 63.97};
        weightValues = {6.70, 12.4, 96.8, 25.2};
    }
    layout->addLayout(summaryGrid);

    auto *tableBox = new QGroupBox(tr("Detaylı Test Kayıtları"), page);
    auto *tableLayout = new QVBoxLayout(tableBox);
    auto *table = new QTableWidget(0, 12, tableBox);
    if (kind == "compiled") {
        table->setHorizontalHeaderLabels({
            tr("Tarih"), tr("Model"), tr("Tür"), tr("Kart"), tr("Chip"),
            tr("Sensör"), tr("Input"), tr("Params"), tr("MACC"), tr("Weights"),
            tr("Firmware"), tr("Arşiv")
        });
    } else {
        table->setHorizontalHeaderLabels({
            tr("Tarih"), tr("Oturum"), tr("Kart"), tr("Chip"), tr("CPU"),
            tr("Model"), tr("Tür"), tr("Sensör"), tr("Ortalama"), tr("RAM"),
            tr("Weights"), tr("Sonuç")
        });
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setMinimumSectionSize(72);
    table->setWordWrap(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(320);

    if (kind == "benchmark") {
        m_benchmarkTable = table;
        populateBenchmarkData(table);
    } else if (kind == "simulation") {
        m_simulationTable = table;
        populateSimulationData(table);
    } else if (kind == "sensor") {
        m_sensorTable = table;
        populateSensorData(table);
    } else {
        m_compiledModelTable = table;
        populateCompiledModelData(table);
    }

    tableLayout->addWidget(table);
    layout->addWidget(tableBox, 3);

    auto *chartsRow = new QHBoxLayout;
    chartsRow->setSpacing(10);

    auto *latencyBox = new QGroupBox(tr("Performans Grafiği"), page);
    auto *latencyLayout = new QVBoxLayout(latencyBox);
    latencyLayout->addWidget(createChart(chartTitle, chartCategories, chartValues));
    chartsRow->addWidget(latencyBox, 1);

    auto *resourceBox = new QGroupBox(tr("Kaynak Kullanımı"), page);
    auto *resourceLayout = new QVBoxLayout(resourceBox);
    resourceLayout->addWidget(createResourceChart(resourceTitle, chartCategories,
                                                 ramValues, flashValues, weightValues));
    chartsRow->addWidget(resourceBox, 1);
    layout->addLayout(chartsRow, 2);

    return page;
}

QChartView *AnalysisTab::createChart(const QString &title,
                                     const QStringList &categories,
                                     const QList<qreal> &values)
{
    auto *set = new QBarSet(title);
    set->setColor(QColor("#89B4FA"));
    for (qreal value : values)
        *set << value;

    auto *series = new QBarSeries;
    series->append(set);

    auto *chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(title);
    chart->setBackgroundBrush(QColor("#181825"));
    chart->setTitleBrush(QColor("#CDD6F4"));

    auto *axisX = new QBarCategoryAxis;
    axisX->append(categories);
    axisX->setLabelsColor(QColor("#CDD6F4"));
    axisX->setGridLineColor(QColor("#45475A"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis;
    axisY->setRange(0, values.isEmpty() ? 10 : (*std::max_element(values.begin(), values.end()) * 1.25));
    axisY->setLabelsColor(QColor("#CDD6F4"));
    axisY->setGridLineColor(QColor("#45475A"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->hide();
    auto *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setMinimumHeight(320);
    m_chartView = view;
    return view;
}

QChartView *AnalysisTab::createResourceChart(const QString &title,
                                             const QStringList &categories,
                                             const QList<qreal> &ramValues,
                                             const QList<qreal> &flashValues,
                                             const QList<qreal> &weightValues)
{
    auto *ramSet = new QBarSet(tr("RAM KiB"));
    auto *flashSet = new QBarSet(tr("Firmware KiB"));
    auto *weightSet = new QBarSet(tr("Weights KiB"));
    ramSet->setColor(QColor("#A6E3A1"));
    flashSet->setColor(QColor("#89B4FA"));
    weightSet->setColor(QColor("#F9E2AF"));

    qreal maxValue = 10.0;
    const int count = categories.size();
    for (int i = 0; i < count; ++i) {
        const qreal ram = ramValues.value(i);
        const qreal flash = flashValues.value(i);
        const qreal weight = weightValues.value(i);
        *ramSet << ram;
        *flashSet << flash;
        *weightSet << weight;
        maxValue = qMax(maxValue, qMax(ram, qMax(flash, weight)));
    }

    auto *series = new QBarSeries;
    series->append(ramSet);
    series->append(flashSet);
    series->append(weightSet);

    auto *chart = new QChart;
    chart->addSeries(series);
    chart->setTitle(title);
    chart->setBackgroundBrush(QColor("#181825"));
    chart->setTitleBrush(QColor("#CDD6F4"));

    auto *axisX = new QBarCategoryAxis;
    axisX->append(categories);
    axisX->setLabelsColor(QColor("#CDD6F4"));
    axisX->setGridLineColor(QColor("#45475A"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto *axisY = new QValueAxis;
    axisY->setRange(0, maxValue * 1.25);
    axisY->setTitleText(tr("KiB"));
    axisY->setTitleBrush(QColor("#CDD6F4"));
    axisY->setLabelsColor(QColor("#CDD6F4"));
    axisY->setGridLineColor(QColor("#45475A"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor("#CDD6F4"));

    auto *view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setMinimumHeight(260);
    return view;
}

void AnalysisTab::populateBenchmarkData(QTableWidget *table)
{
    const QList<QStringList> rows = {
        {"2026-05-24 12:41", "BENCH-001", "NUCLEO-H723ZG", "STM32H723ZG", "Cortex-M7", "anomaly_cnn", "INT8", "BME280", "0.934 ms", "6.44 KiB", "6.70 KiB", "Tamamlandı"},
        {"2026-05-24 11:58", "BENCH-002", "NUCLEO-H723ZG", "STM32H723ZG", "Cortex-M7", "anomaly_cnn", "Float32", "BME280", "2.84 ms", "18.2 KiB", "25.2 KiB", "Tamamlandı"},
        {"2026-05-22 18:12", "BENCH-003", "STM32F407 Discovery", "STM32F407VG", "Cortex-M4", "har_mlp", "INT8", "MPU6050", "8.20 ms", "3.00 KiB", "12.4 KiB", "Tamamlandı"},
        {"2026-05-20 16:40", "BENCH-004", "STM32N6570-DK", "STM32N657", "Cortex-M55/NPU", "kws_lstm", "INT8", "PDM_MIC", "0.61 ms", "22.1 KiB", "96.8 KiB", "Tamamlandı"},
    };
    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row)
        for (int col = 0; col < rows[row].size(); ++col)
            table->setItem(row, col, makeItem(rows[row][col]));
}

void AnalysisTab::populateSimulationData(QTableWidget *table)
{
    const QList<QStringList> rows = {
        {"2026-05-24 13:14", "SIM-001", "NUCLEO-H723ZG", "STM32H723ZG", "Cortex-M7", "anomaly_cnn", "INT8", "BME280", "0.941 ms", "6.44 KiB", "6.70 KiB", "normal 92%"},
        {"2026-05-24 13:02", "SIM-002", "NUCLEO-H723ZG", "STM32H723ZG", "Cortex-M7", "anomaly_cnn", "INT8", "BME280", "0.948 ms", "6.44 KiB", "6.70 KiB", "anomaly 88%"},
        {"2026-05-23 21:18", "SIM-003", "STM32F407 Discovery", "STM32F407VG", "Cortex-M4", "har_mlp", "INT8", "MPU6050", "8.35 ms", "3.00 KiB", "12.4 KiB", "walking 96%"},
        {"2026-05-21 15:05", "SIM-004", "STM32N6570-DK", "STM32N657", "Cortex-M55/NPU", "kws_lstm", "INT8", "PDM_MIC", "0.72 ms", "22.1 KiB", "96.8 KiB", "keyword 91%"},
    };
    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row)
        for (int col = 0; col < rows[row].size(); ++col)
            table->setItem(row, col, makeItem(rows[row][col]));
}

void AnalysisTab::populateSensorData(QTableWidget *table)
{
    const QList<QStringList> rows = {
        {"2026-05-24 13:30", "REAL-001", "NUCLEO-H723ZG", "STM32H723ZG", "Cortex-M7", "anomaly_cnn", "INT8", "BME280", "0.956 ms", "6.44 KiB", "6.70 KiB", "normal 90%"},
        {"2026-05-24 13:35", "REAL-002", "NUCLEO-H723ZG", "STM32H723ZG", "Cortex-M7", "anomaly_cnn", "INT8", "BME280", "0.961 ms", "6.44 KiB", "6.70 KiB", "anomaly 84%"},
        {"2026-05-22 10:14", "REAL-003", "STM32F407 Discovery", "STM32F407VG", "Cortex-M4", "har_mlp", "INT8", "MPU6050", "8.62 ms", "3.00 KiB", "12.4 KiB", "standing 93%"},
        {"2026-05-20 09:44", "REAL-004", "STM32N6570-DK", "STM32N657", "Cortex-M55/NPU", "kws_lstm", "INT8", "PDM_MIC", "0.79 ms", "22.1 KiB", "96.8 KiB", "noise 76%"},
    };
    table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row)
        for (int col = 0; col < rows[row].size(); ++col)
            table->setItem(row, col, makeItem(rows[row][col]));
}

void AnalysisTab::populateCompiledModelData(QTableWidget *table)
{
    table->setRowCount(0);
}

void AnalysisTab::loadPersistentRecords()
{
    const QVector<AnalysisRecord> benchmarkRecords =
        m_analysisManager->records(QStringLiteral("benchmark"));
    for (auto it = benchmarkRecords.crbegin(); it != benchmarkRecords.crend(); ++it) {
        ensureFilterOption(m_boardFilter, it->cells.value(2));
        ensureFilterOption(m_modelFilter, it->cells.value(6));
        prependRow(m_benchmarkTable, it->cells, it->id);
    }

    const QVector<AnalysisRecord> simulationRecords =
        m_analysisManager->records(QStringLiteral("simulation"));
    for (auto it = simulationRecords.crbegin(); it != simulationRecords.crend(); ++it) {
        ensureFilterOption(m_boardFilter, it->cells.value(2));
        ensureFilterOption(m_modelFilter, it->cells.value(6));
        prependRow(m_simulationTable, it->cells, it->id);
    }

    const QVector<AnalysisRecord> sensorRecords =
        m_analysisManager->records(QStringLiteral("sensor"));
    for (auto it = sensorRecords.crbegin(); it != sensorRecords.crend(); ++it) {
        ensureFilterOption(m_boardFilter, it->cells.value(2));
        ensureFilterOption(m_modelFilter, it->cells.value(6));
        prependRow(m_sensorTable, it->cells, it->id);
    }

    const QVector<AnalysisRecord> compiledRecords =
        m_analysisManager->records(QStringLiteral("compiled"));
    for (auto it = compiledRecords.crbegin(); it != compiledRecords.crend(); ++it) {
        ensureFilterOption(m_boardFilter, it->cells.value(3));
        ensureFilterOption(m_modelFilter, it->cells.value(2));
        prependRow(m_compiledModelTable, it->cells, it->id);
    }
}

void AnalysisTab::prependRow(QTableWidget *table, const QStringList &cells, int recordId)
{
    if (!table)
        return;

    table->insertRow(0);
    for (int col = 0; col < table->columnCount(); ++col) {
        QTableWidgetItem *item = makeItem(cells.value(col, QStringLiteral("--")));
        if (col == 0) {
            item->setData(Qt::UserRole, recordId);
            item->setData(Qt::UserRole + 1, cells.value(12));
            item->setData(Qt::UserRole + 2, cells.value(1));
        }
        table->setItem(0, col, item);
    }
}

void AnalysisTab::ensureFilterOption(QComboBox *combo, const QString &text)
{
    if (!combo || text.isEmpty() || text == QStringLiteral("--"))
        return;
    if (combo->findText(text) < 0)
        combo->addItem(text);
}

void AnalysisTab::addBenchmarkResult(const BenchData &data, const BoardInfo &board)
{
    const QString model = data.model.isEmpty() ? QStringLiteral("--") : data.model;
    const QString boardName = boardNameForRow(data.card, board);
    const QString modelType = modelTypeFromName(model);
    const ModelReportDetails report = loadModelReportDetails();
    const QStringList cells = {
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"),
        QString("BENCH-LIVE-%1").arg(data.samples),
        boardName,
        chipNameForRow(board),
        cpuNameForRow(board),
        model,
        modelType,
        report.sensor,
        QString("%1 ms").arg(data.avg_us / 1000.0, 0, 'f', 3),
        QString("%1 KiB").arg(data.ram_b / 1024.0, 0, 'f', 2),
        report.weights,
        QString("label=%1").arg(data.label.isEmpty() ? QStringLiteral("--") : data.label)
    };

    ensureFilterOption(m_boardFilter, boardName);
    ensureFilterOption(m_modelFilter, modelType);
    const int id = m_analysisManager->addRecord(QStringLiteral("benchmark"), cells);
    prependRow(m_benchmarkTable, cells, id);
    applyFilters();
}

void AnalysisTab::addSimulationResult(const InferenceData &data, const BoardInfo &board, quint32 samples)
{
    const QString model = data.model.isEmpty() ? QStringLiteral("--") : data.model;
    const QString boardName = boardNameForRow(data.card, board);
    const QString modelType = modelTypeFromName(model);
    const ModelReportDetails report = loadModelReportDetails();
    const QStringList cells = {
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"),
        QString("SIM-LIVE-%1").arg(m_simulationTable ? m_simulationTable->rowCount() + 1 : 1),
        boardName,
        chipNameForRow(board),
        cpuNameForRow(board),
        model,
        modelType,
        report.sensor == QStringLiteral("--") ? QStringLiteral("UART Sim") : report.sensor,
        QString("%1 ms").arg(data.inf_us / 1000.0, 0, 'f', 3),
        QString("%1 KiB").arg(data.ram_b / 1024.0, 0, 'f', 2),
        report.weights,
        QString("avg %1% | last=%2 | n=%3")
            .arg(data.acc_pct)
            .arg(data.label.isEmpty() ? QStringLiteral("--") : data.label)
            .arg(samples)
    };

    ensureFilterOption(m_boardFilter, boardName);
    ensureFilterOption(m_modelFilter, modelType);
    const int id = m_analysisManager->addRecord(QStringLiteral("simulation"), cells);
    prependRow(m_simulationTable, cells, id);
    applyFilters();
}

void AnalysisTab::addSensorResult(const SensorData &data, const BoardInfo &board)
{
    const QString model = data.model.isEmpty() ? QStringLiteral("--") : data.model;
    const QString boardName = boardNameForRow(data.card, board);
    const QString modelType = modelTypeFromName(model);
    const ModelReportDetails report = loadModelReportDetails();
    const QString sensor = data.sensor.isEmpty()
        ? (report.sensor == QStringLiteral("--") ? QStringLiteral("UART Sensor") : report.sensor)
        : data.sensor;
    const QString sampleSummary = data.values.isEmpty()
        ? QStringLiteral("--")
        : data.values.join(data.unit.isEmpty() ? QStringLiteral(", ") : QStringLiteral(" ") + data.unit + QStringLiteral(", "))
            + (data.unit.isEmpty() ? QString() : QStringLiteral(" ") + data.unit);
    const QString result = QString("%1 %2% | n=%3 | %4")
        .arg(data.label.isEmpty() ? QStringLiteral("--") : data.label)
        .arg(data.acc_pct)
        .arg(data.samples)
        .arg(sampleSummary);

    const QStringList cells = {
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"),
        QString("REAL-LIVE-%1").arg(data.seq > 0 ? data.seq : (m_sensorTable ? m_sensorTable->rowCount() + 1 : 1)),
        boardName,
        chipNameForRow(board),
        cpuNameForRow(board),
        model,
        modelType,
        sensor,
        data.inf_us > 0 ? QString("%1 ms").arg(data.inf_us / 1000.0, 0, 'f', 3) : QStringLiteral("--"),
        data.ram_b > 0 ? QString("%1 KiB").arg(data.ram_b / 1024.0, 0, 'f', 2) : QStringLiteral("--"),
        report.weights,
        result
    };

    ensureFilterOption(m_boardFilter, boardName);
    ensureFilterOption(m_modelFilter, modelType);
    const int id = m_analysisManager->addRecord(QStringLiteral("sensor"), cells);
    prependRow(m_sensorTable, cells, id);
    applyFilters();
}

void AnalysisTab::addCompiledModel(const PipelineConfig &config, const BoardInfo &board)
{
    const QString expectedElf = QDir(config.outputDir).filePath(
        QStringLiteral("build/%1_%2.elf").arg(config.modelName, config.targetBoard));
    if (!QFile::exists(expectedElf))
        return;

    const QString archiveRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/compiled_models");
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));
    const QString archiveDirPath = QDir(archiveRoot).filePath(
        QStringLiteral("%1_%2_%3").arg(stamp, config.modelName, config.targetBoard));
    QDir().mkpath(archiveDirPath);

    const QString archivedElf = QDir(archiveDirPath).filePath(QFileInfo(expectedElf).fileName());
    QFile::remove(archivedElf);
    QFile::copy(expectedElf, archivedElf);

    const QString reportPath = QDir(config.outputDir).filePath(
        QStringLiteral("xcubeai_output/network_c_info.json"));
    if (QFile::exists(reportPath)) {
        const QString archivedReport = QDir(archiveDirPath).filePath(QStringLiteral("network_c_info.json"));
        QFile::remove(archivedReport);
        QFile::copy(reportPath, archivedReport);
    }

    const ModelReportDetails report = loadModelReportDetails();
    const QString boardName = boardNameForRow(config.targetBoard, board);
    const QString firmwareSize = QString("%1 KiB")
        .arg(QFileInfo(archivedElf).size() / 1024.0, 0, 'f', 2);
    QString modelType = config.quantization.contains(QStringLiteral("Float"), Qt::CaseInsensitive)
        ? QStringLiteral("Float32")
        : modelTypeFromName(config.modelName);
    if (modelType == QStringLiteral("--"))
        modelType = config.quantization;

    const QStringList cells = {
        QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm"),
        config.modelName,
        modelType,
        boardName,
        chipNameForRow(board),
        config.sensorType,
        report.input,
        report.params,
        report.macc,
        report.weights,
        firmwareSize,
        QStringLiteral("Arşivlendi"),
        archivedElf,
        config.modelPath,
        archiveDirPath
    };

    ensureFilterOption(m_boardFilter, boardName);
    ensureFilterOption(m_modelFilter, modelType);
    const int id = m_analysisManager->addRecord(QStringLiteral("compiled"), cells);
    prependRow(m_compiledModelTable, cells, id);
    applyFilters();
}

QTableWidget *AnalysisTab::currentTable() const
{
    if (!m_tabs)
        return nullptr;
    if (m_tabs->currentIndex() == 0)
        return m_benchmarkTable;
    if (m_tabs->currentIndex() == 1)
        return m_simulationTable;
    if (m_tabs->currentIndex() == 2)
        return m_sensorTable;
    return m_compiledModelTable;
}

void AnalysisTab::applyFilters()
{
    QTableWidget *table = currentTable();
    if (!table || !m_boardFilter || !m_modelFilter)
        return;

    const QString board = m_boardFilter->currentText();
    const QString modelType = m_modelFilter->currentText();
    const bool allBoards = board == tr("Tüm Kartlar");
    const bool allModels = modelType == tr("Tüm Model Türleri");
    const bool compiled = table == m_compiledModelTable;
    const int boardColumn = compiled ? 3 : 2;
    const int modelColumn = compiled ? 2 : 6;

    for (int row = 0; row < table->rowCount(); ++row) {
        const bool boardMatch = allBoards || tableCellText(table, row, boardColumn) == board;
        const bool modelMatch = allModels || tableCellText(table, row, modelColumn) == modelType;
        table->setRowHidden(row, !(boardMatch && modelMatch));
    }
}

void AnalysisTab::onRefreshClicked()
{
    applyFilters();
}

void AnalysisTab::onDeleteClicked()
{
    QTableWidget *table = currentTable();
    if (!table)
        return;

    QList<int> rows;
    const QModelIndexList selected = table->selectionModel()->selectedRows();
    for (const QModelIndex &index : selected)
        rows.append(index.row());
    if (rows.isEmpty()) {
        const QModelIndexList indexes = table->selectionModel()->selectedIndexes();
        for (const QModelIndex &index : indexes) {
            if (!rows.contains(index.row()))
                rows.append(index.row());
        }
    }
    if (rows.isEmpty() && table->currentRow() >= 0)
        rows.append(table->currentRow());

    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for (int row : rows) {
        const QTableWidgetItem *item = table->item(row, 0);
        const int id = item ? item->data(Qt::UserRole).toInt() : -1;
        if (id >= 0)
            m_analysisManager->deleteRecord(id);
        table->removeRow(row);
    }
}

void AnalysisTab::onFlashCompiledClicked()
{
    if (!m_tabs || m_tabs->currentWidget() != m_compiledModelTable->parentWidget()->parentWidget())
        m_tabs->setCurrentIndex(3);

    QTableWidget *table = m_compiledModelTable;
    if (!table)
        return;

    const int row = table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, tr("Model Yükle"),
            tr("Tekrar yüklenecek derlenmiş modeli seçin."));
        return;
    }

    const QTableWidgetItem *first = table->item(row, 0);
    const QString firmwarePath = first ? first->data(Qt::UserRole + 1).toString() : QString();
    const QString modelName = tableCellText(table, row, 1);
    if (firmwarePath.isEmpty() || !QFile::exists(firmwarePath)) {
        QMessageBox::warning(this, tr("Model Yükle"),
            tr("Arşivlenmiş firmware dosyası bulunamadı."));
        return;
    }

    emit flashCompiledModelRequested(firmwarePath, modelName);
}

void AnalysisTab::onExportCsvClicked()
{
    QTableWidget *table = currentTable();
    int visibleRows = 0;
    if (table) {
        for (int row = 0; row < table->rowCount(); ++row)
            if (!table->isRowHidden(row))
                ++visibleRows;
    }
    if (!table || visibleRows == 0) {
        QMessageBox::warning(this, tr("CSV Export"), tr("Dışa aktarılacak kayıt yok."));
        return;
    }

    const QString defaultPath = QDir::homePath() +
        QString("/stm32_ai_analysis_%1.csv").arg(exportTimestamp());
    const QString path = QFileDialog::getSaveFileName(
        this, tr("CSV Export"), defaultPath, tr("CSV Files (*.csv);;All Files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("CSV Export"),
            tr("Dosya yazılamadı:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF);

    QStringList headers;
    for (int col = 0; col < table->columnCount(); ++col)
        headers << csvEscape(headerText(table, col));
    out << headers.join(',') << '\n';

    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->isRowHidden(row))
            continue;
        QStringList cells;
        for (int col = 0; col < table->columnCount(); ++col)
            cells << csvEscape(tableCellText(table, row, col));
        out << cells.join(',') << '\n';
    }

    QMessageBox::information(this, tr("CSV Export"),
        tr("CSV dosyası oluşturuldu:\n%1").arg(path));
}

void AnalysisTab::onExportPdfClicked()
{
    QTableWidget *table = currentTable();
    int visibleRows = 0;
    if (table) {
        for (int row = 0; row < table->rowCount(); ++row)
            if (!table->isRowHidden(row))
                ++visibleRows;
    }
    if (!table || visibleRows == 0) {
        QMessageBox::warning(this, tr("PDF Rapor"), tr("Rapora eklenecek kayıt yok."));
        return;
    }

    const QString defaultPath = QDir::homePath() +
        QString("/stm32_ai_analysis_%1.pdf").arg(exportTimestamp());
    const QString path = QFileDialog::getSaveFileName(
        this, tr("PDF Rapor"), defaultPath, tr("PDF Files (*.pdf);;All Files (*)"));
    if (path.isEmpty())
        return;

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(14, 14, 14, 14), QPageLayout::Millimeter);
    writer.setResolution(96);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        QMessageBox::critical(this, tr("PDF Rapor"), tr("PDF dosyası oluşturulamadı."));
        return;
    }

    const QRect pageRect = writer.pageLayout().paintRectPixels(writer.resolution());
    int y = pageRect.top();
    QFont titleFont = painter.font();
    titleFont.setPointSize(17);
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
                     tr("Oluşturma zamanı: %1")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));
    y += 34;

    const int columns = table->columnCount();
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
            drawTextInCell(painter, cell, headerText(table, col));
        }
        y += rowHeight;
        headerFont.setBold(false);
        painter.setFont(headerFont);
    };

    drawHeader();
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->isRowHidden(row))
            continue;
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
            drawTextInCell(painter, cell, tableCellText(table, row, col));
        }
        y += rowHeight;
    }

    const QList<QChartView *> charts = m_tabs->currentWidget()->findChildren<QChartView *>();
    for (QChartView *chartView : charts) {
        if (!chartView)
            continue;

        y += 26;
        const QPixmap chartPixmap = chartView->grab();
        if (chartPixmap.isNull())
            continue;

        const int targetHeight = 220;
        if (y + targetHeight > pageRect.bottom()) {
            writer.newPage();
            y = pageRect.top();
        }

        const QPixmap scaledChart = chartPixmap.scaled(
            pageRect.width(), targetHeight,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        painter.drawPixmap(pageRect.left(), y, scaledChart);
        y += scaledChart.height();
    }

    painter.end();
    QMessageBox::information(this, tr("PDF Rapor"),
        tr("PDF raporu oluşturuldu:\n%1").arg(path));
}
