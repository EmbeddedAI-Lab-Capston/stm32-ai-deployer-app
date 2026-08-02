#include "ValueCodec.h"

#include <QtGlobal>

#include <cmath>
#include <cstring>

int ValueCodec::byteSize(WatchValueType t)
{
    switch (t) {
    case WatchValueType::U8:
    case WatchValueType::I8:
        return 1;
    case WatchValueType::U16:
    case WatchValueType::I16:
        return 2;
    case WatchValueType::U32:
    case WatchValueType::I32:
    case WatchValueType::F32:
        return 4;
    case WatchValueType::U64:
    case WatchValueType::I64:
    case WatchValueType::F64:
        return 8;
    }
    return 0;
}

double ValueCodec::decode(const QByteArray &buf, int offset, WatchValueType t, bool *ok)
{
    const int n = byteSize(t);
    if (offset < 0 || n <= 0 || offset + n > buf.size()) {
        if (ok) *ok = false;
        return 0.0;
    }
    if (ok) *ok = true;

    quint64 raw = 0;
    for (int i = 0; i < n; ++i)
        raw |= quint64(static_cast<unsigned char>(buf.at(offset + i))) << (8 * i);

    switch (t) {
    case WatchValueType::U8:  return double(quint8(raw));
    case WatchValueType::I8:  return double(qint8(raw));
    case WatchValueType::U16: return double(quint16(raw));
    case WatchValueType::I16: return double(qint16(raw));
    case WatchValueType::U32: return double(quint32(raw));
    case WatchValueType::I32: return double(qint32(raw));
    case WatchValueType::U64: return double(raw);
    case WatchValueType::I64: return double(qint64(raw));
    case WatchValueType::F32: {
        const quint32 bits = quint32(raw);
        float f = 0.0f;
        std::memcpy(&f, &bits, sizeof(f));
        return double(f);
    }
    case WatchValueType::F64: {
        double d = 0.0;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }
    }
    return 0.0;
}

QString ValueCodec::format(double raw, const WatchItem &item)
{
    if (item.format == DisplayFormat::Hex || item.format == DisplayFormat::Bin) {
        const int bits = byteSize(item.type) * 8;
        quint64 bitsValue = 0;
        switch (item.type) {
        case WatchValueType::F32: {
            const float f = float(raw);
            quint32 b = 0;
            std::memcpy(&b, &f, sizeof(b));
            bitsValue = b;
            break;
        }
        case WatchValueType::F64: {
            quint64 b = 0;
            std::memcpy(&b, &raw, sizeof(b));
            bitsValue = b;
            break;
        }
        default:
            bitsValue = quint64(qint64(std::llround(raw)));
            break;
        }

        if (item.format == DisplayFormat::Hex)
            return QStringLiteral("0x%1").arg(bitsValue, qMax(1, bits / 4), 16, QLatin1Char('0'));
        return QString::number(bitsValue, 2).rightJustified(qMax(1, bits), QLatin1Char('0'));
    }

    const bool isFloatType = (item.type == WatchValueType::F32 || item.type == WatchValueType::F64);
    const bool isScaled    = !qFuzzyCompare(item.scale, 1.0) || !qFuzzyIsNull(item.offset);
    const double value     = raw * item.scale + item.offset;

    QString numberText = (isFloatType || isScaled)
                              ? QString::number(value, 'f', 3)
                              : QString::number(qint64(std::llround(value)));
    if (!item.unit.isEmpty())
        numberText += QStringLiteral(" ") + item.unit;
    return numberText;
}
