import QtQuick

Canvas {
    anchors.fill: parent
    opacity: 0.5
    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.fillStyle = "rgba(0,0,0,0.30)"
        for (var y = 0; y < height; y += 3)
            ctx.fillRect(0, y, width, 1)
    }
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()
}
