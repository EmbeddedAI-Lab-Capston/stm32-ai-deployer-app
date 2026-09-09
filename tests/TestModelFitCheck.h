#pragma once

#include <QObject>

class TestModelFitCheck : public QObject
{
    Q_OBJECT

private slots:
    void realF4ModelFitsRealF4BoardWithNoWarnings();
    void oversizedModelOnTinyBoardWarnsForBothRamAndFlash();
    void tinyFlashOnlyWarnsAboutFlashNotRam();
};
