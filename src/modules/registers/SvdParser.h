#pragma once
#include "SvdModel.h"
#include <QString>

class QXmlStreamReader;

// ── SvdParser ──────────────────────────────────────────────────────────────
// Parses a CMSIS-SVD file into an SvdDevice using QXmlStreamReader (Qt-native,
// no extra dependency). Resolves <peripheral derivedFrom="..."> at load time so
// the returned tree is self-contained. Plain class (no QObject) — pure logic,
// unit-testable without the app. See docs/register_inspector_plan.md Bolum 3.1.
//
// NOTE: the three shipped SVDs (F407/H723/N657) contain no <dim> arrays, so dim
// expansion is not implemented yet. TODO(faz-1): add dim/dimIncrement/dimIndex
// support if a future SVD needs it (parser must not silently drop such registers).
class SvdParser
{
public:
    SvdDevice parseFile(const QString &path);
    QString   errorString() const { return m_error; }

private:
    void parseDevice(QXmlStreamReader &xml, SvdDevice &dev);
    void parseCpu(QXmlStreamReader &xml, SvdCpu &cpu);
    // Parses one <peripheral>; fills `p` and returns its derivedFrom (or empty).
    QString parsePeripheral(QXmlStreamReader &xml, SvdPeripheral &p, const SvdDevice &dev);
    void parseAddressBlock(QXmlStreamReader &xml, SvdPeripheral &p);
    void parseRegisters(QXmlStreamReader &xml, SvdPeripheral &p, const SvdDevice &dev);
    SvdRegister parseRegister(QXmlStreamReader &xml, const SvdDevice &dev);
    void parseFields(QXmlStreamReader &xml, SvdRegister &reg);
    SvdField parseField(QXmlStreamReader &xml);
    void parseEnumeratedValues(QXmlStreamReader &xml, SvdField &field);

    // Copy base peripheral's registers/meta into each derivedFrom peripheral.
    void resolveDerivedFrom(QList<SvdPeripheral> &peripherals,
                            const QList<QString> &derivedFrom);

    QString m_error;
};
