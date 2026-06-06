import QtQuick
import QtQuick.Layouts
import STM32AiDeployer

// A factory zone tile on the map: header (name + health) plus a flow of
// NodeMarkers for every STM32 node in the zone.
Rectangle {
    id: root

    property var zone: ({})       // { id, name, accent, hint, status, nodeCount, sensorCount }
    property var nodes: []        // list of node maps in this zone
    signal zoneActivated(int zoneId)
    signal nodeActivated(int nodeId)

    readonly property color accentColor: {
        switch (zone.accent) {
        case "cyan":    return Theme.cyan
        case "primary": return Theme.primary
        case "warning": return Theme.warning
        case "purple":  return Theme.purple
        case "success": return Theme.success
        default:        return Theme.primary
        }
    }

    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.alpha(accentColor, 0.35)

    // accent header strip
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 4
        radius: Theme.radiusLg
        color: root.accentColor
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        anchors.topMargin: Theme.spacingMd + 4
        spacing: Theme.spacingSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm

            Rectangle {
                Layout.preferredWidth: 30; Layout.preferredHeight: 30
                radius: Theme.radiusSm
                color: Theme.alpha(root.accentColor, 0.18)
                Text {
                    anchors.centerIn: parent
                    text: root.zone.id !== undefined ? root.zone.id : "?"
                    color: root.accentColor
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.Bold
                }
            }

            ColumnLayout {
                spacing: 0
                Layout.fillWidth: true
                Text {
                    text: root.zone.name !== undefined ? root.zone.name : ""
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: (root.zone.nodeCount !== undefined ? root.zone.nodeCount : 0) + " node · "
                          + (root.zone.sensorCount !== undefined ? root.zone.sensorCount : 0) + " sensör"
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontXs
                    Layout.fillWidth: true
                }
            }

            StatusPill {
                status: root.zone.status !== undefined ? root.zone.status : "ok"
                text: root.zone.status === "error" ? "Kritik"
                      : (root.zone.status === "warning" ? "Uyarı" : "Normal")
            }
        }

        // node markers
        Flow {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingXs
            Repeater {
                model: root.nodes
                delegate: NodeMarker {
                    node: modelData
                    onClicked: (id) => root.nodeActivated(id)
                }
            }
        }

        // footer: open zone detail
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: Theme.radiusSm
            color: footMouse.containsMouse ? Theme.surfaceHover : "transparent"
            border.color: Theme.border
            Text {
                anchors.centerIn: parent
                text: "Bölge detayını aç →"
                color: footMouse.containsMouse ? Theme.primary : Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: footMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.zoneActivated(root.zone.id)
            }
        }
    }
}
