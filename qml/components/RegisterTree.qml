import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// Expandable register tree: peripheral -> register -> bit fields.
// Consumes the backend.registerModel tree (a list of peripheral maps). The tree
// is flattened into a single ListModel of typed rows so ListView can virtualise
// it; expand/collapse rebuilds the flat model. Density and status language
// follow real SFR/peripheral-register debug tools (STM32CubeIDE SFR view,
// Keil System Viewer): tight rows, monospace-first, color carries status
// instead of pill badges — legibility without eating vertical space.
Rectangle {
    id: root

    // The decoded snapshot tree: [{ name, group, clock, registers:[
    //   { name, addr, raw, reset, changed, status, fields:[...] } ] }]
    property var model: []
    // "Sadece değişenleri göster" — hides registers unchanged from reset.
    property bool changedOnly: false
    // Filters by register/field name across the whole tree (not peripheral
    // name — that's the left panel's job). Matching peripherals force-open.
    property string filterText: ""

    radius: Theme.radiusMd
    color: Theme.surface
    border.color: Theme.border
    clip: true

    // Column widths shared by header and rows so they line up.
    readonly property int wChevron: 20
    readonly property int wAddr: 112
    readonly property int wValue: 108
    readonly property int wReset: 108
    readonly property int wStatus: 82

    // Expansion state, keyed by unique id; reassigned to trigger rebuild.
    property var _expP: ({})
    property var _expR: ({})

    function _togglePeripheral(key) {
        var m = root._expP; m[key] = !m[key]; root._expP = Object.assign({}, m); rebuild()
    }
    function _toggleRegister(key) {
        var m = root._expR; m[key] = !m[key]; root._expR = Object.assign({}, m); rebuild()
    }

    // Invisible TextEdit used purely as a clipboard sink (QML has no direct
    // clipboard API); select-all + copy is the standard workaround.
    TextEdit { id: clipSink; visible: false; text: "" }
    function copyToClipboard(text) {
        clipSink.text = text
        clipSink.selectAll()
        clipSink.copy()
    }

    onModelChanged: rebuild()
    onChangedOnlyChanged: rebuild()
    onFilterTextChanged: rebuild()

    function _matches(text, needle) {
        return needle.length === 0 || String(text).toLowerCase().indexOf(needle) !== -1
    }

    // Flattened rows, assigned to the ListView's model in one atomic swap.
    // NOTE: this used to be a ListModel mutated via clear()+append() on every
    // rebuild. clear() briefly makes every currently-visible delegate see a
    // null model row; a nested binding like `rowData.pClock` throws mid-clear
    // and QML does not always recover that binding once append() repopulates
    // the model, leaving the row permanently blank (confirmed via
    // app_trace.log: "Cannot read property 'pClock' of null" fired exactly
    // once per rebuild, right when the intermittent blank-row bug occurred).
    // Building a plain array and assigning it whole avoids the empty
    // intermediate state entirely.
    property var _rows: []
    function rebuild() {
        var rows = []
        var peris = root.model || []
        var needle = (root.filterText || "").trim().toLowerCase()
        var searching = needle.length > 0
        var filtering = searching || root.changedOnly   // either forces full expansion

        for (var i = 0; i < peris.length; ++i) {
            var p = peris[i]
            var regs = p.registers || []

            // Decide which registers survive changedOnly + search, up front,
            // so an all-filtered-out peripheral can be skipped entirely
            // instead of showing an empty header (no dead-end groups).
            var visibleRegs = []
            for (var j = 0; j < regs.length; ++j) {
                var r = regs[j]
                if (root.changedOnly && !(r.status === "ok" && r.changed)) continue
                if (searching) {
                    var regMatch = root._matches(r.name, needle)
                    var fieldMatch = false
                    if (!regMatch) {
                        var fs = r.fields || []
                        for (var k = 0; k < fs.length; ++k)
                            if (root._matches(fs[k].name, needle)) { fieldMatch = true; break }
                    }
                    if (!regMatch && !fieldMatch) continue
                }
                visibleRegs.push(r)
            }
            if (filtering && visibleRegs.length === 0) continue

            var pKey = p.name
            var pOpen = filtering ? true : (root._expP[pKey] === true)
            rows.push({ rowType: "peripheral", pName: p.name, pClock: p.clock || "unknown",
                        pCount: regs.length, pShown: visibleRegs.length, pOpen: pOpen,
                        key: pKey })
            if (!pOpen) continue

            for (var j2 = 0; j2 < visibleRegs.length; ++j2) {
                var r2 = visibleRegs[j2]
                var rKey = pKey + "/" + r2.addr
                var hasFields = r2.fields && r2.fields.length > 0 && r2.status === "ok"
                var rOpen = hasFields && (filtering ? true : root._expR[rKey] === true)
                rows.push({ rowType: "register", rName: r2.name, rAddr: r2.addr,
                            rRaw: r2.raw, rReset: r2.reset, rChanged: r2.changed === true,
                            rStatus: r2.status, rHasFields: hasFields, rOpen: rOpen,
                            key: rKey })
                if (!rOpen) continue

                var fields = r2.fields || []
                for (var k2 = 0; k2 < fields.length; ++k2) {
                    var f = fields[k2]
                    var bits = f.bitWidth > 1
                        ? ("[" + (f.bitOffset + f.bitWidth - 1) + ":" + f.bitOffset + "]")
                        : ("[" + f.bitOffset + "]")
                    rows.push({ rowType: "field", fBits: bits, fName: f.name,
                                fVal: String(f.value), fEnum: f.enumName || "",
                                fChanged: f.changed === true, fDesc: f.description || "",
                                key: rKey + "." + f.name })
                }
            }
        }
        root._rows = rows   // single atomic assignment — no empty intermediate state
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
        if (status === "ok") return changed ? Theme.cyan : Theme.textFaint
        if (status === "clock-off") return Theme.warning
        if (status === "side-effect") return Theme.purple
        if (status === "unreadable") return Theme.danger
        return Theme.textMuted
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header row ──────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: Theme.surfaceRaised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm
                Item { Layout.preferredWidth: root.wChevron }
                Text { text: "Register"; Layout.fillWidth: true; color: Theme.primary
                       font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Adres"; Layout.preferredWidth: root.wAddr; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Değer"; Layout.preferredWidth: root.wValue; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Reset"; Layout.preferredWidth: root.wReset; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Durum"; Layout.preferredWidth: root.wStatus; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        // ── Empty state (search/filter produced nothing) ─────────────────
        Text {
            visible: root._rows.length === 0 && (root.model || []).length > 0
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingLg
            horizontalAlignment: Text.AlignHCenter
            text: root.filterText.length > 0 ? "Eşleşen register/field yok."
                : "Seçili peripheral'larda değişen register yok."
            color: Theme.textFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
        }

        // ── Body ────────────────────────────────────────────────────────
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root._rows
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 8; policy: ScrollBar.AsNeeded }

            // Delegate item reuse (Qt 6 ListView's default) requires the
            // delegate to use `required property` for every model role so
            // pooled items get refreshed correctly on reuse. This delegate
            // instead exposes the whole row via a plain `property var rowData:
            // model` on a wrapper Loader — with reuse enabled that binding can
            // keep a stale "model" reference when a pooled item is recycled
            // for a different row, rendering blank name/address/value cells
            // intermittently. Disabling reuse forces a fresh delegate (and a
            // fresh rowData binding) for every row; tree sizes here are small
            // enough that the performance cost is not noticeable.
            reuseItems: false

            // modelData (the raw array element), not the "model" role-object
            // context — for a plain-array-backed view, model.xxx role access
            // has shown transient nulls during whole-array reassignment;
            // modelData is the array element directly and doesn't depend on
            // that role-flattening machinery.
            delegate: Loader {
                width: list.width
                sourceComponent: modelData.rowType === "peripheral" ? periComp
                               : modelData.rowType === "register" ? regComp : fieldComp
                property var rowData: modelData
            }
        }
    }

    // ── Peripheral header row ───────────────────────────────────────────
    Component {
        id: periComp
        Rectangle {
            width: list.width
            height: 30
            color: pmouse.containsMouse ? Theme.surfaceHover : Theme.bgElevated
            opacity: rowData.pClock === "off" ? 0.6 : 1.0
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm

                Text {
                    Layout.preferredWidth: root.wChevron
                    text: "▶"; color: Theme.textMuted; font.pixelSize: 10
                    rotation: rowData.pOpen ? 90 : 0
                    Behavior on rotation { NumberAnimation { duration: Theme.animFast } }
                }
                Text { text: rowData.pName; color: Theme.text; font.family: Theme.monoFamily
                       font.pixelSize: Theme.fontSm; font.weight: Font.Bold }
                Item { Layout.fillWidth: true }
                Text {
                    visible: rowData.pClock === "on" || rowData.pClock === "off"
                    text: rowData.pClock === "on" ? "clock on" : "clock off"
                    color: rowData.pClock === "on" ? Theme.success : Theme.textMuted
                    font.family: Theme.fontFamily; font.pixelSize: 10; font.weight: Font.DemiBold
                }
                Text {
                    text: rowData.pShown !== undefined && rowData.pShown !== rowData.pCount
                          ? (rowData.pShown + "/" + rowData.pCount)
                          : String(rowData.pCount)
                    color: Theme.textFaint; font.family: Theme.monoFamily
                    font.pixelSize: 10; Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignRight
                }
            }
            MouseArea { id: pmouse; anchors.fill: parent; hoverEnabled: true
                        onClicked: root._togglePeripheral(rowData.key) }
        }
    }

    // ── Register row ────────────────────────────────────────────────────
    Component {
        id: regComp
        Rectangle {
            id: regRow
            width: list.width
            height: 26
            property bool isChanged: rowData.rStatus === "ok" && rowData.rChanged
            property bool isSkip: rowData.rStatus !== "ok"
            color: rmouse.containsMouse ? Theme.surfaceHover
                 : isChanged ? Theme.alpha(Theme.cyan, 0.05) : "transparent"
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1
                        color: Theme.alpha(Theme.border, 0.6) }
            // changed accent stripe
            Rectangle { visible: isChanged; anchors.left: parent.left; anchors.top: parent.top
                        anchors.bottom: parent.bottom; width: 2; color: Theme.cyan }

            HoverHandler { id: rowHover }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm

                Text {
                    Layout.preferredWidth: root.wChevron
                    text: rowData.rHasFields ? "▶" : ""
                    color: Theme.textMuted; font.pixelSize: 9
                    rotation: rowData.rOpen ? 90 : 0
                    Behavior on rotation { NumberAnimation { duration: Theme.animFast } }
                }
                Text { text: rowData.rName; Layout.fillWidth: true
                       color: isSkip ? Theme.textMuted : Theme.text
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                       font.weight: isSkip ? Font.Normal : Font.DemiBold }

                RowLayout {
                    Layout.preferredWidth: root.wAddr
                    spacing: 3
                    Item { Layout.fillWidth: true }
                    Text { text: rowData.rAddr; color: Theme.textMuted
                           font.family: Theme.monoFamily; font.pixelSize: 11 }
                    Text { text: "⧉"; color: Theme.textFaint; font.pixelSize: 11
                           visible: rowHover.hovered
                           MouseArea { anchors.fill: parent; anchors.margins: -4
                                       cursorShape: Qt.PointingHandCursor
                                       onClicked: root.copyToClipboard(rowData.rAddr) } }
                }
                RowLayout {
                    Layout.preferredWidth: root.wValue
                    spacing: 3
                    Item { Layout.fillWidth: true }
                    Text { text: rowData.rStatus === "ok" ? rowData.rRaw : "—"
                           color: isChanged ? Theme.cyan : (isSkip ? Theme.textFaint : Theme.text)
                           font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm
                           font.weight: isChanged ? Font.Bold : Font.Normal }
                    Text { text: "⧉"; color: Theme.textFaint; font.pixelSize: 11
                           visible: rowHover.hovered && rowData.rStatus === "ok"
                           MouseArea { anchors.fill: parent; anchors.margins: -4
                                       cursorShape: Qt.PointingHandCursor
                                       onClicked: root.copyToClipboard(rowData.rRaw) } }
                }
                Text { text: rowData.rReset; Layout.preferredWidth: root.wReset
                       horizontalAlignment: Text.AlignRight; color: Theme.textFaint
                       font.family: Theme.monoFamily; font.pixelSize: 11 }
                Text { text: root.statusText(rowData.rStatus, rowData.rChanged)
                       Layout.preferredWidth: root.wStatus; horizontalAlignment: Text.AlignRight
                       color: root.statusColor(rowData.rStatus, rowData.rChanged)
                       font.family: Theme.fontFamily; font.pixelSize: 11
                       font.weight: isChanged ? Font.Bold : Font.Normal
                       visible: rowData.rStatus !== "ok" || rowData.rChanged }
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
            height: 22
            color: Theme.alpha(Theme.bg, 0.5)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm + root.wChevron
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm
                Text { text: rowData.fBits; Layout.preferredWidth: 64; color: Theme.textFaint
                       font.family: Theme.monoFamily; font.pixelSize: 11 }
                Text { text: rowData.fName; Layout.preferredWidth: 130
                       color: rowData.fChanged ? Theme.text : Theme.textMuted
                       font.family: Theme.monoFamily; font.pixelSize: 11
                       font.weight: Font.DemiBold }
                Text { text: rowData.fVal + (rowData.fEnum.length > 0 ? "  " + rowData.fEnum : "")
                       Layout.preferredWidth: 140
                       color: rowData.fChanged ? Theme.cyan : Theme.textMuted
                       font.family: Theme.monoFamily; font.pixelSize: 11
                       font.weight: rowData.fChanged ? Font.Bold : Font.Normal }
                // Item wrapper + anchors.fill (rather than elide directly on a
                // Layout.fillWidth Text) — avoids a width/implicitWidth binding
                // loop that otherwise lets long descriptions wrap and bleed
                // into the next row despite elide being set.
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        text: rowData.fDesc; color: Theme.textFaint
                        font.family: Theme.fontFamily; font.pixelSize: 11
                        elide: Text.ElideRight; wrapMode: Text.NoWrap
                    }
                }
            }
        }
    }
}
