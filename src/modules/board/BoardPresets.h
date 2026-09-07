#pragma once
#include <QString>
#include <QList>
#include <QMetaType>

// ── BoardInfo ─────────────────────────────────────────────────────────────
// Plain data struct describing a single board configuration.
// Registered as a Qt meta-type so it can be stored in QVariant (ComboBox data).
struct BoardInfo
{
    QString name;       // "STM32F4"
    int     flashKb  = 0;   // 1024
    int     ramKb    = 0;   // 192
    int     clockMhz = 0;   // 168
    bool    isPreset = true; // false = custom (user-defined)
    QString portName;
    QString probeBoardName;
    QString deviceId;
    QString revisionId;
    QString deviceName;
    QString nvmSize;
    QString deviceCpu;
    QString stlinkSn;
    QString stlinkFw;
    QString voltage;

    bool isNull() const { return name.isEmpty(); }
};
Q_DECLARE_METATYPE(BoardInfo)

// ── BoardPresets ──────────────────────────────────────────────────────────
namespace BoardPresets
{
    inline QList<BoardInfo> all()
    {
        return {
            { "STM32F4", 1024,  192,  168, true },
            { "STM32H7", 2048, 1024,  480, true },
            { "STM32N6", 65536, 4096, 600, true },
            { "NUCLEO-N657X0-Q", 65536, 4096, 600, true },
        };
    }

    // Resolves a raw/fuzzy board identifier (e.g. "NUCLEO-H743ZI", "H723")
    // to one of the canonical preset names in all(). Only decides WHICH
    // preset matches - never carries its own copy of flash/RAM/clock, so
    // there is exactly one place those numbers are written. (A prior
    // version of this function hardcoded a second copy of the STM32H7
    // entry here that silently drifted from all()'s - 1024/564/550 vs.
    // the real 2048/1024/480 - found live 2026-09-07 comparing the "Kart
    // Seçimi" list against "Aktif Kart Bilgileri" after selecting H7.)
    inline QString canonicalNameFor(const QString &name)
    {
        const QString upper = name.trimmed().toUpper();
        if (upper.contains("NUCLEO-N657X0-Q"))
            return QStringLiteral("NUCLEO-N657X0-Q");
        if (upper.contains("N657") || upper.contains("N655") ||
            upper.contains("STM32N6") || upper.contains("STM32N") ||
            upper.contains("NUCLEO-N6") || upper.contains("CORTEX-M55") ||
            upper.contains("NPU"))
            return QStringLiteral("STM32N6");
        if (upper.contains("H723") || upper.contains("H72") ||
            upper.contains("H73") || upper.contains("STM32H7") ||
            upper.contains("NUCLEO-H7"))
            return QStringLiteral("STM32H7");
        if (upper.contains("STM32F4") || upper.contains("NUCLEO-F4"))
            return QStringLiteral("STM32F4");
        return {};
    }

    inline BoardInfo find(const QString &name)
    {
        const QString needle = name.trimmed();

        const QString canonicalName = canonicalNameFor(needle);
        if (!canonicalName.isEmpty()) {
            for (const auto &b : all())
                if (b.name == canonicalName)
                    return b;
        }

        for (const auto &b : all()) {
            if (b.name.compare(needle, Qt::CaseInsensitive) == 0)
                return b;
            if (needle.contains(b.name, Qt::CaseInsensitive) ||
                b.name.contains(needle, Qt::CaseInsensitive))
                return b;
        }
        return {};
    }

    inline BoardInfo defaultBoard() { return all().first(); }
}
