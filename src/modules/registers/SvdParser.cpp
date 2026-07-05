#include "SvdParser.h"

#include <QFile>
#include <QHash>
#include <QXmlStreamReader>

namespace {

// Parse an SVD numeric literal: 0x.. (hex), #.. (binary), or decimal.
quint64 svdNumber(const QString &raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty())
        return 0;
    bool ok = false;
    quint64 v = 0;
    if (s.startsWith(QLatin1String("0x")) || s.startsWith(QLatin1String("0X")))
        v = s.mid(2).toULongLong(&ok, 16);
    else if (s.startsWith(QLatin1Char('#')))
        v = s.mid(1).toULongLong(&ok, 2);
    else
        v = s.toULongLong(&ok, 10);
    return ok ? v : 0;
}

} // namespace

SvdDevice SvdParser::parseFile(const QString &path)
{
    m_error.clear();
    SvdDevice dev;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = QStringLiteral("Cannot open SVD file: %1").arg(path);
        return dev;
    }

    QXmlStreamReader xml(&file);
    if (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("device"))
            parseDevice(xml, dev);
        else
            m_error = QStringLiteral("Root element is not <device>: %1")
                          .arg(xml.name().toString());
    }

    if (xml.hasError()) {
        m_error = QStringLiteral("XML error at line %1: %2")
                      .arg(xml.lineNumber())
                      .arg(xml.errorString());
        return SvdDevice{};   // partial parse is not trustworthy
    }
    return dev;
}

void SvdParser::parseDevice(QXmlStreamReader &xml, SvdDevice &dev)
{
    QList<SvdPeripheral> peripherals;
    QList<QString>       derivedFrom;

    while (xml.readNextStartElement()) {
        const auto n = xml.name();
        if (n == QLatin1String("name"))
            dev.name = xml.readElementText();
        else if (n == QLatin1String("version"))
            dev.version = xml.readElementText();
        else if (n == QLatin1String("description"))
            dev.description = xml.readElementText();
        else if (n == QLatin1String("cpu"))
            parseCpu(xml, dev.cpu);
        else if (n == QLatin1String("size"))
            dev.defaultSizeBits = static_cast<int>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("resetValue"))
            dev.defaultResetValue = static_cast<quint32>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("resetMask"))
            dev.defaultResetMask = static_cast<quint32>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("peripherals")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("peripheral")) {
                    SvdPeripheral p;
                    const QString from = parsePeripheral(xml, p, dev);
                    peripherals.append(p);
                    derivedFrom.append(from);
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else {
            xml.skipCurrentElement();
        }
    }

    resolveDerivedFrom(peripherals, derivedFrom);
    dev.peripherals = peripherals;
}

void SvdParser::parseCpu(QXmlStreamReader &xml, SvdCpu &cpu)
{
    while (xml.readNextStartElement()) {
        const auto n = xml.name();
        if (n == QLatin1String("name"))
            cpu.name = xml.readElementText();
        else if (n == QLatin1String("endian"))
            cpu.endian = xml.readElementText();
        else
            xml.skipCurrentElement();
    }
}

QString SvdParser::parsePeripheral(QXmlStreamReader &xml, SvdPeripheral &p,
                                   const SvdDevice &dev)
{
    // derivedFrom is an attribute on the <peripheral> start element.
    const QString derivedFrom =
        xml.attributes().value(QLatin1String("derivedFrom")).toString();

    while (xml.readNextStartElement()) {
        const auto n = xml.name();
        if (n == QLatin1String("name"))
            p.name = xml.readElementText();
        else if (n == QLatin1String("groupName"))
            p.groupName = xml.readElementText();
        else if (n == QLatin1String("description"))
            p.description = xml.readElementText();
        else if (n == QLatin1String("baseAddress"))
            p.baseAddress = svdNumber(xml.readElementText());
        else if (n == QLatin1String("addressBlock"))
            parseAddressBlock(xml, p);
        else if (n == QLatin1String("registers"))
            parseRegisters(xml, p, dev);
        else
            xml.skipCurrentElement();
    }
    return derivedFrom;
}

void SvdParser::parseAddressBlock(QXmlStreamReader &xml, SvdPeripheral &p)
{
    SvdAddressBlock block;
    while (xml.readNextStartElement()) {
        const auto n = xml.name();
        if (n == QLatin1String("offset"))
            block.offset = svdNumber(xml.readElementText());
        else if (n == QLatin1String("size"))
            block.size = svdNumber(xml.readElementText());
        else if (n == QLatin1String("usage"))
            block.usage = xml.readElementText();
        else
            xml.skipCurrentElement();
    }
    p.addressBlocks.append(block);
}

void SvdParser::parseRegisters(QXmlStreamReader &xml, SvdPeripheral &p,
                               const SvdDevice &dev)
{
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("register"))
            p.registers.append(parseRegister(xml, dev));
        else
            xml.skipCurrentElement();   // <cluster> not used by shipped SVDs
    }
}

SvdRegister SvdParser::parseRegister(QXmlStreamReader &xml, const SvdDevice &dev)
{
    SvdRegister reg;
    reg.sizeBits   = dev.defaultSizeBits;   // inherit device-level defaults
    reg.resetValue = dev.defaultResetValue;
    reg.resetMask  = dev.defaultResetMask;

    while (xml.readNextStartElement()) {
        const auto n = xml.name();
        if (n == QLatin1String("name"))
            reg.name = xml.readElementText();
        else if (n == QLatin1String("displayName"))
            reg.displayName = xml.readElementText();
        else if (n == QLatin1String("description"))
            reg.description = xml.readElementText();
        else if (n == QLatin1String("addressOffset"))
            reg.addressOffset = svdNumber(xml.readElementText());
        else if (n == QLatin1String("size"))
            reg.sizeBits = static_cast<int>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("access"))
            reg.access = xml.readElementText();
        else if (n == QLatin1String("readAction"))
            reg.readAction = xml.readElementText();
        else if (n == QLatin1String("resetValue"))
            reg.resetValue = static_cast<quint32>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("resetMask"))
            reg.resetMask = static_cast<quint32>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("fields"))
            parseFields(xml, reg);
        else
            xml.skipCurrentElement();
    }
    return reg;
}

void SvdParser::parseFields(QXmlStreamReader &xml, SvdRegister &reg)
{
    while (xml.readNextStartElement()) {
        if (xml.name() == QLatin1String("field"))
            reg.fields.append(parseField(xml));
        else
            xml.skipCurrentElement();
    }
}

SvdField SvdParser::parseField(QXmlStreamReader &xml)
{
    SvdField field;
    // Support both bitOffset/bitWidth and lsb/msb (bitRangeOffsetWidthStyle vs
    // bitRangeLsbMsbStyle). Shipped SVDs use bitOffset/bitWidth.
    int lsb = -1, msb = -1;

    while (xml.readNextStartElement()) {
        const auto n = xml.name();
        if (n == QLatin1String("name"))
            field.name = xml.readElementText();
        else if (n == QLatin1String("description"))
            field.description = xml.readElementText();
        else if (n == QLatin1String("bitOffset"))
            field.bitOffset = static_cast<int>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("bitWidth"))
            field.bitWidth = static_cast<int>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("lsb"))
            lsb = static_cast<int>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("msb"))
            msb = static_cast<int>(svdNumber(xml.readElementText()));
        else if (n == QLatin1String("access"))
            field.access = xml.readElementText();
        else if (n == QLatin1String("readAction"))
            field.readAction = xml.readElementText();
        else if (n == QLatin1String("enumeratedValues"))
            parseEnumeratedValues(xml, field);
        else
            xml.skipCurrentElement();
    }

    if (lsb >= 0 && msb >= lsb) {   // lsb/msb style overrides defaults
        field.bitOffset = lsb;
        field.bitWidth  = msb - lsb + 1;
    }
    return field;
}

void SvdParser::parseEnumeratedValues(QXmlStreamReader &xml, SvdField &field)
{
    while (xml.readNextStartElement()) {
        if (xml.name() != QLatin1String("enumeratedValue")) {
            xml.skipCurrentElement();   // <name>/<usage> of the container
            continue;
        }
        SvdEnumValue ev;
        while (xml.readNextStartElement()) {
            const auto n = xml.name();
            if (n == QLatin1String("name"))
                ev.name = xml.readElementText();
            else if (n == QLatin1String("description"))
                ev.description = xml.readElementText();
            else if (n == QLatin1String("value"))
                ev.value = svdNumber(xml.readElementText());
            else if (n == QLatin1String("isDefault")) {
                ev.isDefault = (xml.readElementText().trimmed() == QLatin1String("true"));
            } else {
                xml.skipCurrentElement();
            }
        }
        field.enums.append(ev);
    }
}

void SvdParser::resolveDerivedFrom(QList<SvdPeripheral> &peripherals,
                                   const QList<QString> &derivedFrom)
{
    QHash<QString, int> indexByName;
    for (int i = 0; i < peripherals.size(); ++i)
        indexByName.insert(peripherals[i].name, i);

    // Track which entries are still unresolved (derivedFrom not yet applied).
    QList<QString> pending = derivedFrom;

    bool progress = true;
    while (progress) {
        progress = false;
        for (int i = 0; i < peripherals.size(); ++i) {
            if (pending[i].isEmpty())
                continue;
            const int base = indexByName.value(pending[i], -1);
            if (base < 0) {          // dangling reference — give up on this one
                pending[i].clear();
                progress = true;
                continue;
            }
            if (!pending[base].isEmpty())
                continue;            // resolve base first (handles derived chains)

            const SvdPeripheral &b = peripherals[base];
            SvdPeripheral &d = peripherals[i];
            // Inherit from base, keep the derived instance's own identity.
            if (d.groupName.isEmpty())      d.groupName = b.groupName;
            if (d.description.isEmpty())    d.description = b.description;
            if (d.addressBlocks.isEmpty())  d.addressBlocks = b.addressBlocks;
            if (d.registers.isEmpty())      d.registers = b.registers;
            pending[i].clear();
            progress = true;
        }
    }
}
