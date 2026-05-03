#pragma once

#include <QWidget>

class AnalysisManager;
class QTableWidget;
class QChartView;

class AnalysisTab : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisTab(QWidget *parent = nullptr);
    ~AnalysisTab() override;

private slots:
    void onRefreshClicked();
    void onDeleteClicked();
    void onExportCsvClicked();
    void onExportPdfClicked();

private:
    void setupUi();
    void populateSampleData();

    AnalysisManager *m_analysisManager = nullptr;

    QTableWidget *m_sessionTable = nullptr;
    QChartView   *m_chartView    = nullptr;
};
