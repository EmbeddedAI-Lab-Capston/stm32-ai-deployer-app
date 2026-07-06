import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Expandable register tree: peripheral -> register -> bit fields.
// Consumes the backend.registerModel tree (a list of peripheral maps). The tree
// is flattened into a single ListModel of typed rows so ListView can virtualise
// it; expand/collapse rebuilds the flat model. Styling is taken straight from
// Theme (matching DataTable's row language).
Rectangle {
    id: root

    // The decoded snapshot tree: [{ name, group, description, clock, registers:[
    //   { name, addr, raw, reset, changed, status, description, fields:[...] } ] }]
    property var model: []
    // Diff mode dims unchanged rows; kept for Faz 5 (ignored for now).
    property bool changedOnly: false

    radius: Theme.radiusMd
    color: Theme.surface
    border.color: Theme.border
    clip: true

    // Column widths shared by header and rows so they line up.
    readonly property int wChevron: 24
    readonly property int wAddr: 138
    readonly property int wValue: 128
    readonly property int wReset: 128
    readonly property int wStatus: 118

    // Expansion state, keyed by unique id; reassigned to trigger rebuild.
    property var _expP: ({})
    property var _expR: ({})

    function _togglePeripheral(key) {
        var m = root._expP; m[key] = !m[key]; root._expP = Object.assign({}, m); rebuild()
    }
    function _toggleRegister(key) {
        var m = root._expR; m[key] = !m[key]; root._expR = Object.assign({}, m); rebuild()
    }

    onModelChanged: rebuild()

    function rebuild() {
        flat.clear()
        var peris = root.model || []
        for (var i = 0; i < peris.length; ++i) {
            var p = peris[i]
            var pKey = p.name
            var pOpen = root._expP[pKey] === true
            flat.append({ rowType: "peripheral", pName: p.name, pGroup: p.group || "",
                          pDesc: p.description || "", pClock: p.clock || "unknown",
                          pCount: (p.registers ? p.registers.length : 0), pOpen: pOpen,
                          key: pKey })
            if (!pOpen) continue

            var regs = p.registers || []
            for (var j = 0; j < regs.length; ++j) {
                var r = regs[j]
                if (root.changedOnly && !r.changed && r.status === "ok") continue
                var rKey = pKey + "/" + r.addr
                var hasFields = r.fields && r.fields.length > 0 && r.status === "ok"
                var rOpen = hasFields && root._expR[rKey] === true
                flat.append({ rowType: "register", rName: r.name, rAddr: r.addr,
                              rRaw: r.raw, rReset: r.reset, rChanged: r.changed === true,
                              rStatus: r.status, rHasFields: hasFields, rOpen: rOpen,
                              rFieldCount: (r.fields ? r.fields.length : 0), key: rKey })
                if (!rOpen) continue

                var fields = r.fields || []
                for (var k = 0; k < fields.length; ++k) {
                    var f = fields[k]
                    var bits = f.bitWidth > 1
                        ? ("[" + (f.bitOffset + f.bitWidth - 1) + ":" + f.bitOffset + "]")
                        : ("[" + f.bitOffset + "]")
                    flat.append({ rowType: "field", fBits: bits, fName: f.name,
                                  fVal: String(f.value), fEnum: f.enumName || "",
                                  fChanged: f.changed === true, fDesc: f.description || "",
                                  key: rKey + "." + f.name })
                }
            }
        }
    }

    function statusText(status, changed) {
        if (status === "ok") return changed ? "≠ reset" : "= reset"
        if (status === "clock-off") return "clock off"
        if (status === "side-effect") return "yan etki"
        if (status === "write-only") return "write-only"
        if (status === "unreadable") return "okunamadı"
        return status
    }
    function statusColor(status, changed) {
        if (status === "ok") return changed ? Theme.cyan : Theme.textMuted
        if (status === "clock-off") return Theme.warning
        if (status === "side-effect") return Theme.purple
        if (status === "unreadable") return Theme.danger
        return Theme.textMuted
    }

    // dynamicRoles is required: peripheral/register/field rows each append a
    // different set of property names, and ListModel otherwise fixes its roles
    // from the first appended row, silently dropping later-introduced keys.
    ListModel { id: flat; dynamicRoles: true }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header row ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: Theme.surfaceRaised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingMd
                function hcol(t) { return t }
                Item { Layout.preferredWidth: root.wChevron }
                Text { text: "Register"; Layout.fillWidth: true; color: Theme.primary
                       font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Adres"; Layout.preferredWidth: root.wAddr; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Değer"; Layout.preferredWidth: root.wValue; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Reset"; Layout.preferredWidth: root.wReset; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Durum"; Layout.preferredWidth: root.wStatus; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // ── Body ────────────────────────────────────────────────────────
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: flat
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 9; policy: ScrollBar.AsNeeded }

            delegate: Loader {
                width: list.width
                sourceComponent: model.rowType === "peripheral" ? periComp
                               : model.rowType === "register" ? regComp : fieldComp
                property var rowData: model
            }
        }
    }

    // ── Peripheral header row ───────────────────────────────────────────
    Component {
        id: periComp
        Rectangle {
            width: list.width
            height: 42
            color: pmouse.containsMouse ? Theme.surfaceHover : Theme.bgElevated
            opacity: rowData.pClock === "off" ? 0.62 : 1.0
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingSm

                Text {
                    Layout.preferredWidth: root.wChevron
                    text: "▶"; color: Theme.textMuted; font.pixelSize: 11
                    rotation: rowData.pOpen ? 90 : 0
                    Behavior on rotation { NumberAnimation { duration: Theme.animFast } }
                }
                Text { text: rowData.pName; color: Theme.text; font.family: Theme.fontFamily
                       font.pixelSize: Theme.fontSm; font.weight: Font.Bold }
                Text { text: rowData.pDesc; color: Theme.textFaint; font.family: Theme.fontFamily
                       font.pixelSize: Theme.fontXs; elide: Text.ElideRight; Layout.fillWidth: true }
                // clock badge
                Rectangle {
                    visible: rowData.pClock !== "unknown"
                    Layout.preferredHeight: 20
                    Layout.preferredWidth: clkRow.implicitWidth + Theme.spacingSm * 2
                    radius: 10
                    color: Theme.alpha(rowData.pClock === "on" ? Theme.success : Theme.textMuted, 0.12)
                    RowLayout {
                        id: clkRow; anchors.centerIn: parent; spacing: 5
                        Rectangle { width: 6; height: 6; radius: 3
                            color: rowData.pClock === "on" ? Theme.success : Theme.textMuted }
                        Text { text: rowData.pClock === "on" ? "on" : "clock off"
                               color: rowData.pClock === "on" ? Theme.success : Theme.textMuted
                               font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold }
                    }
                }
                Text { text: rowData.pCount + " reg"; color: Theme.textMuted; font.family: Theme.fontFamily
                       font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
            }
            MouseArea { id: pmouse; anchors.fill: parent; hoverEnabled: true
                        onClicked: root._togglePeripheral(rowData.key) }
        }
    }

    // ── Register row ────────────────────────────────────────────────────
    Component {
        id: regComp
        Rectangle {
            width: list.width
            height: 40
            property bool isChanged: rowData.rStatus === "ok" && rowData.rChanged
            property bool isSkip: rowData.rStatus !== "ok"
            color: rmouse.containsMouse ? Theme.surfaceHover
                 : isChanged ? Theme.alpha(Theme.cyan, 0.05) : "transparent"
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1
                        color: Theme.alpha(Theme.border, 0.6) }
            // changed accent stripe
            Rectangle { visible: isChanged; anchors.left: parent.left; anchors.top: parent.top
                        anchors.bottom: parent.bottom; width: 3; color: Theme.cyan }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingMd

                Text {
                    Layout.preferredWidth: root.wChevron
                    text: rowData.rHasFields ? "▶" : ""
                    color: Theme.textMuted; font.pixelSize: 10
                    rotation: rowData.rOpen ? 90 : 0
                    Behavior on rotation { NumberAnimation { duration: Theme.animFast } }
                }
                Text { text: rowData.rName; Layout.fillWidth: true
                       color: isSkip ? Theme.textMuted : Theme.text
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                       font.weight: isSkip ? Font.Normal : Font.DemiBold }
                Text { text: rowData.rAddr; Layout.preferredWidth: root.wAddr
                       horizontalAlignment: Text.AlignRight; color: Theme.textMuted
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs }
                Text { text: rowData.rStatus === "ok" ? rowData.rRaw : "—"
                       Layout.preferredWidth: root.wValue; horizontalAlignment: Text.AlignRight
                       color: isChanged ? Theme.cyan : (isSkip ? Theme.textFaint : Theme.text)
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                       font.weight: isChanged ? Font.Bold : Font.Normal }
                Text { text: rowData.rReset; Layout.preferredWidth: root.wReset
                       horizontalAlignment: Text.AlignRight; color: Theme.textFaint
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs }
                // status badge
                Item {
                    Layout.preferredWidth: root.wStatus
                    Layout.fillHeight: true
                    Rectangle {
                        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        height: 20; width: badgeText.implicitWidth + Theme.spacingSm * 2; radius: 10
                        readonly property color c: root.statusColor(rowData.rStatus, rowData.rChanged)
                        color: Theme.alpha(c, 0.14)
                        border.color: rowData.rStatus === "ok" && !rowData.rChanged
                                      ? "transparent" : Theme.alpha(c, 0.34)
                        Text { id: badgeText; anchors.centerIn: parent
                               text: root.statusText(rowData.rStatus, rowData.rChanged)
                               color: parent.c; font.family: Theme.fontFamily
                               font.pixelSize: 10; font.weight: Font.DemiBold }
                    }
                }
            }
            MouseArea { id: rmouse; anchors.fill: parent; hoverEnabled: true
                        enabled: rowData.rHasFields
                        cursorShape: rowData.rHasFields ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: if (rowData.rHasFields) root._toggleRegister(rowData.key) }
        }
    }

    // ── Field row (expanded register) ───────────────────────────────────
    Component {
        id: fieldComp
        Rectangle {
            width: list.width
            height: 30
            color: Theme.alpha(Theme.bg, 0.5)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd + root.wChevron
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingMd
                Text { text: rowData.fBits; Layout.preferredWidth: 78; color: Theme.textFaint
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs }
                Text { text: rowData.fName; Layout.preferredWidth: 150
                       color: rowData.fChanged ? Theme.text : Theme.textMuted
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                       font.weight: Font.DemiBold }
                Text { text: rowData.fVal + (rowData.fEnum.length > 0 ? "  " + rowData.fEnum : "")
                       Layout.preferredWidth: 150
                       color: rowData.fChanged ? Theme.cyan : Theme.textMuted
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontXs
                       font.weight: rowData.fChanged ? Font.Bold : Font.Normal }
                Text { text: rowData.fDesc; Layout.fillWidth: true; color: Theme.textFaint
                       font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; elide: Text.ElideRight }
            }
        }
    }
}
