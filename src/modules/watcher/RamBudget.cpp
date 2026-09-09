#include "RamBudget.h"

#include <algorithm>

RamBudgetResult computeRamBudget(const RamBudgetInput &in)
{
    RamBudgetResult out;

    if (in.ramTotalBytes == 0) {
        out.warning = QStringLiteral("Kart RAM boyutu bilinmiyor");
        return out;
    }
    if (!in.ramTopKnown) {
        out.warning = QStringLiteral("_estack sembolü bulunamadı (RAM tepesi bilinmiyor)");
        return out;
    }
    if (!in.staticEndKnown) {
        out.warning = QStringLiteral("_end sembolü bulunamadı (statik veri sonu bilinmiyor)");
        return out;
    }
    if (!in.stackKnown) {
        out.warning = QStringLiteral("stackWatermark kalemi izlenmiyor veya henüz değer okumadı");
        return out;
    }

    out.ramTotal  = in.ramTotalBytes;
    out.ramTop    = in.ramTopAddr;
    out.ramBase   = (in.ramTopAddr > in.ramTotalBytes) ? (in.ramTopAddr - in.ramTotalBytes) : 0;
    out.staticEnd = in.staticEndAddr;
    out.heapTop   = in.heapKnown ? in.heapTopAddr : 0;
    out.stackDip  = in.stackBaseAddr + in.stackHeadroomBytes;

    const quint64 staticOrHeapEnd = std::max(out.staticEnd, out.heapTop);
    out.freeBytes = qint64(out.stackDip) - qint64(staticOrHeapEnd);

    out.staticUsed = (out.staticEnd > out.ramBase) ? (out.staticEnd - out.ramBase) : 0;
    out.heapUsed   = (out.heapTop > out.staticEnd) ? (out.heapTop - out.staticEnd) : 0;
    out.stackUsed  = (out.ramTop > out.stackDip) ? (out.ramTop - out.stackDip) : 0;

    out.usedPct = out.ramTotal > 0
        ? 100.0 * double(out.staticUsed + out.heapUsed + out.stackUsed) / double(out.ramTotal)
        : 0.0;

    out.ok = true;
    if (out.freeBytes < 0) {
        out.warning = QStringLiteral("RAM çakışması: stack en derin noktası statik/heap sınırını %1 bayt aştı")
                          .arg(-out.freeBytes);
    }
    return out;
}
