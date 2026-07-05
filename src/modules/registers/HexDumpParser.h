#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <QtGlobal>

// ── HexDumpParser ──────────────────────────────────────────────────────────
// Pure parser for STM32_Programmer_CLI `-r32` output. Anchors on the address
// field of each data line ("0xADDR : W0 W1 W2 W3"), never on line order — the
// grammar and rationale are recorded in docs/register_inspector_findings.md
// (Faz 0). No QObject / no CLI dependency, so it is unit-tested directly against
// the captured fixtures in docs/register_fixtures/.
namespace HexDumpParser {

// Parse every "0xADDR : words" data line into addr -> 32-bit value. Each word
// is the value at addr, addr+4, ... . All other lines (banner, headers, blanks)
// are ignored.
QHash<quint64, quint32> parse(const QStringList &lines);
QHash<quint64, quint32> parse(const QString &text);

// Lines that look like CLI read errors ("Error", "cannot read", "not readable").
// Used for block-error attribution; the snapshot survives partial failures.
QStringList errorMarkers(const QStringList &lines);

} // namespace HexDumpParser
