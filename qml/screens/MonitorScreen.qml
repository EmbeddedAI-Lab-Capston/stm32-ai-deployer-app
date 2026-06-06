import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import STM32AiDeployer

Item {
    id: root

    readonly property var _lines: (typeof backend !== "undefined" && backend) ? backend.monitorLines : []
    readonly property bool _connected: (typeof appState !== "undefined" && appState) ? appState.connected : false
    readonly property bool _simRunning: (typeof backend !== "undefined" && backend) ? backend.simRunning : false
    readonly property string _connText: _connected
        ? ((typeof appState !== "undefined" && appState) ? (appState.activePort + " @ " + appState.activeBaud) : "Bagli")
        : "Bagli degil"
    readonly property string _lastLabel: (typeof appState !== "undefined" && appState && appState.lastLabel.length > 0)
                                         ? appState.lastLabel
                                         : ((typeof appState !== "undefined" && appState) ? appState.lastModel : "")
    property bool _simInputsLoaded: false

    function resetSimInputs() {
        simInputModel.clear()
        simInputModel.append({ name: "input[0]", label: "", shape: "-", min: "0.0", max: "1.0" })
        root._simInputsLoaded = false
    }

    function loadSimInputs() {
        simInputModel.clear()
        var specs = (typeof backend !== "undefined" && backend) ? backend.deployedModelInputSpecs() : []
        if (!specs || specs.length === 0) {
            resetSimInputs()
            return
        }
        for (var i = 0; i < specs.length; i++) {
            simInputModel.append({
                name: specs[i].name || ("input[" + i + "]"),
                label: specs[i].label || "",
                shape: specs[i].shape || "-",
                min: Number(specs[i].min || 0).toFixed(4),
                max: Number(specs[i].max || 1).toFixed(4)
            })
        }
        root._simInputsLoaded = true
    }

    function collectSimInputs() {
        var ranges = []
        for (var i = 0; i < simInputModel.count; i++) {
            var row = simInputModel.get(i)
            var minValue = parseFloat(row.min)
            var maxValue = parseFloat(row.max)
            if (isNaN(minValue)) minValue = 0.0
            if (isNaN(maxValue)) maxValue = 1.0
            ranges.push({ name: row.name, label: row.label, min: minValue, max: maxValue })
        }
        return ranges
    }

    Component.onCompleted: loadSimInputs()

    ListModel { id: simInputModel }

    Connections {
        target: (typeof appState !== "undefined") ? appState : null
        function onLastModelChanged() { root.loadSimInputs() }
        function onLastSensorChanged() { root.loadSimInputs() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "UART Monitor"
                subtitle: "Canli protokol akisi ve kart metrikleri"
                Layout.fillWidth: true
            }
            StatusPill { text: root._connText; status: root._connected ? "connected" : "offline" }
            AppButton {
                text: "Temizle"
                variant: "secondary"
                onClicked: if (typeof backend !== "undefined" && backend) backend.clearMonitor()
            }
            AppButton {
                text: "Kaydet"
                variant: "secondary"
                onClicked: monitorSaveDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            // Terminal expands/shrinks with the window; the info sidebar keeps
            // a fixed width right beside it.
            Card {
                title: "Terminal"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 260

                Terminal {
                    anchors.fill: parent
                    lines: root._lines
                }
            }

            ScrollView {
                id: sideScroll
                // Side info panel scales with the window (~36% of width) but
                // stays within readable bounds; terminal flexes into the rest.
                Layout.preferredWidth: Math.round(root.width * 0.36)
                Layout.minimumWidth: 300
                Layout.maximumWidth: 460
                Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                width: sideScroll.availableWidth
                spacing: Theme.spacingMd

                Repeater {
                    model: [
                        { title: "Inference", icon: "speed", accentProp: "primary" },
                        { title: "RAM", icon: "memory", accentProp: "success" },
                        { title: "Son Tahmin", icon: "target", accentProp: "cyan" }
                    ]
                    delegate: Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 90
                        padding: Theme.spacingMd

                        RowLayout {
                            anchors.fill: parent
                            spacing: Theme.spacingMd

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: Theme.radiusMd
                                color: Theme.alpha(Theme[modelData.accentProp], 0.16)
                                MetricIcon {
                                    anchors.centerIn: parent
                                    width: 22
                                    height: 22
                                    name: modelData.icon
                                    color: Theme[modelData.accentProp]
                                }
                            }

                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                Text {
                                    text: modelData.title
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                }
                                Text {
                                    text: {
                                        if (typeof appState === "undefined" || !appState) return "-"
                                        if (modelData.accentProp === "primary")
                                            return appState.lastInfMs > 0 ? (appState.lastInfMs.toFixed(2) + " ms") : "-"
                                        if (modelData.accentProp === "success")
                                            return appState.lastRamKb > 0 ? (appState.lastRamKb.toFixed(2) + " KB") : "-"
                                        return root._lastLabel.length > 0 ? (root._lastLabel + " / %" + appState.lastAcc) : "-"
                                    }
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXl
                                    font.weight: Font.Bold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                Card {
                    title: "Sistem"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 116

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingXs
                        Repeater {
                            model: ["Uptime", "Sicaklik", "Free RAM"]
                            delegate: RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: modelData
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXs
                                    Layout.preferredWidth: 82
                                }
                                Text {
                                    text: {
                                        if (typeof appState === "undefined" || !appState) return "-"
                                        if (modelData === "Uptime")
                                            return appState.lastUptime > 0 ? (appState.lastUptime + " s") : "-"
                                        if (modelData === "Sicaklik")
                                            return appState.lastTempC > 0 ? (appState.lastTempC.toFixed(1) + " C") : "-"
                                        return appState.lastFreeRamKb > 0 ? (appState.lastFreeRamKb.toFixed(1) + " KB") : "-"
                                    }
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                Card {
                    title: "Simulasyon"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 420

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Theme.spacingMd

                        // Active model + sensor info row
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            Text {
                                text: "Model:"
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: (typeof appState !== "undefined" && appState && appState.lastModel.length > 0)
                                      ? appState.lastModel : "-"
                                color: Theme.primary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: (typeof appState !== "undefined" && appState && appState.lastSensor.length > 0)
                                      ? appState.lastSensor : "MPU6050"
                                color: Theme.success
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                            }
                        }

                        Text { text: "Aralik (ms)"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold }
                        TextField {
                            id: intervalField
                            Layout.fillWidth: true
                            text: "500"
                            enabled: !root._simRunning
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSm
                            selectByMouse: true
                            background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: intervalField.activeFocus ? Theme.primary : Theme.border }
                            leftPadding: Theme.spacingSm
                            rightPadding: Theme.spacingSm
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    Layout.fillWidth: true
                                    text: root._simInputsLoaded ? "Model inputlari" : "Varsayilan input"
                                    color: Theme.textMuted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                }
                                AppButton { text: "Yenile"; variant: "secondary"; enabled: !root._simRunning; onClicked: root.loadSimInputs() }
                            }
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 108
                                clip: true
                                ColumnLayout {
                                    width: parent.width
                                    spacing: Theme.spacingXs
                                    Repeater {
                                        model: simInputModel
                                        delegate: RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Theme.spacingSm
                                            Text {
                                                Layout.fillWidth: true
                                                text: (model.label && model.label.length > 0 ? model.label : model.name)
                                                color: Theme.text
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontXs
                                                elide: Text.ElideRight
                                            }
                                            TextField {
                                                id: simMinInput
                                                Layout.preferredWidth: 74
                                                text: model.min
                                                enabled: !root._simRunning
                                                color: Theme.text
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontXs
                                                selectByMouse: true
                                                background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: simMinInput.activeFocus ? Theme.primary : Theme.border }
                                                leftPadding: Theme.spacingXs; rightPadding: Theme.spacingXs
                                                onEditingFinished: simInputModel.setProperty(index, "min", text)
                                            }
                                            TextField {
                                                id: simMaxInput
                                                Layout.preferredWidth: 74
                                                text: model.max
                                                enabled: !root._simRunning
                                                color: Theme.text
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontXs
                                                selectByMouse: true
                                                background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: simMaxInput.activeFocus ? Theme.primary : Theme.border }
                                                leftPadding: Theme.spacingXs; rightPadding: Theme.spacingXs
                                                onEditingFinished: simInputModel.setProperty(index, "max", text)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            AppButton {
                                text: root._simRunning ? "Calisiyor..." : "Host"
                                iconText: ">"
                                enabled: !root._simRunning
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.startSimulationWithInputs(parseInt(intervalField.text) || 500,
                                                                      root.collectSimInputs())
                                }
                            }
                            AppButton {
                                text: "Karta"
                                iconText: ">"
                                variant: "secondary"
                                enabled: !root._simRunning
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.startHardwareSimulationWithInputs(parseInt(intervalField.text) || 500,
                                                                              root.collectSimInputs())
                                }
                            }
                            AppButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: "Durdur"
                                iconText: "x"
                                variant: "danger"
                                enabled: root._simRunning
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.stopSimulation()
                                    backend.stopHardwareSimulation()
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
                }
            }
        }
    }

    FileDialog {
        id: monitorSaveDialog
        title: "Monitor log kaydet"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Text (*.txt *.log)", "Tum dosyalar (*)"]
        onAccepted: if (typeof backend !== "undefined" && backend) backend.saveMonitorLog(selectedFile.toString().replace("file:///", ""))
    }
}
