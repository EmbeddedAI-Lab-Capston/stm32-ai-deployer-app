#include "ModelFitCheck.h"

namespace {
constexpr qint64 kEstFirmwareRamBytes   = 40 * 1024;
constexpr qint64 kEstFirmwareFlashBytes = 60 * 1024;
}

QStringList checkModelFitsBoard(const ModelFootprint &footprint, const BoardInfo &board)
{
    QStringList warnings;

    const qint64 ramTotalBytes   = qint64(board.ramKb)   * 1024;
    const qint64 flashTotalBytes = qint64(board.flashKb) * 1024;
    const qint64 estRamNeed      = footprint.activationsBytes + kEstFirmwareRamBytes;
    const qint64 estFlashNeed    = footprint.weightsBytes + kEstFirmwareFlashBytes;

    if (estRamNeed > ramTotalBytes) {
        warnings << QStringLiteral(
            "UYARI: model + firmware icin tahmini RAM ihtiyaci (~%1 KB) "
            "%2 kartinin RAM'ini (%3 KB) asabilir.")
            .arg(estRamNeed / 1024).arg(board.name).arg(board.ramKb);
    }
    if (estFlashNeed > flashTotalBytes) {
        warnings << QStringLiteral(
            "UYARI: model + firmware icin tahmini Flash ihtiyaci (~%1 KB) "
            "%2 kartinin Flash'ini (%3 KB) asabilir.")
            .arg(estFlashNeed / 1024).arg(board.name).arg(board.flashKb);
    }
    return warnings;
}
