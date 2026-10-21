import QtQuick

Item {
    id: root
    property real value: 0.5
    property real from: 0
    property real to: 1
    property real defaultValue: 0.5
    property bool dragging: false
    signal moved(real v)

    implicitHeight: 14

    readonly property real p: root.to === root.from ? 0 : (root.value - root.from) / (root.to - root.from)

    Rectangle {
        id: track
        width: parent.width
        height: 8
        anchors.verticalCenter: parent.verticalCenter
        color: "#050505"
        border.width: 1
        border.color: "#000000"
        Rectangle {
            x: 1; y: 1
            width: parent.width - 2
            height: 1
            color: "#000000"
            opacity: 0.8
        }
    }

    Rectangle {
        id: thumb
        width: 9; height: 14
        x: (track.width - width) * Math.max(0, Math.min(1, root.p))
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#dedede" }
            GradientStop { position: 0.45; color: "#9a9a9a" }
            GradientStop { position: 0.56; color: "#626262" }
            GradientStop { position: 1.0; color: "#8f8f8f" }
        }
        border.width: 1
        border.color: "#080808"
        Rectangle { x: 3; y: 2; width: 1; height: 9; color: "#2c2c2c"; opacity: 0.85 }
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: "#ffffff"; opacity: 0.55 }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPressed: { root.dragging = true; setFromX(mouse.x) }
        onPositionChanged: if (root.dragging) setFromX(mouse.x)
        onReleased: root.dragging = false
        onCanceled: root.dragging = false
        onDoubleClicked: root.moved(root.defaultValue)
        onWheel: (wheel) => {
            var step = (root.to - root.from) * 0.03
            root.moved(Math.max(root.from, Math.min(root.to, root.value + (wheel.angleDelta.y > 0 ? step : -step))))
        }
    }

    function setFromX(mx) {
        var p = Math.max(0, Math.min(1, (mx - 4.5) / Math.max(1, width - 9)))
        root.moved(root.from + (root.to - root.from) * p)
    }
}
