#include "ElfTargetMatcher.h"

namespace {

bool findSymbol(const QList<Symbol> &symbols, const QString &name, quint64 *addrOut)
{
    for (const Symbol &s : symbols) {
        if (s.name == name) {
            if (addrOut) *addrOut = s.address;
            return true;
        }
    }
    return false;
}

quint32 u32le(const QByteArray &b, int off)
{
    return quint32(static_cast<unsigned char>(b.at(off)))
         | (quint32(static_cast<unsigned char>(b.at(off + 1))) << 8)
         | (quint32(static_cast<unsigned char>(b.at(off + 2))) << 16)
         | (quint32(static_cast<unsigned char>(b.at(off + 3))) << 24);
}

}

ElfMatchReport ElfTargetMatcher::evaluate(quint64 vtor, const QByteArray &firstEightBytes,
                                          const QList<Symbol> &symbols)
{
    ElfMatchReport report;
    report.vtor = vtor;

    quint64 estack = 0, resetHandler = 0;
    const bool hasEstack       = findSymbol(symbols, QStringLiteral("_estack"), &estack);
    const bool hasResetHandler = findSymbol(symbols, QStringLiteral("Reset_Handler"), &resetHandler);

    if (!hasEstack || !hasResetHandler) {
        // No fabricated comparison — a missing symbol means "we don't know",
        // never "assume mismatch" or "assume match".
        report.result = ElfMatchResult::Unknown;
        report.detail = QStringLiteral("ELF'te %1 sembolu bulunamadi - karsilastirma yapilamiyor")
                             .arg(!hasEstack ? QStringLiteral("_estack") : QStringLiteral("Reset_Handler"));
        return report;
    }
    report.elfEstack       = estack;
    report.elfResetHandler = resetHandler;

    if (firstEightBytes.size() < 8) {
        report.result = ElfMatchResult::Unknown;
        report.detail = QStringLiteral("Vektor tablosu okunamadi (beklenen 8 bayt, gelen %1)")
                             .arg(firstEightBytes.size());
        return report;
    }

    report.targetInitialSp = u32le(firstEightBytes, 0);
    report.targetResetVec  = u32le(firstEightBytes, 4);

    report.spMatches    = (quint64(report.targetInitialSp) == report.elfEstack);
    // Thumb bit: a Cortex-M code address always has bit0 set in the vector
    // table entry, even though the symbol's own address does not.
    report.resetMatches = (quint64(report.targetResetVec) == (report.elfResetHandler | 1ull));

    if (report.spMatches && report.resetMatches) {
        report.result = ElfMatchResult::Match;
        report.detail = QStringLiteral("ELF hedef firmware ile eslesiyor (SP ve reset vektoru tutuyor)");
    } else {
        report.result = ElfMatchResult::Mismatch;
        report.detail = QStringLiteral(
            "Yuklenen ELF karttaki firmware ile eslesmiyor olabilir - degerler yanlis olabilir "
            "(SP eslesme=%1, reset vektoru eslesme=%2)")
                             .arg(report.spMatches ? QStringLiteral("evet") : QStringLiteral("hayir"))
                             .arg(report.resetMatches ? QStringLiteral("evet") : QStringLiteral("hayir"));
    }
    return report;
}
