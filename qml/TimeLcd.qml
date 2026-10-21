import QtQuick
import "PixelFont.js" as PF

LcdBox {
    id: root
    property string text: "0:00"
    property bool dim: false
    signal toggled()

    Canvas {
        id: cv
        anchors.fill: parent
        antialiasing: false
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var txt = root.text || "0:00"
            var scale = 3
            while (scale > 2 && PF.measure(txt, scale, 2) > width - 4) scale--
            var w = PF.measure(txt, scale, 2)
            var h = PF.CH_H * scale
            PF.draw(ctx, txt, Math.round((width - w) / 2), Math.round((height - h) / 2),
                    scale, root.dim ? "#1b8f2d" : "#38ff55", 2)
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: root.toggled()
    }
    onTextChanged: cv.requestPaint()
    onDimChanged: cv.requestPaint()
    onWidthChanged: cv.requestPaint()
    onHeightChanged: cv.requestPaint()
    Component.onCompleted: cv.requestPaint()
}
