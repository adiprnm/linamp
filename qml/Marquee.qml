import QtQuick
import "PixelFont.js" as PF

Item {
    id: root
    property string text: ""
    property bool paused: false
    property real offset: 0
    property real lastTs: 0

    Canvas {
        id: cv
        anchors.fill: parent
        antialiasing: false
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var txt = PF.norm(root.text)
            if (!txt) return
            var full = PF.measure(txt, 1, 1)
            var color = root.paused ? "#1f9a34" : "#3bff5a"
            if (full <= width - 4) {
                PF.draw(ctx, txt, 2, 2, 1, color, 1)
                return
            }
            var gap = 24
            ctx.save()
            ctx.beginPath()
            ctx.rect(0, 0, width, height)
            ctx.clip()
            var x = 2 - root.offset
            PF.draw(ctx, txt, Math.round(x), 2, 1, color, 1)
            PF.draw(ctx, txt, Math.round(x + full + gap), 2, 1, color, 1)
            ctx.restore()
        }
    }

    Timer {
        interval: 33
        running: true
        repeat: true
        onTriggered: {
            var now = Date.now()
            var dt = root.lastTs ? Math.min(80, now - root.lastTs) : 33
            root.lastTs = now
            var full = PF.measure(root.text, 1, 1)
            if (!root.paused && full > root.width - 4) {
                root.offset += dt * 0.028
                if (root.offset > full + 24) root.offset = 0
            }
            cv.requestPaint()
        }
    }

    onTextChanged: { root.offset = 0; cv.requestPaint() }
    onPausedChanged: cv.requestPaint()
    onWidthChanged: cv.requestPaint()
    Component.onCompleted: cv.requestPaint()
}
