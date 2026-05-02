#pragma once

#include <QWidget>

class FlashManager;

// Tab widget for Module 2: Model Loading & Flashing
class FlashTab : public QWidget
{
    Q_OBJECT

public:
    explicit FlashTab(QWidget *parent = nullptr);
    ~FlashTab() override;

private:
    void setupUi();

    FlashManager *m_flashManager = nullptr;
};
