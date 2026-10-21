import QtQuick
import QtQuick.Controls

Popup {
    id: root
    property var items: []
    property int fontSize: 11
    property int columns: 1

    width: 200
    padding: 2
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle {
        color: "#c0c0c0"
        border.width: 1
        border.color: "#000000"
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: "#ffffff" }
        Rectangle { x: 1; y: 1; width: 1; height: parent.height - 2; color: "#ffffff" }
        Rectangle { x: 1; y: parent.height - 2; width: parent.width - 2; height: 1; color: "#808080" }
        Rectangle { x: parent.width - 2; y: 1; width: 1; height: parent.height - 2; color: "#808080" }
    }

    contentItem: Grid {
        width: root.width - 4
        columns: root.columns
        spacing: 0
        Repeater {
            model: root.items
            delegate: Item {
                id: entry
                width: root.columns > 1 ? (root.width - 4) / root.columns : root.width - 4
                height: !modelData ? 0 : (modelData.sep ? 6 : 18)

                Rectangle {
                    visible: !modelData || !modelData.sep
                    anchors.fill: parent
                    color: hover.containsMouse && !(modelData && modelData.disabled) ? "#000080" : "transparent"
                }
                Rectangle {
                    visible: !!(modelData && modelData.sep)
                    x: 2; y: 2; width: parent.width - 4; height: 2
                    color: "#808080"
                    Rectangle { width: parent.width; height: 1; color: "#ffffff" }
                }
                Text {
                    visible: !modelData || !modelData.sep
                    x: 20; anchors.verticalCenter: parent.verticalCenter
                    text: modelData && modelData.label ? modelData.label : ""
                    color: hover.containsMouse && !(modelData && modelData.disabled) ? "#ffffff"
                         : (modelData && modelData.disabled ? "#808080" : "#000000")
                    font.pixelSize: root.fontSize
                    font.family: "Tahoma, sans-serif"
                    elide: Text.ElideRight
                    width: parent.width - 30
                }
                Text {
                    visible: !modelData || !modelData.sep
                    text: modelData && modelData.checked ? "✓" : ""
                    x: 5; anchors.verticalCenter: parent.verticalCenter
                    color: hover.containsMouse ? "#ffffff" : "#000000"
                    font.pixelSize: 10
                }
                Text {
                    visible: !modelData || !modelData.sep
                    text: modelData && modelData.hint ? modelData.hint : ""
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    color: hover.containsMouse ? "#ffffff" : "#666666"
                    font.pixelSize: 9
                    font.family: "Tahoma, sans-serif"
                }
                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !!(modelData && !modelData.sep && !modelData.disabled)
                    onClicked: {
                        root.close()
                        if (modelData && modelData.action) modelData.action()
                    }
                }
            }
        }
    }
}
