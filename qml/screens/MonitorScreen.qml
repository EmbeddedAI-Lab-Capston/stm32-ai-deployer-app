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

            Card {
                title: "Terminal"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 360

                Terminal {
                    anchors.fill: parent
                    lines: root._lines
                }
            }

            ScrollView {
                id: sideScroll
                Layout.preferredWidth: 300
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
                    Layout.preferredHeight: 290

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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text { text: "Min"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                                TextField {
                                    id: minField
                                    Layout.fillWidth: true
                                    text: "0.0"
                                    enabled: !root._simRunning
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    selectByMouse: true
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: minField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm
                                    rightPadding: Theme.spacingSm
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                Text { text: "Max"; color: Theme.textMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontXs }
                                TextField {
                                    id: maxField
                                    Layout.fillWidth: true
                                    text: "1.0"
                                    enabled: !root._simRunning
                                    color: Theme.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSm
                                    selectByMouse: true
                                    background: Rectangle { radius: Theme.radiusMd; color: Theme.surfaceRaised; border.color: maxField.activeFocus ? Theme.primary : Theme.border }
                                    leftPadding: Theme.spacingSm
                                    rightPadding: Theme.spacingSm
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
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.startSimulation(parseInt(intervalField.text) || 500,
                                                            parseFloat(minField.text) || 0.0,
                                                            parseFloat(maxField.text) || 1.0)
                                }
                            }
                            AppButton {
                                text: "Karta"
                                iconText: ">"
                                variant: "secondary"
                                enabled: !root._simRunning
                                onClicked: {
                                    if (typeof backend === "undefined" || !backend) return
                                    backend.startHardwareSimulation(parseInt(intervalField.text) || 500,
                                                                    parseFloat(minField.text) || 0.0,
                                                                    parseFloat(maxField.text) || 1.0)
                                }
                            }
                            AppButton {
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
