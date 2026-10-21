import QtQuick

Item {
    id: root
    property string icon: ""
    property string trailingIcon: ""
    property string label: ""
    property int labelScale: 1
    property bool active: false
    property bool showLed: false
    property string tip: ""

    signal clicked()
    signal rightClicked()

    implicitWidth: 24
    implicitHeight: 18
    z: mouse.containsMouse ? 50 : 0

    readonly property bool down: mouse.pressed || active

    Rectangle { anchors.fill: parent; color: "#070707" }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.down ? "#333333" : "#9a9a9a" }
            GradientStop { position: 0.42; color: root.down ? "#2b2b2b" : "#727272" }
            GradientStop { position: 0.60; color: root.down ? "#262626" : "#4d4d4d" }
            GradientStop { position: 1.0; color: root.down ? "#303030" : "#666666" }
        }
    }
    Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: root.down ? "#121212" : "#d2d2d2" }
    Rectangle { x: 1; y: 1; width: 1; height: parent.height - 2; color: root.down ? "#121212" : "#bdbdbd" }
    Rectangle { x: 1; y: parent.height - 2; width: parent.width - 2; height: 1; color: root.down ? "#6d6d6d" : "#2a2a2a" }
    Rectangle { x: parent.width - 2; y: 1; width: 1; height: parent.height - 2; color: root.down ? "#6d6d6d" : "#2a2a2a" }
    Rectangle { anchors.fill: parent; color: "#ffffff"; opacity: mouse.containsMouse && !root.down ? 0.07 : 0 }

    PixelIcon {
        visible: root.icon !== "" && root.label === ""
        icon: root.icon
        color: root.active ? "#46ff5e" : (root.down ? "#cfcfcf" : "#e6e6e6")
        anchors.centerIn: parent
        anchors.verticalCenterOffset: mouse.pressed ? 1 : 0
        width: 12
        height: 12
    }

    Row {
        visible: root.label !== ""
        anchors.centerIn: parent
        anchors.verticalCenterOffset: mouse.pressed ? 1 : 0
        spacing: 3
        Rectangle {
            visible: root.showLed
            width: 4; height: 4
            color: root.active ? "#3dff5e" : "#232323"
            border.width: 1
            border.color: "#000000"
            anchors.verticalCenter: parent.verticalCenter
        }
        PixelText {
            text: root.label
            pixelScale: root.labelScale
            color: root.active ? "#46ff5e" : "#e2e2e2"
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    PixelIcon {
        visible: root.trailingIcon !== ""
        icon: root.trailingIcon
        color: "#d2d2d2"
        width: 8; height: 6
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => { if (mouse.button === Qt.LeftButton) root.clicked() }
        onPressed: (mouse) => { if (mouse.button === Qt.RightButton) root.rightClicked() }
    }

    Rectangle {
        visible: mouse.containsMouse && root.tip !== ""
        z: 1000
        x: Math.round((root.width - width) / 2)
        y: -height - 6
        width: tipText.implicitWidth + 10
        height: 17
        color: "#eae8b4"
        border.width: 1
        border.color: "#101010"
        Text {
            id: tipText
            anchors.centerIn: parent
            text: root.tip
            color: "#101010"
            font.pixelSize: 10
            font.family: "Tahoma, sans-serif"
        }
    }
}
