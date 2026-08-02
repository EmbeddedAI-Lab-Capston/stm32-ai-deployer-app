#pragma once

#include <QByteArray>
#include <QString>

// ── GdbRspCodec ──────────────────────────────────────────────────────────
// Pure (not a QObject) GDB Remote Serial Protocol codec: framing, checksum,
// run-length decoding, hex, and the outgoing-packet whitelist. No socket, no
// state — every method is a free function grouped in a class so it can be
// unit tested without QCoreApplication/QThread (docs/variable_watcher_plan.md
// Bolum 4.3, 13).
class GdbRspCodec
{
public:
    enum class Extract { NeedMore, Packet, Notification, Ack, Nak, Garbage };

    static quint8     checksum(const QByteArray &payload);
    static QByteArray frame(const QByteArray &payload);      // "$" + payload + "#" + 2 hex

    // Consumes one unit from `buffer` (removes the consumed bytes on Ack, Nak,
    // Packet, Notification, and Garbage; leaves `buffer` untouched on
    // NeedMore so the caller can append more bytes and retry). '%' non-stop
    // notifications are reported separately so the request/reply matcher
    // never confuses them with replies. `payloadOut` receives the raw body
    // between the framing markers — NOT yet run-length expanded or unescaped;
    // callers apply expandRunLength()/unescape() themselves in the order
    // appropriate to what the payload represents.
    static Extract    extract(QByteArray &buffer, QByteArray &payloadOut);

    // RSP run-length: "<c>*<n>" => c repeated (ASCII(n) - 29) extra times.
    // "0* " (n=' '=32) => 32-29 = 3 extra => "0000". Repeat chars '#' and '$'
    // never occur. Returns false in *ok on malformed input (e.g. '*' with no
    // preceding character, or truncated at the count byte).
    static QByteArray expandRunLength(const QByteArray &payload, bool *ok);
    static QByteArray unescape(const QByteArray &payload);   // 0x7d escape

    static QByteArray hexDecode(const QByteArray &hex, bool *ok);
    static QByteArray memoryReadPacket(quint64 addr, quint32 len); // "m<hex>,<hex>"
    static bool       isErrorReply(const QByteArray &payload, QString *codeOut); // "E01" | "E 01"

    // ── Observer guarantee ───────────────────────────────────────────────
    // The ONLY packets this application is ever allowed to transmit. Anything
    // that could halt, reset, write, or breakpoint the target is rejected here,
    // not by convention. See CLAUDE.md "Degisken Izleyici" decision record.
    static bool isAllowedOutgoing(const QByteArray &payload);
};
