#pragma once

#include <QWidget>

class AnalysisManager;

// Tab widget for Module 4: Analysis & Comparison
class AnalysisTab : public QWidget
{
    Q_OBJECT

public:
    explicit AnalysisTab(QWidget *parent = nullptr);
    ~AnalysisTab() override;

private:
    void setupUi();

    AnalysisManager *m_analysisManager = nullptr;
};
