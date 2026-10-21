import QtQuick

Canvas {
    id: root
    property string icon: ""
    property string color: "#e6e6e6"
    implicitWidth: 12
    implicitHeight: 12
    antialiasing: false

    function tri(ctx, x1, y1, x2, y2, x3, y3) {
        ctx.beginPath()
        ctx.moveTo(x1, y1)
        ctx.lineTo(x2, y2)
        ctx.lineTo(x3, y3)
        ctx.closePath()
        ctx.fill()
    }

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.fillStyle = root.color
        ctx.strokeStyle = root.color
        ctx.lineWidth = 1.2
        ctx.lineCap = "square"
        ctx.lineJoin = "miter"

        if (root.icon === "prev") {
            ctx.fillRect(1, 1, 2, 10)
            tri(ctx, 11, 1, 5, 6, 11, 11)
        } else if (root.icon === "play") {
            tri(ctx, 3, 1, 11, 6, 3, 11)
        } else if (root.icon === "pause") {
            ctx.fillRect(3, 1, 2, 10)
            ctx.fillRect(7, 1, 2, 10)
        } else if (root.icon === "stop") {
            ctx.fillRect(2, 2, 8, 8)
        } else if (root.icon === "next") {
            tri(ctx, 1, 1, 7, 6, 1, 11)
            ctx.fillRect(9, 1, 2, 10)
        } else if (root.icon === "eject") {
            tri(ctx, 1, 5, 6, 1, 11, 5)
            ctx.fillRect(1, 7, 10, 2)
        } else if (root.icon === "shuffle") {
            ctx.beginPath()
            ctx.moveTo(1, 3); ctx.lineTo(4, 3); ctx.lineTo(8, 9); ctx.lineTo(11, 9)
            ctx.moveTo(1, 9); ctx.lineTo(4, 9); ctx.lineTo(8, 3); ctx.lineTo(11, 3)
            ctx.stroke()
            tri(ctx, 9, 1.5, 11.5, 3, 9, 4.5)
            tri(ctx, 9, 7.5, 11.5, 9, 9, 10.5)
        } else if (root.icon === "repeat") {
            ctx.beginPath()
            ctx.moveTo(3, 2); ctx.lineTo(9, 2); ctx.lineTo(9, 5)
            ctx.moveTo(10, 3); ctx.lineTo(10, 7)
            ctx.moveTo(9, 10); ctx.lineTo(3, 10); ctx.lineTo(3, 7)
            ctx.moveTo(2, 9); ctx.lineTo(2, 5)
            ctx.stroke()
            tri(ctx, 8, 0.5, 10.5, 2, 8, 3.5)
            tri(ctx, 4, 8.5, 1.5, 10, 4, 11.5)
        } else if (root.icon === "min") {
            ctx.fillRect(2, 8, 8, 1)
        } else if (root.icon === "shade") {
            ctx.fillRect(2, 2, 8, 1)
            ctx.fillRect(2, 2, 1, 7)
            ctx.fillRect(2, 8, 8, 1)
            ctx.fillRect(9, 2, 1, 7)
            ctx.fillRect(2, 5, 8, 1)
        } else if (root.icon === "close") {
            ctx.beginPath()
            ctx.moveTo(2.5, 2.5); ctx.lineTo(8.5, 8.5)
            ctx.moveTo(8.5, 2.5); ctx.lineTo(2.5, 8.5)
            ctx.stroke()
        } else if (root.icon === "caret") {
            tri(ctx, 1, 2, 9, 2, 5, 6)
        } else if (root.icon === "speaker") {
            ctx.fillRect(1, 4, 2, 4)
            tri(ctx, 3, 4, 6, 1, 6, 11)
            ctx.fillRect(3, 4, 2, 4)
            ctx.beginPath()
            ctx.arc(7.5, 6, 2.5, -1.0, 1.0)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(7.5, 6, 4.5, -0.9, 0.9)
            ctx.stroke()
        } else if (root.icon === "led") {
            ctx.fillRect(0, 0, 4, 4)
        }
    }

    onIconChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()
}
