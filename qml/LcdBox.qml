import QtQuick

Rectangle {
    id: root
    color: "#000000"
    clip: true
    default property alias content: contentItem.data

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#08170b" }
            GradientStop { position: 0.55; color: "#040b06" }
            GradientStop { position: 1.0; color: "#020503" }
        }
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: "transparent"
        border.width: 1
        border.color: "#12351a"
        opacity: 0.7
    }
    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: 1
    }
    Scanlines {
        anchors.fill: parent
        anchors.margins: 1
        opacity: 0.35
    }
}
