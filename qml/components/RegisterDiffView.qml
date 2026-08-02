import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

// A → B farkı: field-level semantic diff table. Consumes the QVariantMap
// returned by backend.registerDiff() (Backend::diffToVariant) — that data is
// already filtered to changed registers/fields only, so this component is a
// pure renderer, no filtering logic of its own.
//
// Row model mirrors RegisterTree.qml's proven pattern: flatten to a plain JS
// array and assign it to ListView.model in one atomic swap (never
// ListModel.clear()+append(), which caused an intermittent blank-row bug —
// see RegisterTree.qml's rebuild() comment for the full story).
Rectangle {
    id: root

    // The QVariantMap from backend.registerDiff(): { comparable, changedRegisters:[
    //   { peripheral, register, addr, statusA, statusB, rawA, rawB,
    //     changedFields:[ { name, description, bitOffset, bitWidth,
    //                       valueA, valueB, enumNameA, enumNameB } ] } ] }
    property var diff: ({})

    radius: Theme.radiusMd
    color: Theme.surface
    border.color: Theme.border
    clip: true

    readonly property int wField: 150
    readonly property int wValue: 150

    property var _rows: []

    function _delta(f) {
        if (f.enumNameA.length > 0 || f.enumNameB.length > 0) {
            const a = f.enumNameA.length > 0 ? f.enumNameA : String(f.valueA)
            const b = f.enumNameB.length > 0 ? f.enumNameB : String(f.valueB)
            return a + " → " + b
        }
        return f.valueA + " → " + f.valueB
    }

    function rebuild() {
        var rows = []
        var regs = (root.diff && root.diff.changedRegisters) || []
        for (var i = 0; i < regs.length; ++i) {
            var r = regs[i]
            var statusChanged = r.statusA !== r.statusB
            rows.push({
                rowType: "register",
                key: r.peripheral + "/" + r.register,
                peripheral: r.peripheral,
                register: r.register,
                addr: r.addr,
                statusA: r.statusA,
                statusB: r.statusB,
                statusChanged: statusChanged,
                fieldCount: (r.changedFields || []).length
            })
            if (statusChanged) {
                rows.push({
                    rowType: "statusNote",
                    key: r.peripheral + "/" + r.register + "/status",
                    text: "Durum değişti: " + r.statusA + " → " + r.statusB
                })
            }
            var fields = r.changedFields || []
            for (var j = 0; j < fields.length; ++j) {
                var f = fields[j]
                rows.push({
                    rowType: "field",
                    key: r.peripheral + "/" + r.register + "/" + f.name,
                    name: f.name,
                    description: f.description || "",
                    valueA: f.enumNameA.length > 0 ? f.enumNameA : String(f.valueA),
                    valueB: f.enumNameB.length > 0 ? f.enumNameB : String(f.valueB),
                    delta: root._delta(f)
                })
            }
        }
        root._rows = rows   // single atomic assignment, no empty intermediate state
    }

    onDiffChanged: rebuild()
    Component.onCompleted: rebuild()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            visible: root._rows.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            Text {
                anchors.centerIn: parent
                text: "İki snapshot arasında fark yok."
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSm
            }
        }

        Rectangle {
            visible: root._rows.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            color: Theme.surfaceRaised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm
                Text { text: "Field"; Layout.fillWidth: true; color: Theme.primary
                       font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Değer A"; Layout.preferredWidth: root.wValue; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Değer B"; Layout.preferredWidth: root.wValue; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
                Text { text: "Δ"; Layout.preferredWidth: 220; horizontalAlignment: Text.AlignRight
                       color: Theme.primary; font.family: Theme.fontFamily; font.pixelSize: 10
                       font.weight: Font.Bold; font.capitalization: Font.AllUppercase }
            }
        }
        Rectangle { visible: root._rows.length > 0; Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

        ListView {
            id: list
            visible: root._rows.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root._rows
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { width: 8; policy: ScrollBar.AsNeeded }

            // Disabled for the same reason as RegisterTree: this delegate exposes
            // the row via a plain `property var` on the Loader, and pooled-item
            // reuse can leave that binding pointing at a stale row on recycle.
            reuseItems: false

            delegate: Loader {
                width: list.width
                sourceComponent: modelData.rowType === "register" ? regComp
                               : modelData.rowType === "statusNote" ? statusComp : fieldComp
                property var rowData: modelData
            }
        }
    }

    Component {
        id: regComp
        Rectangle {
            width: list.width
            height: 28
            color: Theme.bgElevated
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm
                Text { text: rowData.peripheral + "." + rowData.register; color: Theme.text
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Bold }
                Text { text: rowData.addr; color: Theme.textMuted
                       font.family: Theme.monoFamily; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Text { text: rowData.fieldCount + " field"; color: Theme.textFaint
                       font.family: Theme.fontFamily; font.pixelSize: 10 }
            }
        }
    }

    Component {
        id: statusComp
        Rectangle {
            width: list.width
            height: 22
            color: Theme.alpha(Theme.warning, 0.08)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm + 14
                anchors.rightMargin: Theme.spacingSm
                Text { text: rowData.text; color: Theme.warning
                       font.family: Theme.fontFamily; font.pixelSize: 11; font.weight: Font.DemiBold }
            }
        }
    }

    Component {
        id: fieldComp
        Rectangle {
            width: list.width
            height: 24
            color: Theme.alpha(Theme.cyan, 0.04)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingSm + 14
                anchors.rightMargin: Theme.spacingSm
                spacing: Theme.spacingSm

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text { text: rowData.name; color: Theme.text
                           font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold }
                    Text {
                        visible: rowData.description.length > 0
                        text: rowData.description; color: Theme.textFaint
                        font.family: Theme.fontFamily; font.pixelSize: 10
                        elide: Text.ElideRight; Layout.fillWidth: true
                    }
                }
                Text { text: rowData.valueA; Layout.preferredWidth: root.wValue
                       horizontalAlignment: Text.AlignRight; color: Theme.textMuted
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm }
                Text { text: rowData.valueB; Layout.preferredWidth: root.wValue
                       horizontalAlignment: Text.AlignRight; color: Theme.text
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Bold }
                Text { text: rowData.delta; Layout.preferredWidth: 220
                       horizontalAlignment: Text.AlignRight; color: Theme.cyan
                       font.family: Theme.monoFamily; font.pixelSize: Theme.fontSm; font.weight: Font.Bold
                       elide: Text.ElideLeft }
            }
        }
    }
}
