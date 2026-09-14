#include "N6RamImage.h"

#include <QLatin1Char>
#include <QRegularExpression>

namespace N6RamImage {

QString hex32(quint32 value)
{
    return QStringLiteral("0x%1").arg(value, 8, 16, QLatin1Char('0'));
}

bool vectorLooksValid(quint32 initialSp, quint32 resetHandler)
{
    const bool spInRam = initialSp > kRamBegin && initialSp <= kRamEnd;
    const bool spAligned = (initialSp & 0x7U) == 0;
    const quint32 pc = resetHandler & ~1U;
    const bool pcInRam = pc >= kLoadAddress && pc < kRamEnd;
    // Cortex-M reset handlers are Thumb; a cleared bit 0 means this is not a
    // vector table at all.
    const bool pcIsThumb = (resetHandler & 1U) != 0;
    return spInRam && spAligned && pcInRam && pcIsThumb;
}

// Shared scan so the single-word and two-word reads cannot drift apart.
static bool scanWords(const QString &output, int wanted, quint32 *out)
{
    static const QRegularExpression wordRe(
        QStringLiteral("0x[0-9A-Fa-f]{8}\\s*:\\s*((?:[0-9A-Fa-f]{8}\\s*)+)"));
    static const QRegularExpression splitRe(QStringLiteral("\\s+"));

    int found = 0;
    QRegularExpressionMatchIterator it = wordRe.globalMatch(output);
    while (it.hasNext() && found < wanted) {
        const QString group = it.next().captured(1).trimmed();
        const QStringList words = group.split(splitRe, Qt::SkipEmptyParts);
        for (const QString &word : words) {
            if (found >= wanted)
                break;
            bool ok = false;
            const quint32 parsed = word.toUInt(&ok, 16);
            if (!ok)
                return false;
            out[found++] = parsed;
        }
    }
    return found == wanted;
}

bool parseCliWord32(const QString &output, quint32 &value)
{
    quint32 word = 0;
    if (!scanWords(output, 1, &word))
        return false;
    value = word;
    return true;
}

bool parseCliVector(const QString &output, quint32 &initialSp, quint32 &resetHandler)
{
    quint32 words[2] = {0, 0};
    if (!scanWords(output, 2, words))
        return false;
    initialSp = words[0];
    resetHandler = words[1];
    return true;
}

QStringList connectArgsWithMode(const QStringList &connectArgs, const QString &mode)
{
    QStringList args;
    for (const QString &arg : connectArgs) {
        if (arg.startsWith(QStringLiteral("mode="), Qt::CaseInsensitive))
            continue;
        args << arg;
    }
    args << QStringLiteral("mode=%1").arg(mode);
    return args;
}

QStringList resetArgs(const QStringList &connectArgs)
{
    QStringList args{QStringLiteral("-c")};
    args += connectArgsWithMode(connectArgs, QStringLiteral("UR"));
    args << QStringLiteral("-run");
    return args;
}

QStringList bootPinsArgs(const QStringList &connectArgs)
{
    QStringList args{QStringLiteral("-c")};
    args += connectArgsWithMode(connectArgs, QStringLiteral("UR"));
    args << QStringLiteral("-r32") << hex32(kBootsrAddress) << QStringLiteral("0x4");
    return args;
}

QStringList vectorReadArgs(const QStringList &connectArgs)
{
    QStringList args{QStringLiteral("-c")};
    args += connectArgsWithMode(connectArgs, QStringLiteral("HOTPLUG"));
    args << QStringLiteral("-r32") << hex32(kLoadAddress) << QStringLiteral("0x8");
    return args;
}

QStringList armArgs(const QStringList &connectArgs,
                    quint32 initialSp,
                    quint32 resetHandler,
                    const QString &binPath)
{
    QStringList args{QStringLiteral("-c")};
    args += connectArgsWithMode(connectArgs, QStringLiteral("HOTPLUG"));
    args << QStringLiteral("-halt");
    if (!binPath.isEmpty())
        args << QStringLiteral("-w") << binPath << hex32(kLoadAddress);
    args << QStringLiteral("-w32") << hex32(kVtorAddress)  << hex32(kLoadAddress)
         << QStringLiteral("-w32") << hex32(kCpacrAddress) << hex32(kCpacrFullAccess)
         << QStringLiteral("-coreReg")
         << QStringLiteral("MSP=%1").arg(hex32(initialSp))
         << QStringLiteral("PC=%1").arg(hex32(resetHandler & ~1U))
         << QStringLiteral("-run");
    return args;
}

}  // namespace N6RamImage
