pragma Singleton

import QtQuick

QtObject {
    id: theme

    // ── Surfaces ──────────────────────────────────────────────────────────
    readonly property color bg:            "#0D1117"
    readonly property color bgElevated:    "#11161F"
    readonly property color surface:       "#161B22"
    readonly property color surfaceRaised: "#1C2330"
    readonly property color surfaceHover:  "#222B3A"
    readonly property color border:        "#2A313C"
    readonly property color borderStrong:  "#3A4452"

    // ── Text ──────────────────────────────────────────────────────────────
    readonly property color text:        "#E6EDF3"
    readonly property color textMuted:   "#8B97A7"
    readonly property color textFaint:   "#5B6573"

    // ── Accents ───────────────────────────────────────────────────────────
    readonly property color primary:     "#4F8BFF"
    readonly property color primaryHover: "#6EA0FF"
    readonly property color primaryDim:   "#2C4877"
    readonly property color primarySoft:  "#16294A"
    readonly property color cyan:        "#3FD0C9"
    readonly property color success:     "#3FB950"
    readonly property color warning:     "#D8A23A"
    readonly property color danger:      "#F0616D"
    readonly property color purple:      "#A371F7"

    // ── Radii ─────────────────────────────────────────────────────────────
    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 18

    // ── Spacing ───────────────────────────────────────────────────────────
    readonly property int spacingXs: 6
    readonly property int spacingSm: 10
    readonly property int spacingMd: 16
    readonly property int spacingLg: 24
    readonly property int spacingXl: 32

    // ── Typography ────────────────────────────────────────────────────────
    readonly property string fontFamily: "Segoe UI"
    readonly property string monoFamily: "Cascadia Code"
    readonly property int fontXs: 11
    readonly property int fontSm: 13
    readonly property int fontMd: 15
    readonly property int fontLg: 19
    readonly property int fontXl: 26
    readonly property int fontXxl: 34

    // ── Motion ────────────────────────────────────────────────────────────
    readonly property int animFast:   120
    readonly property int animNormal: 200
    readonly property int animSlow:   320

    // ── Layout constants ──────────────────────────────────────────────────
    readonly property int titleBarHeight: 44
    readonly property int tabBarHeight:   52

    // ── Helpers ───────────────────────────────────────────────────────────
    function alpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }

    function statusColor(status) {
        switch (status) {
        case "ok":
        case "ready":
        case "connected":
        case "success":
            return success
        case "warning":
        case "experimental":
        case "pending":
            return warning
        case "error":
        case "offline":
        case "failed":
            return danger
        case "info":
        case "running":
            return primary
        default:
            return textMuted
        }
    }
}
