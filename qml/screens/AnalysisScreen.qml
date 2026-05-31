import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import STM32AiDeployer

Item {
    id: root

    property var subTabs: ["Benchmark", "Simülasyon", "Gerçek Sensör", "Derlenen Modeller"]
    property int subIndex: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingLg

        RowLayout {
            Layout.fillWidth: true
            SectionHeader {
                title: "Analiz"
                subtitle: "Karşılaştırmalı performans ve kaynak analizi"
                Layout.fillWidth: true
            }
            AppButton { text: "CSV"; iconText: "⭳"; variant: "secondary" }
            AppButton { text: "PDF"; iconText: "⭳"; variant: "secondary" }
        }

        // ── Sub tabs ──────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            Repeater {
                model: root.subTabs
                delegate: Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: subLabel.implicitWidth + Theme.spacingLg
                    radius: Theme.radiusMd
                    property bool active: index === root.subIndex
                    color: active ? Theme.primarySoft : Theme.surface
                    border.color: active ? Theme.primary : Theme.border

                    Text {
                        id: subLabel
                        anchors.centerIn: parent
                        text: modelData
                        color: active ? Theme.text : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSm
                        font.weight: active ? Font.DemiBold : Font.Normal
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.subIndex = index
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ── Summary cards ─────────────────────────────────────────────────
        GridLayout {
            Layout.fillWidth: true
            columns: root.width > 1100 ? 4 : 2
            columnSpacing: Theme.spacingMd
            rowSpacing: Theme.spacingMd

            Repeater {
                model: MockData.analysisSummary
                delegate: InfoCard {
                    title: modelData.title
                    value: modelData.value
                    accent: modelData.accent
                }
            }
        }

        // ── Table + chart ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingMd

            Card {
                title: "Kayıtlar"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 420

                DataTable {
                    anchors.fill: parent
                    columns: [
                        { title: "Oturum", width: 110 },
                        { title: "Kart", width: 0 },
                        { title: "Model", width: 0 },
                        { title: "Q", width: 50 },
                        { title: "Süre", width: 80 },
                        { title: "RAM", width: 80 },
                        { title: "Acc", width: 55 }
                    ]
                    rows: MockData.analysisRows
                }
            }

            Card {
                title: "Inference Süresi"
                subtitle: "Model karşılaştırması"
                Layout.preferredWidth: 400
                Layout.fillWidth: false
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.spacingMd

                    Item { Layout.fillWidth: true; Layout.fillHeight: true
                        RowLayout {
                            anchors.fill: parent
                            anchors.bottomMargin: 24
                            spacing: Theme.spacingMd

                            Repeater {
                                model: MockData.analysisBars
                                delegate: ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: Theme.spacingXs

                                    Item { Layout.fillHeight: true; Layout.fillWidth: true
                                        Rectangle {
                                            anchors.bottom: parent.bottom
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            width: parent.width * 0.6
                                            height: parent.height * Math.max(0.04, modelData.value)
                                            radius: Theme.radiusSm
                                            gradient: Gradient {
                                                GradientStop { position: 0.0; color: Theme.primary }
                                                GradientStop { position: 1.0; color: Theme.cyan }
                                            }

                                            Text {
                                                anchors.bottom: parent.top
                                                anchors.bottomMargin: 4
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: modelData.text
                                                color: Theme.textMuted
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontXs
                                            }
                                        }
                                    }

                                    Text {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: modelData.label
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontXs
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
