import QtQuick
import "PixelFont.js" as PF

Item {
    id: root
    property string text: ""
    property int pixelScale: 1
    property string color: "#d6d6d6"
    property int spacing: 1
    property string align: "left"

    implicitWidth: PF.measure(text, pixelScale, spacing)
    implicitHeight: PF.CH_H * pixelScale

    Canvas {
        id: cv
        anchors.fill: parent
        antialiasing: false
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.text) return
            var w = PF.measure(root.text, root.pixelScale, root.spacing)
            var x = 0
            if (root.align === "center") x = (width - w) / 2
            else if (root.align === "right") x = width - w
            PF.draw(ctx, root.text, Math.round(x), 0, root.pixelScale, root.color, root.spacing)
        }
    }

    onTextChanged: cv.requestPaint()
    onColorChanged: cv.requestPaint()
    onPixelScaleChanged: cv.requestPaint()
    onSpacingChanged: cv.requestPaint()
    onWidthChanged: cv.requestPaint()
    Component.onCompleted: cv.requestPaint()
}
