#pragma once

#include <QWidget>

class BoardManager;

// Tab widget for Module 1: Board Management
class BoardTab : public QWidget
{
    Q_OBJECT

public:
    explicit BoardTab(QWidget *parent = nullptr);
    ~BoardTab() override;

private:
    void setupUi();

    BoardManager *m_boardManager = nullptr;
};
