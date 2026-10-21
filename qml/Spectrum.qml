import QtQuick

Item {
    id: root
    property var spectrum: []
    property var waveform: []
    property bool playing: false
    property string mode: "bars"
    property var vals: []
    property var peaks: []
    property var lastWave: []

    Canvas {
        id: cv
        anchors.fill: parent
        antialiasing: false
        onPaint: {
            var ctx = getContext("2d")
            var W = width, H = height
            ctx.clearRect(0, 0, W, H)

            ctx.fillStyle = "rgba(40,255,80,0.055)"
            for (var yy = H - 8; yy > 0; yy -= 8) ctx.fillRect(0, yy, W, 1)
            for (var xx = 0; xx < W; xx += 5) ctx.fillRect(xx, 0, 1, H)

            if (root.mode === "osc") {
                var wave = root.waveform
                if (!wave || wave.length < 2) return
                ctx.strokeStyle = "#3bff5a"
                ctx.lineWidth = 1
                ctx.beginPath()
                var n = wave.length
                for (var i = 0; i < n; ++i) {
                    var x = i * (W - 1) / (n - 1)
                    var y = H / 2 - wave[i] * (H / 2 - 1) * 0.9
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke()
                return
            }

            var nBars = 31
            var step = W / nBars
            var bw = Math.max(2, Math.floor(step) - 1)
            var data = root.spectrum
            var v = root.vals, pk = root.peaks
            for (var b = 0; b < nBars; ++b) {
                var target = 0
                if (root.playing && data && data.length > b) target = data[b]
                var prev = v[b] || 0
                var val = root.playing ? Math.max(target, prev * 0.85) : prev * 0.90
                if (!root.playing && target > val) val = target
                v[b] = val
                var peak = Math.max(val, (pk[b] || 0) - 0.02)
                pk[b] = peak

                var bh = Math.round(val * (H - 1))
                if (bh <= 0) continue
                var x = Math.round(b * step)
                var yTop = H - bh
                var gEnd = Math.round(H - H * 0.58)
                var yEnd = Math.round(H - H * 0.83)

                ctx.fillStyle = "#1fe23c"
                ctx.fillRect(x, Math.max(yTop, gEnd), bw, Math.min(bh, H - Math.max(yTop, gEnd)))
                if (yTop < gEnd) {
                    ctx.fillStyle = "#dfe321"
                    ctx.fillRect(x, Math.max(yTop, yEnd), bw, gEnd - Math.max(yTop, yEnd))
                }
                if (yTop < yEnd) {
                    ctx.fillStyle = "#ff3b2f"
                    ctx.fillRect(x, yTop, bw, yEnd - yTop)
                }
                var ph = Math.round(peak * (H - 1))
                if (peak > 0.04) {
                    ctx.fillStyle = "rgba(220,255,225,0.85)"
                    ctx.fillRect(x, H - ph - 2, bw, 1)
                }
            }
            root.vals = v
            root.peaks = pk
        }
    }

    Timer {
        interval: 33
        running: true
        repeat: true
        onTriggered: cv.requestPaint()
    }

    onSpectrumChanged: cv.requestPaint()
    onWaveformChanged: cv.requestPaint()
    onPlayingChanged: cv.requestPaint()
    onModeChanged: cv.requestPaint()
    onWidthChanged: cv.requestPaint()
    onHeightChanged: cv.requestPaint()
    Component.onCompleted: cv.requestPaint()
}
