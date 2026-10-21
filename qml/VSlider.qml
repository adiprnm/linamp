import QtQuick

Item {
    id: root
    property real value: 0
    property real from: -12
    property real to: 12
    property real defaultValue: 0
    property bool dragging: false
    signal moved(real v)

    implicitWidth: 22
    implicitHeight: 58

    readonly property real p: root.to === root.from ? 0 : (root.value - root.from) / (root.to - root.from)

    Rectangle {
        id: track
        width: 8
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        color: "#060606"
        border.width: 1
        border.color: "#000000"
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: "#000000" }
        Rectangle { x: parent.width - 2; y: 1; width: 1; height: parent.height - 2; color: "#2a2a2a" }
    }

    Rectangle {
        width: 12; height: 9
        x: Math.round((parent.width - width) / 2)
        y: (track.height - height) * (1 - Math.max(0, Math.min(1, root.p)))
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#e4e4e4" }
            GradientStop { position: 0.45; color: "#9d9d9d" }
            GradientStop { position: 0.58; color: "#666666" }
            GradientStop { position: 1.0; color: "#929292" }
        }
        border.width: 1
        border.color: "#080808"
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: "#ffffff"; opacity: 0.6 }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPressed: { root.dragging = true; setFromY(mouse.y) }
        onPositionChanged: if (root.dragging) setFromY(mouse.y)
        onReleased: root.dragging = false
        onCanceled: root.dragging = false
        onDoubleClicked: root.moved(root.defaultValue)
        onWheel: (wheel) => {
            var step = (root.to - root.from) * 0.03
            root.moved(Math.max(root.from, Math.min(root.to, root.value + (wheel.angleDelta.y > 0 ? step : -step))))
        }
    }

    function setFromY(my) {
        var p = 1 - (my - 4.5) / Math.max(1, height - 9)
        p = Math.max(0, Math.min(1, p))
        root.moved(root.from + (root.to - root.from) * p)
    }
}
