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
            { "STM32N6", 4096, 4096,  800, true },
        };
    }

    inline BoardInfo find(const QString &name)
    {
        const QString needle = name.trimmed();
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
