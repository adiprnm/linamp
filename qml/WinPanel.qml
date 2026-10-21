import QtQuick

Item {
    id: root
    property string title: "LINAMP"
    property string version: ""
    property int bodyHeight: 104
    property bool shaded: false
    property bool showLogo: true
    property bool showMin: true
    property var snapCallback: null
    property var raiseCallback: null

    signal closeClicked()
    signal minimizeClicked()
    signal shadeToggled()
    signal logoClicked()
    signal moved()

    width: 275
    height: 14 + (shaded ? 0 : bodyHeight)
    default property alias content: contentItem.data

    Rectangle { anchors.fill: parent; color: "#0b0b0b" }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: "#2a2a2a"
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: "transparent"
        border.width: 1
        border.color: "#5a5a5a"
    }

    // ---------------- title bar ----------------
    Item {
        id: titlebar
        width: parent.width
        height: 14
        y: 0

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#7d7d7d" }
                GradientStop { position: 0.42; color: "#5e5e5e" }
                GradientStop { position: 0.58; color: "#444444" }
                GradientStop { position: 1.0; color: "#303030" }
            }
        }
        Rectangle { x: 0; y: 0; width: parent.width; height: 1; color: "#bdbdbd"; opacity: 0.55 }
        Rectangle { x: 0; y: parent.height - 1; width: parent.width; height: 1; color: "#080808" }

        Rectangle {
            visible: root.showLogo
            x: 4; y: 2
            width: 10; height: 10
            color: "#111111"
            border.width: 1
            border.color: "#050505"
            PixelIcon {
                anchors.fill: parent
                anchors.margins: 1
                icon: "play"
                color: "#2dff4d"
            }
        }

        Canvas {
            visible: !root.showLogo
            x: 4; y: 3
            width: 10; height: 10
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = "#9a9a9a"
                for (var yy = 1; yy < height; yy += 4)
                    for (var xx = 1; xx < width; xx += 4)
                        ctx.fillRect(xx, yy, 1, 1)
            }
        }

        PixelText {
            id: titleText
            x: 18; y: 4
            text: root.title
            color: "#e4e4e4"
        }
        PixelText {
            visible: root.version !== ""
            x: 18 + titleText.implicitWidth + 4
            y: 4
            text: root.version
            color: "#9a9a9a"
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent
            property real pressX: 0
            property real pressY: 0
            property real startX: 0
            property real startY: 0
            property bool movedSincePress: false
            onPressed: (mouse) => {
                var sp = dragArea.mapToItem(null, mouse.x, mouse.y)
                pressX = sp.x
                pressY = sp.y
                startX = root.x
                startY = root.y
                movedSincePress = false
                if (root.raiseCallback) root.raiseCallback(root)
            }
            onPositionChanged: (mouse) => {
                if (!pressed) return
                var sp = dragArea.mapToItem(null, mouse.x, mouse.y)
                var nx = startX + sp.x - pressX
                var ny = startY + sp.y - pressY
                movedSincePress = true
                if (root.snapCallback) {
                    var s = root.snapCallback(root, nx, ny)
                    nx = s.x; ny = s.y
                }
                root.x = nx
                root.y = ny
            }
            onReleased: {
                if (movedSincePress)
                    root.moved()
            }
            onDoubleClicked: {
                root.shaded = !root.shaded
                root.shadeToggled()
            }
        }

        Row {
            x: parent.width - width - 3
            y: 1
            spacing: 1
            MetalButton {
                width: 12; height: 12
                icon: "min"
                tip: "Minimize"
                onClicked: root.minimizeClicked()
            }
            MetalButton {
                width: 12; height: 12
                icon: "shade"
                tip: "Shade"
                onClicked: { root.shaded = !root.shaded; root.shadeToggled() }
            }
            MetalButton {
                width: 12; height: 12
                icon: "close"
                tip: "Close"
                onClicked: root.closeClicked()
            }
        }

        MouseArea {
            x: 3; y: 1
            width: 14; height: 12
            onClicked: root.logoClicked()
        }
    }

    // ---------------- body ----------------
    Rectangle {
        visible: !root.shaded
        x: 0; y: 14
        width: parent.width
        height: root.height - 14
        color: "#242424"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#2c2c2c" }
            GradientStop { position: 0.5; color: "#262626" }
            GradientStop { position: 1.0; color: "#1e1e1e" }
        }
        Rectangle { x: 0; y: 0; width: parent.width; height: 1; color: "#111111" }
        Rectangle { x: 0; y: 1; width: 1; height: parent.height - 2; color: "#4a4a4a"; opacity: 0.5 }
        Rectangle { x: parent.width - 1; y: 1; width: 1; height: parent.height - 2; color: "#111111" }
        Rectangle { x: 0; y: parent.height - 1; width: parent.width; height: 1; color: "#0c0c0c" }
    }

    Item {
        id: contentItem
        visible: !root.shaded
        x: 0; y: 14
        width: root.width
        height: root.height - 14
    }
}
