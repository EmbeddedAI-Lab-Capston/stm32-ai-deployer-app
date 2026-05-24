#pragma once

#include <QList>
#include <QStringList>
#include <QWidget>

#include "modules/board/BoardPresets.h"
#include "modules/flash/PipelineConfig.h"
#include "modules/serial/PacketParser.h"

class AnalysisManager;
class QTableWidget;
class QChartView;
class QTabWidget;
class QWidget;
class QComboBox;
class QPushButton;

class AnalysisTab : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisTab(QWidget *parent = nullptr);
    ~AnalysisTab() override;

public slots:
    void addBenchmarkResult(const BenchData &data, const BoardInfo &board);
    void addSimulationResult(const InferenceData &data, const BoardInfo &board, quint32 samples = 1);
    void addCompiledModel(const PipelineConfig &config, const BoardInfo &board);

signals:
    void flashCompiledModelRequested(const QString &firmwarePath, const QString &modelName);

private slots:
    void onRefreshClicked();
    void onDeleteClicked();
    void onExportCsvClicked();
    void onExportPdfClicked();
    void applyFilters();

private:
    void setupUi();
    QWidget *createAnalysisPage(const QString &kind);
    QTableWidget *currentTable() const;
    QChartView *createChart(const QString &title,
                            const QStringList &categories,
                            const QList<qreal> &values);
    QChartView *createResourceChart(const QString &title,
                                    const QStringList &categories,
                                    const QList<qreal> &ramValues,
                                    const QList<qreal> &flashValues,
                                    const QList<qreal> &weightValues);
    void populateBenchmarkData(QTableWidget *table);
    void populateSimulationData(QTableWidget *table);
    void populateSensorData(QTableWidget *table);
    void populateCompiledModelData(QTableWidget *table);
    void loadPersistentRecords();
    void prependRow(QTableWidget *table, const QStringList &cells, int recordId = -1);
    void ensureFilterOption(QComboBox *combo, const QString &text);
    void onFlashCompiledClicked();

    AnalysisManager *m_analysisManager = nullptr;

    QTabWidget *m_tabs = nullptr;
    QComboBox *m_boardFilter = nullptr;
    QComboBox *m_modelFilter = nullptr;
    QTableWidget *m_benchmarkTable = nullptr;
    QTableWidget *m_simulationTable = nullptr;
    QTableWidget *m_sensorTable = nullptr;
    QTableWidget *m_compiledModelTable = nullptr;
    QChartView *m_chartView = nullptr;
    QPushButton *m_flashCompiledBtn = nullptr;
};
