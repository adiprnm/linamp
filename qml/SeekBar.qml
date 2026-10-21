import QtQuick
import "Theme.js" as Theme

Item {
    id: root
    property real position: 0
    property real duration: 0
    property bool dragging: false
    property real preview: 0
    signal seekTo(real ms)

    implicitHeight: 8

    readonly property real p: {
        if (root.dragging) return root.duration > 0 ? Math.max(0, Math.min(1, root.preview / root.duration)) : 0
        return root.duration > 0 ? Math.max(0, Math.min(1, root.position / root.duration)) : 0
    }

    Rectangle {
        anchors.fill: parent
        color: "#070707"
        border.width: 1
        border.color: "#000000"
    }
    Rectangle {
        x: 1; y: 1
        width: Math.max(0, (parent.width - 2) * root.p)
        height: parent.height - 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#176b2c" }
            GradientStop { position: 0.5; color: "#26c247" }
            GradientStop { position: 1.0; color: "#0d4a1c" }
        }
        opacity: 0.75
    }
    Rectangle {
        x: (parent.width - 7) * root.p
        y: -2
        width: 7; height: 12
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#dedede" }
            GradientStop { position: 0.45; color: "#8b8b8b" }
            GradientStop { position: 0.58; color: "#5d5d5d" }
            GradientStop { position: 1.0; color: "#8c8c8c" }
        }
        border.width: 1
        border.color: "#080808"
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPressed: { root.dragging = true; updatePreview(mouse.x) }
        onPositionChanged: if (root.dragging) updatePreview(mouse.x)
        onReleased: {
            if (!root.dragging) return
            root.dragging = false
            if (root.duration > 0) root.seekTo(root.preview)
        }
        onCanceled: root.dragging = false
    }

    Rectangle {
        visible: root.dragging
        z: 50
        width: tipText.implicitWidth + 10
        height: 17
        x: Math.max(0, Math.min(root.width - width, (root.width - 7) * root.p + 3 - width / 2))
        y: -21
        color: "#eae8b4"
        border.width: 1
        border.color: "#101010"
        Text {
            id: tipText
            anchors.centerIn: parent
            text: Theme.fmt(root.preview)
            color: "#101010"
            font.pixelSize: 10
            font.family: "Tahoma, sans-serif"
        }
    }

    function updatePreview(mx) {
        if (root.duration <= 0) return
        var p = Math.max(0, Math.min(1, mx / Math.max(1, width)))
        root.preview = p * root.duration
    }
}
