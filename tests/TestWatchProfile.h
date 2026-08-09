#pragma once

#include <QObject>

class TestWatchProfile : public QObject
{
    Q_OBJECT

private slots:
    void buildsOneRowPerItemWithFifteenCellsCorrectlyMapped();
    void emptyItemListProducesNoRows();
};
