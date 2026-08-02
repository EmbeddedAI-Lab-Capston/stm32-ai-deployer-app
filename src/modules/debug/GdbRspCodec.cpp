#include "GdbRspCodec.h"

quint8 GdbRspCodec::checksum(const QByteArray &payload)
{
    quint32 sum = 0;
    for (int i = 0; i < payload.size(); ++i)
        sum += static_cast<unsigned char>(payload.at(i));
    return static_cast<quint8>(sum & 0xFF);
}

QByteArray GdbRspCodec::frame(const QByteArray &payload)
{
    QByteArray out;
    out.reserve(payload.size() + 4);
    out += '$';
    out += payload;
    out += '#';
    out += QByteArray::number(checksum(payload), 16).rightJustified(2, '0');
    return out;
}

GdbRspCodec::Extract GdbRspCodec::extract(QByteArray &buffer, QByteArray &payloadOut)
{
    payloadOut.clear();

    if (buffer.isEmpty())
        return Extract::NeedMore;

    const char lead = buffer.at(0);

    if (lead == '+') {
        buffer.remove(0, 1);
        return Extract::Ack;
    }
    if (lead == '-') {
        buffer.remove(0, 1);
        return Extract::Nak;
    }
    if (lead == '$' || lead == '%') {
        const int hashIdx = buffer.indexOf('#', 1);
        if (hashIdx < 0)
            return Extract::NeedMore;               // '#' not seen yet
        if (buffer.size() < hashIdx + 3)
            return Extract::NeedMore;                // checksum hex pair not fully arrived

        payloadOut = buffer.mid(1, hashIdx - 1);
        const bool isNotification = (lead == '%');
        buffer.remove(0, hashIdx + 3);                // consume marker + payload + '#' + 2 hex
        return isNotification ? Extract::Notification : Extract::Packet;
    }

    // Unrecognised leading byte — drop it so the stream can resync instead of
    // spinning forever on it.
    payloadOut = buffer.left(1);
    buffer.remove(0, 1);
    return Extract::Garbage;
}

QByteArray GdbRspCodec::expandRunLength(const QByteArray &payload, bool *ok)
{
    QByteArray out;
    out.reserve(payload.size());
    bool localOk = true;

    for (int i = 0; i < payload.size(); ++i) {
        const char c = payload.at(i);
        if (c == '*') {
            if (out.isEmpty() || i + 1 >= payload.size()) {
                localOk = false;   // no char to repeat, or truncated before count byte
                break;
            }
            const char repeatChar = out.at(out.size() - 1);
            const int  extra = static_cast<int>(static_cast<unsigned char>(payload.at(i + 1))) - 29;
            if (extra < 0) {
                localOk = false;
                break;
            }
            out.append(extra, repeatChar);
            ++i;   // consume the count byte too
            continue;
        }
        out.append(c);
    }

    if (ok) *ok = localOk;
    return localOk ? out : QByteArray();
}

QByteArray GdbRspCodec::unescape(const QByteArray &payload)
{
    QByteArray out;
    out.reserve(payload.size());
    for (int i = 0; i < payload.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(payload.at(i));
        if (c == 0x7d && i + 1 < payload.size()) {
            const unsigned char next = static_cast<unsigned char>(payload.at(++i));
            out.append(static_cast<char>(next ^ 0x20));
        } else {
            out.append(static_cast<char>(c));
        }
    }
    return out;
}

QByteArray GdbRspCodec::hexDecode(const QByteArray &hex, bool *ok)
{
    bool localOk = (hex.size() % 2 == 0);
    QByteArray out;

    if (localOk) {
        out.reserve(hex.size() / 2);
        for (int i = 0; i < hex.size() && localOk; i += 2) {
            bool hiOk = false, loOk = false;
            const int hi = QByteArray(1, hex.at(i)).toInt(&hiOk, 16);
            const int lo = QByteArray(1, hex.at(i + 1)).toInt(&loOk, 16);
            if (!hiOk || !loOk) { localOk = false; break; }
            out.append(static_cast<char>((hi << 4) | lo));
        }
    }

    if (ok) *ok = localOk;
    return localOk ? out : QByteArray();
}

QByteArray GdbRspCodec::memoryReadPacket(quint64 addr, quint32 len)
{
    QByteArray out = "m";
    out += QByteArray::number(static_cast<qulonglong>(addr), 16);
    out += ',';
    out += QByteArray::number(static_cast<uint>(len), 16);
    return out;
}

bool GdbRspCodec::isErrorReply(const QByteArray &payload, QString *codeOut)
{
    if (payload.isEmpty() || payload.at(0) != 'E')
        return false;

    QByteArray rest = payload.mid(1);
    if (rest.startsWith(' '))
        rest = rest.mid(1);

    if (rest.size() != 2)
        return false;

    bool hiOk = false, loOk = false;
    QByteArray(1, rest.at(0)).toInt(&hiOk, 16);
    QByteArray(1, rest.at(1)).toInt(&loOk, 16);
    if (!hiOk || !loOk)
        return false;

    if (codeOut) *codeOut = QString::fromLatin1(rest);
    return true;
}

bool GdbRspCodec::isAllowedOutgoing(const QByteArray &payload)
{
    if (payload.isEmpty())
        return false;

    // Exact matches.
    if (payload == "QStartNoAckMode") return true;
    if (payload == "QNonStop:1")      return true;
    if (payload == "vCont;c")         return true;
    if (payload == "D")               return true;

    // Prefix matches — deliberately narrow, nothing else is whitelisted.
    if (payload.startsWith("qSupported"))                  return true;
    if (payload.startsWith("qXfer:memory-map:read:"))      return true;
    if (payload.startsWith("qXfer:features:read:"))        return true;
    if (payload.startsWith("m"))                            return true; // memory read only ('M' write is a different, rejected, byte)

    return false;
}
