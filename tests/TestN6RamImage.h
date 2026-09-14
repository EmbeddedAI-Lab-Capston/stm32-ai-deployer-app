#pragma once

#include <QObject>

class TestN6RamImage : public QObject
{
    Q_OBJECT

private slots:
    void parsesSingleWordFromCliOutput();
    void parsesVectorPairFromOneLine();
    void parsesVectorPairAcrossSeparateReads();
    void rejectsOutputWithoutWords();

    void acceptsRealImageVector();
    void rejectsClearedRam();
    void rejectsNonThumbResetHandler();
    void rejectsStackPointerOutsideRam();

    void connectArgsReplaceExistingMode();
    void resetUsesRunNotRst();
    void armArgsCarryVtorCpacrAndCoreRegs();
    void armArgsOmitWriteWhenNoBinaryGiven();
};
