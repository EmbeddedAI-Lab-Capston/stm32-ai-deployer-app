import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Bottom status strip for the Variable Watcher screen: link state, rate,
// core liveness, ELF match — all in one glance (plan
// docs/variable_watcher_plan.md Bolum 7.7).
Rectangle {
    id: root

    readonly property bool _hasBackend: (typeof backend !== "undefined" && backend)
    readonly property string _linkState: _hasBackend ? backend.watchLinkState : "closed"
    readonly property var    _rate: _hasBackend ? backend.watchRateInfo : ({})
    readonly property string _elfMatch: _hasBackend ? backend.watchElfMatch : "unknown"
    readonly property var    _elfDetail: _hasBackend ? backend.watchElfMatchDetail : ({})

    implicitHeight: 44
    color: Theme.bgElevated
    radius: Theme.radiusMd
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingLg

        // ── Link state ────────────────────────────────────────────────────
        StatusPill {
            text: root._linkState === "open" ? "Bağlı"
                : root._linkState === "failed" ? "Başarısız"
                : root._linkState === "closed" ? "Kapalı"
                : root._linkState === "starting" ? "Başlatılıyor…"
                : root._linkState === "connecting" ? "Bağlanıyor…"
                : root._linkState === "handshaking" ? "El sıkışıyor…"
                : root._linkState
            status: root._linkState === "open" ? "ready"
                : root._linkState === "failed" ? "error"
                : (root._linkState === "closed" ? "" : "warning")
        }

        // ── Rate ──────────────────────────────────────────────────────────
        RowLayout {
            spacing: Theme.spacingXs
            visible: root._hasBackend && backend.watchRunning
            Text {
                text: "Hedef " + (root._rate.targetHz === 0 ? "maks" : root._rate.targetHz + " Hz")
                      + " / Gerçek " + (root._rate.actualHz !== undefined ? root._rate.actualHz.toFixed(1) : "0.0") + " Hz"
                color: {
                    if (root._rate.targetHz > 0 && root._rate.actualHz !== undefined
                        && root._rate.actualHz < root._rate.targetHz * 0.8)
                        return Theme.warning
                    return Theme.text
                }
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold
            }
        }

        Text {
            visible: root._hasBackend && backend.watchRunning
            text: "RTT " + (root._rate.rttMs !== undefined ? root._rate.rttMs.toFixed(2) : "0") + " ms · "
                  + (root._rate.blocks !== undefined ? root._rate.blocks : 0) + " blok/örnek · "
                  + "kaçırılan " + (root._rate.missed !== undefined ? root._rate.missed : 0)
            color: Theme.textMuted
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
        }

        // ── Skew (tooltip carries the exact meaning per plan Bolum 4.2) ────
        Text {
            visible: root._hasBackend && backend.watchRunning
            text: "kayma " + (root._rate.skewUs !== undefined ? root._rate.skewUs.toFixed(0) : "0") + " µs"
            color: Theme.textFaint
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
            ToolTip.visible: skewHover.hovered
            ToolTip.delay: 200
            ToolTip.text: "Değerler t ile t + skew arasında okundu."
            HoverHandler { id: skewHover }
        }

        // ── Core status ───────────────────────────────────────────────────
        StatusPill {
            visible: root._hasBackend && backend.watchLinkOpen
            text: root._rate.coreRunning === false ? "DURDU" : "koşuyor"
            status: root._rate.coreRunning === false ? "error" : "ready"
        }

        Item { Layout.fillWidth: true }

        // ── ELF match ─────────────────────────────────────────────────────
        RowLayout {
            spacing: Theme.spacingSm
            StatusPill {
                text: root._elfMatch === "match" ? "ELF eşleşiyor"
                    : root._elfMatch === "mismatch" ? "ELF eşleşmiyor olabilir"
                    : "ELF bilinmiyor"
                status: root._elfMatch === "match" ? "ready"
                    : root._elfMatch === "mismatch" ? "warning" : ""
                ToolTip.visible: elfHover.hovered && root._elfDetail.detail
                ToolTip.delay: 200
                ToolTip.text: root._elfDetail.detail || ""
                HoverHandler { id: elfHover }
            }
            AppButton {
                visible: root._elfMatch === "mismatch"
                text: "Yine de devam et"
                variant: "danger"
                implicitHeight: 28
                onClicked: if (root._hasBackend) backend.acknowledgeElfMismatch()
            }
        }
    }
}
