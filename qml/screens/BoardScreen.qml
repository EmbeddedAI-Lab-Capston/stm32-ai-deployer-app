import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    // Live state from backend
    readonly property var _boardRows: (typeof appState !== "undefined" && appState && appState.boardInfoRows.length > 0)
                                      ? appState.boardInfoRows : MockData.boardInfo
    readonly property bool _connected: (typeof appState !== "undefined" && appState) ? appState.connected : false
    readonly property string _boardName: (typeof appState !== "undefined" && appState) ? appState.boardName : "STM32F4"

    property var _portEntries: [{ label: "COM5", portName: "COM5", isStlink: false }]
    property var _customBoards: []
    property int _portIdx: 0
    property int _baudIdx: {
        var baud = (typeof appState !== "undefined" && appState) ? appState.activeBaud : 115200
        var idx = _baudList.indexOf(String(baud))
        return idx >= 0 ? idx : 1
    }

    readonly property var _baudList: ["9600", "115200", "230400", "460800", "921600"]

    // Preset list (always includes the 3 fixed + any custom)
    readonly property var _basePresets: [
        { name: "STM32F4", spec: "1024 KB · 192 KB · 168 MHz", target: "MLP INT8" },
        { name: "STM32H7", spec: "2048 KB · 1024 KB · 480 MHz", target: "1D CNN INT8" },
        { name: "STM32N6", spec: "4096 KB · 4096 KB · 800 MHz", target: "LSTM / KWS" },
        { name: "NUCLEO-N657X0-Q", spec: "4096 KB · 4096 KB · 800 MHz", target: "Cortex-M55 / NPU" }
    ]

    function boardPresets() {
        var out = root._basePresets.slice()
        for (var i = 0; i < root._customBoards.length; ++i) {
            var b = root._customBoards[i]
            out.push({
                name: b.name,
                spec: b.flashKb + " KB · " + b.ramKb + " KB · " + b.clockMhz + " MHz",
                target: "Özel"
            })
        }
        return out
    }

    function activePresetIndex() {
        var bn = root._boardName
        var presets = root.boardPresets()
        for (var i = 0; i < presets.length; i++)
            if (bn === presets[i].name || bn.includes(presets[i].name) || presets[i].name.includes(bn.split(/\W+/)[0])) return i
        return -1
    }

    function refreshPorts() {
        if (typeof backend !== "undefined" && backend) {
            var p = backend.availablePortEntries()
            _portEntries = (p && p.length > 0) ? p : [{ label: "(port bulunamadi)", portName: "", isStlink: false }]
            if (_portIdx >= _portEntries.length) _portIdx = 0
        }
    }
    function portLabels() {
        var out = []
        for (var i = 0; i < root._portEntries.length; ++i)
            out.push(root._portEntries[i].label || root._portEntries[i].portName || "")
        return out
    }
    function selectedPortName() {
        var entry = root._portEntries[root._portIdx]
        return entry ? (entry.portName || entry.label || "") : ""
    }
    function rowLabel(row) {
        if (!row) return "--"
        if (row[0] !== undefined) return row[0]
        return row.label || "--"
    }
    function rowValue(row) {
        if (!row) return "--"
        if (row[1] !== undefined) return row[1]
        return row.value || "--"
    }
    function boardStatusText() {
        if (typeof backend !== "undefined" && backend && backend.probeBusy) return "Taraniyor"
        if (typeof backend !== "undefined" && backend && backend.probeStatus.length > 0) return backend.probeStatus
        if (root._boardRows.length > 0) {
            for (var i = 0; i < root._boardRows.length; ++i) {
                var row = root._boardRows[i]
                var label = root.rowLabel(row)
                if ((label || "").toLowerCase().indexOf("st-link") >= 0) return "ST-Link'ten algilandi"
            }
        }
        if (root._boardName.indexOf("N6") >= 0) return "Experimental template"
        return root._boardName.length > 0 ? "Manuel secim" : "Kart bilgisi bekleniyor"
    }
    function refreshCustomBoards() {
        if (typeof backend !== "undefined" && backend)
            _customBoards = backend.customBoards()
    }
    Component.onCompleted: {
        refreshPorts()
        refreshCustomBoards()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingLg

            Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingMd }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                spacing: Theme.spacingLg

                // ── Page header ───────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    SectionHeader {
                        title: "Kart Seçimi ve Bilgileri"
                        subtitle: "Aktif kartın özellikleri ve bağlantı yönetimi"
                        Layout.fillWidth: true
                    }
                    StatusPill {
                        text: root._connected ? "Bağlı" : "Bağlı değil"
                        status: root._connected ? "connected" : "offline"
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width > 1080 ? 2 : 1
                    columnSpacing: Theme.spacingMd; rowSpacing: Theme.spacingMd

                    // ── Board info panel ──────────────────────────────────
                    Card {
                        title: "Aktif Kart Bilgileri"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 460

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingXs

                            Repeater {
                                model: root._boardRows
                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: root.rowLabel(modelData)
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                        Layout.preferredWidth: 110
                                    }
                                    Text {
                                        text: root.rowValue(modelData)
                                        color: Theme.text
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight; Layout.fillWidth: true
                                    }
                                }
                            }

                            Item { Layout.fillHeight: true }

                            StatusPill {
                                text: root.boardStatusText()
                                status: (typeof backend !== "undefined" && backend && backend.probeBusy) ? "warning" : "ok"
                            }
                        }
                    }

                    // ── Selection + connection ────────────────────────────
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.spacingMd

                        // Preset cards
                        Card {
                            title: "Kart Seçimi"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 320

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: Theme.spacingSm

                                Repeater {
                                    model: root.boardPresets()
                                    delegate: Rectangle {
                                        Layout.fillWidth: true; Layout.preferredHeight: 62
                                        radius: Theme.radiusMd
                                        property bool isActive: index === root.activePresetIndex()
                                        color: isActive ? Theme.primarySoft : Theme.surfaceRaised
                                        border.color: isActive ? Theme.primary : Theme.border
                                        border.width: isActive ? 2 : 1

                                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                                        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: Theme.spacingMd
                                            spacing: Theme.spacingMd

                                            ColumnLayout {
                                                spacing: 2; Layout.fillWidth: true
                                                Text {
                                                    text: modelData.name; color: Theme.text
                                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontMd
                                                    font.weight: Font.DemiBold
                                                }
                                                Text {
                                                    text: modelData.spec; color: Theme.textMuted
                                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs
                                                }
                                            }

                                            StatusPill { text: modelData.target; status: "info" }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                if (typeof backend !== "undefined" && backend)
                                                    backend.selectBoard(modelData.name)
                                            }
                                            cursorShape: Qt.PointingHandCursor
                                        }
                                    }
                                }

                                // Custom board button
                                AppButton {
                                    text: "Özel Kart Ekle"; iconText: "+"; variant: "secondary"
                                    Layout.fillWidth: true
                                    onClicked: customBoardDialog.open()
                                }

                                Item { Layout.fillHeight: true }
                            }
                        }

                        // Connection card
                        Card {
                            title: "Bağlantı"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: Theme.spacingMd

                                RowLayout {
                                    Layout.fillWidth: true; spacing: Theme.spacingMd

                                    ComboField {
                                        label: "Port"; Layout.fillWidth: true
                                        options: root.portLabels()
                                        currentIndex: root._portIdx
                                        onActivated: (idx) => root._portIdx = idx
                                    }

                                    ComboField {
                                        label: "Baud"; Layout.fillWidth: true
                                        options: root._baudList
                                        currentIndex: root._baudIdx
                                        onActivated: (idx) => root._baudIdx = idx
                                    }
                                }

                                Item { Layout.fillHeight: true }

                                RowLayout {
                                    Layout.fillWidth: true; spacing: Theme.spacingSm

                                    AppButton {
                                        text: root._connected ? "Bağlantıyı Kes" : "Bağlan"
                                        iconText: "⏻"
                                        variant: root._connected ? "danger" : "primary"
                                        onClicked: {
                                            if (typeof backend === "undefined" || !backend) return
                                            if (root._connected) {
                                                backend.disconnectSerial()
                                            } else {
                                                var port = root.selectedPortName()
                                                var baud = parseInt(root._baudList[root._baudIdx]) || 115200
                                                if (port.length > 0)
                                                    backend.connectSerial(port, baud)
                                            }
                                        }
                                    }

                                    AppButton {
                                        text: "Yenile"; iconText: "⟳"; variant: "secondary"
                                        onClicked: root.refreshPorts()
                                    }

                                    AppButton {
                                        text: (typeof backend !== "undefined" && backend && backend.probeBusy) ? "Taraniyor" : "ST-Link Tara"
                                        iconText: "?"
                                        variant: "secondary"
                                        enabled: !(typeof backend !== "undefined" && backend && backend.probeBusy)
                                        onClicked: if (typeof backend !== "undefined" && backend) backend.probeStLinkBoardForPort(root.selectedPortName())
                                    }

                                    Item { Layout.fillWidth: true }

                                    StatusPill {
                                        text: root._connected
                                              ? ((root.selectedPortName() || "COM") + " @ " + root._baudList[root._baudIdx])
                                              : "Bağlı değil"
                                        status: root._connected ? "connected" : "offline"
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: Theme.spacingLg }
            }
        }
    }

    // ── Custom board dialog ────────────────────────────────────────────────
    Popup {
        id: customBoardDialog
        modal: true; anchors.centerIn: Overlay.overlay; width: 360; padding: Theme.spacingLg
        background: Rectangle { color: Theme.surface; radius: Theme.radiusLg; border.color: Theme.border }

        contentItem: ColumnLayout {
            spacing: Theme.spacingMd

            Text { text: "Özel Kart Ekle"; color: Theme.text; font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold }

            ColumnLayout { spacing: Theme.spacingXs; Layout.fillWidth: true
                Text { text: "Kart Adı"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                TextField { id: customNameField; Layout.fillWidth: true; placeholderText: "ör. MyBoard-F401"
                    color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: customNameField.activeFocus ? Theme.primary : Theme.border }
                    leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: Theme.spacingMd

                ColumnLayout { spacing: Theme.spacingXs; Layout.fillWidth: true
                    Text { text: "Flash (KB)"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                    TextField { id: flashField; Layout.fillWidth: true; text: "512"; inputMethodHints: Qt.ImhDigitsOnly
                        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: flashField.activeFocus ? Theme.primary : Theme.border }
                        leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm }
                }
                ColumnLayout { spacing: Theme.spacingXs; Layout.fillWidth: true
                    Text { text: "RAM (KB)"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                    TextField { id: ramField; Layout.fillWidth: true; text: "128"; inputMethodHints: Qt.ImhDigitsOnly
                        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: ramField.activeFocus ? Theme.primary : Theme.border }
                        leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm }
                }
                ColumnLayout { spacing: Theme.spacingXs; Layout.fillWidth: true
                    Text { text: "MHz"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                    TextField { id: mhzField; Layout.fillWidth: true; text: "120"; inputMethodHints: Qt.ImhDigitsOnly
                        color: Theme.text; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSm
                        background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: mhzField.activeFocus ? Theme.primary : Theme.border }
                        leftPadding: Theme.spacingSm; rightPadding: Theme.spacingSm }
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: Theme.spacingSm
                AppButton { text: "İptal"; variant: "ghost"; onClicked: customBoardDialog.close() }
                Item { Layout.fillWidth: true }
                AppButton {
                    text: "Ekle ve Seç"
                    enabled: customNameField.text.trim().length > 0
                    onClicked: {
                        if (typeof backend === "undefined" || !backend) { customBoardDialog.close(); return }
                        backend.addCustomBoard(customNameField.text.trim(),
                                               parseInt(flashField.text) || 0,
                                               parseInt(ramField.text) || 0,
                                               parseInt(mhzField.text) || 0)
                        root.refreshCustomBoards()
                        customBoardDialog.close()
                    }
                }
            }
        }
    }
}
