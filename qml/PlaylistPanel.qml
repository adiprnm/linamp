import QtQuick
import "Theme.js" as Theme
import "PixelFont.js" as PF

WinPanel {
    id: playlistRoot
    property var ctrl
    property int totalHeight: 238
    property int maxTotalHeight: 700
    property int scroll: 0
    property int hoverRow: -1

    title: "PLAYLIST"
    bodyHeight: totalHeight - 14
    onCloseClicked: ctrl.playlistOpen = false
    onMinimizeClicked: ctrl.playlistOpen = false

    readonly property int listH: bodyHeight - 34
    readonly property int rowH: 12

    AppMenu { id: buttonMenu; width: 200 }
    AppMenu { id: ctxMenu; width: 190 }

    Canvas {
        id: listCanvas
        x: 0; y: 0
        width: playlistRoot.width
        height: playlistRoot.listH
        onPaint: playlistRoot.paintList(getContext("2d"), width, height)

        MouseArea {
            id: listMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) return
                var i = playlistRoot.rowAt(mouse.y)
                if (i < 0) {
                    ctrl.selectNone()
                    return
                }
                ctrl.selectRow(i, mouse.modifiers)
            }
            onDoubleClicked: {
                var i = playlistRoot.rowAt(mouse.y)
                if (i >= 0) ctrl.playIndex(i)
            }
            onPressed: (mouse) => {
                if (mouse.button !== Qt.RightButton) return
                var i = playlistRoot.rowAt(mouse.y)
                if (i < 0) return
                if (!ctrl.selected["" + i]) ctrl.selectRow(i, 0)
                ctxMenu.items = [
                    { label: "Play", action: function () { ctrl.playIndex(i) } },
                    { label: "Remove selected", action: function () { ctrl.removeSelected() } },
                    { sep: true },
                    { label: "Remove all tracks", action: function () { ctrl.clearPlaylist() } }
                ]
                ctxMenu.x = mouse.x
                ctxMenu.y = mouse.y + 4
                ctxMenu.open()
            }
            onPositionChanged: {
                var i = playlistRoot.rowAt(mouse.y)
                if (i !== playlistRoot.hoverRow) {
                    playlistRoot.hoverRow = i
                    listCanvas.requestPaint()
                }
            }
            onExited: {
                playlistRoot.hoverRow = -1
                listCanvas.requestPaint()
            }
            onWheel: (wheel) => {
                playlistRoot.setScroll(playlistRoot.scroll - wheel.angleDelta.y * 0.6)
            }
        }
    }

    // ---------------- buttons ----------------
    Item {
        x: 0
        y: playlistRoot.bodyHeight - 34
        width: playlistRoot.width
        height: 22

        MetalButton {
            x: 9; y: 3
            width: 49; height: 16
            label: "ADD"
            tip: "Add files (klik kanan: opsi lain)"
            onClicked: ctrl.requestFiles()
            onRightClicked: {
                buttonMenu.items = [
                    { label: "Add files...", action: function () { ctrl.requestFiles() } },
                    { label: "Add folder...", action: function () { ctrl.requestFolder() } }
                ]
                buttonMenu.x = 9
                buttonMenu.y = playlistRoot.bodyHeight - 13
                buttonMenu.open()
            }
        }
        MetalButton {
            x: 61; y: 3
            width: 49; height: 16
            label: "REM"
            tip: "Remove selected"
            onClicked: ctrl.removeSelected()
        }
        MetalButton {
            x: 113; y: 3
            width: 49; height: 16
            label: "SEL"
            tip: "Selection"
            onClicked: {
                buttonMenu.items = [
                    { label: "Select all", hint: "CTRL+A", action: function () { ctrl.selectAll() } },
                    { label: "Select none", action: function () { ctrl.selectNone() } },
                    { label: "Invert selection", action: function () { ctrl.invertSelection() } }
                ]
                buttonMenu.x = 113
                buttonMenu.y = playlistRoot.bodyHeight - 13
                buttonMenu.open()
            }
        }
        MetalButton {
            x: 165; y: 3
            width: 49; height: 16
            label: "SORT"
            tip: "Sort / shuffle list"
            onClicked: {
                buttonMenu.items = [
                    { label: "Sort by title", checked: ctrl.lastSort === "title", action: function () { ctrl.sortBy("title") } },
                    { label: "Sort by duration", checked: ctrl.lastSort === "duration", action: function () { ctrl.sortBy("duration") } },
                    { label: "Reverse order", checked: ctrl.lastSort === "reverse", action: function () { ctrl.sortBy("reverse") } },
                    { sep: true },
                    { label: "Shuffle list", checked: ctrl.lastSort === "shuffle", action: function () { ctrl.sortBy("shuffle") } }
                ]
                buttonMenu.x = 165
                buttonMenu.y = playlistRoot.bodyHeight - 13
                buttonMenu.open()
            }
        }
        MetalButton {
            x: 217; y: 3
            width: 49; height: 16
            label: "CLR"
            tip: "Remove all"
            onClicked: {
                buttonMenu.items = [
                    { label: "Remove all tracks", action: function () { ctrl.clearPlaylist() } },
                    { label: "Remove selected", action: function () { ctrl.removeSelected() } }
                ]
                buttonMenu.x = 217
                buttonMenu.y = playlistRoot.bodyHeight - 13
                buttonMenu.open()
            }
        }
    }

    // ---------------- status ----------------
    Canvas {
        id: statusCanvas
        x: 0
        y: playlistRoot.bodyHeight - 12
        width: playlistRoot.width
        height: 12
        onPaint: playlistRoot.paintStatus(getContext("2d"), width, height)
    }

    // ---------------- resize grip ----------------
    Canvas {
        id: grip
        x: playlistRoot.width - 14
        y: playlistRoot.bodyHeight - 14
        width: 12; height: 12
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "rgba(180,180,180,0.45)"
            for (var i = 3; i < 12; i += 3) {
                ctx.fillRect(width - i, height - 1, 1, 1)
                ctx.fillRect(width - 1, height - i, 1, 1)
            }
        }
    }
    MouseArea {
        x: playlistRoot.width - 15
        y: playlistRoot.bodyHeight - 15
        width: 14; height: 14
        cursorShape: Qt.SizeFDiagCursor
        property real pressY: 0
        property real startH: 0
        onPressed: { pressY = mouse.y; startH = playlistRoot.totalHeight }
        onPositionChanged: {
            if (!pressed) return
            var nh = Math.max(120, Math.min(playlistRoot.maxTotalHeight, startH + mouse.y - pressY))
            playlistRoot.totalHeight = nh
        }
    }

    Connections {
        target: ctrl
        function onListRevisionChanged() {
            playlistRoot.clampScroll()
            listCanvas.requestPaint()
            statusCanvas.requestPaint()
        }
        function onCurrentIndexChanged() {
            playlistRoot.ensureVisible(ctrl.currentIndex)
            listCanvas.requestPaint()
        }
    }

    onHeightChanged: { listCanvas.requestPaint(); statusCanvas.requestPaint() }
    onBodyHeightChanged: { playlistRoot.clampScroll(); listCanvas.requestPaint(); statusCanvas.requestPaint() }
    Component.onCompleted: {
        listCanvas.requestPaint()
        statusCanvas.requestPaint()
    }

    // ---------------- helpers ----------------
    function viewRows() { return Math.max(1, Math.floor((listH - 12) / rowH)) }
    function maxScroll() { return Math.max(0, ctrl.tracksModel.count * rowH - (listH - 12)) }
    function clampScroll() {
        scroll = Math.max(0, Math.min(maxScroll(), scroll))
    }
    function setScroll(v) {
        scroll = Math.max(0, Math.min(maxScroll(), v))
        listCanvas.requestPaint()
    }
    function rowAt(y) {
        if (y < 12) return -1
        var i = Math.floor((y - 12 + scroll) / rowH)
        return (i >= 0 && i < ctrl.tracksModel.count) ? i : -1
    }
    function ensureVisible(i) {
        if (i < 0) return
        var top = i * rowH
        var bottom = top + rowH
        var viewH = listH - 12
        if (top < scroll) scroll = top
        else if (bottom > scroll + viewH) scroll = bottom - viewH
        clampScroll()
        listCanvas.requestPaint()
    }

    function clipText(txt, maxW) {
        if (PF.measure(txt, 1, 1) <= maxW) return txt
        while (txt.length > 1 && PF.measure(txt + "..", 1, 1) > maxW)
            txt = txt.substring(0, txt.length - 1)
        return txt + ".."
    }

    function paintList(ctx, W, H) {
        ctx.clearRect(0, 0, W, H)
        ctx.fillStyle = "#040704"
        ctx.fillRect(0, 0, W, H)

        ctx.fillStyle = "#0b130b"
        ctx.fillRect(0, 0, W, 12)
        ctx.fillStyle = "#1e2a1e"
        ctx.fillRect(0, 11, W, 1)
        PF.draw(ctx, "#", 8, 3, 1, "#517051", 1)
        PF.draw(ctx, "TITLE", 30, 3, 1, "#517051", 1)
        var tw = PF.measure("TIME", 1, 1)
        PF.draw(ctx, "TIME", W - 10 - tw, 3, 1, "#517051", 1)

        var model = ctrl.tracksModel
        var total = model.count
        var viewH = H - 12
        clampScroll()
        var first = Math.floor(scroll / rowH)
        var off = scroll % rowH

        ctx.save()
        ctx.beginPath()
        ctx.rect(0, 12, W, viewH)
        ctx.clip()

        for (var i = first; i < total; ++i) {
            var y = 12 + (i - first) * rowH - off
            if (y > H) break
            var row = model.get(i)
            var isCur = i === ctrl.currentIndex
            var isSel = ctrl.selected && ctrl.selected["" + i]
            var isHover = i === hoverRow

            if (isCur) {
                ctx.fillStyle = "rgba(38,255,80,0.10)"
                ctx.fillRect(0, y, W - 8, rowH)
                ctx.fillStyle = "#3dff5e"
                ctx.fillRect(0, y, 2, rowH)
            } else if (isSel) {
                ctx.fillStyle = "#173a72"
                ctx.fillRect(0, y, W - 8, rowH)
            } else if (isHover) {
                ctx.fillStyle = "rgba(200,255,210,0.05)"
                ctx.fillRect(0, y, W - 8, rowH)
            }

            var textColor = isCur ? "#57ff74" : (isSel ? "#e6f0ff" : "#96b79a")
            var dimColor = isCur ? "#39d957" : (isSel ? "#9db8e0" : "#4a6a4c")

            var num = (i + 1) + "."
            var nw = PF.measure(num, 1, 1)
            PF.draw(ctx, num, 22 - nw, y + 3, 1, dimColor, 1)

            var title = PF.norm(ctrl.displayName(row))
            title = clipText(title, W - 8 - 30 - 46)
            PF.draw(ctx, title, 30, y + 3, 1, textColor, 1)

            var dur = row.duration > 0 ? Theme.fmt(row.duration) : "--:--"
            var dw = PF.measure(dur, 1, 1)
            PF.draw(ctx, dur, W - 12 - dw, y + 3, 1, dimColor, 1)

            if (isCur) {
                ctx.fillStyle = "#46ff63"
                ctx.fillRect(2, y + 4, 2, 4)
                ctx.fillRect(4, y + 5, 2, 2)
                ctx.fillRect(6, y + 6, 1, 1)
            }
        }
        ctx.restore()

        if (total === 0) {
            var l1 = "PLAYLIST IS EMPTY"
            var l2 = "DROP FILES OR PRESS ADD"
            PF.draw(ctx, l1, Math.round((W - PF.measure(l1, 1, 1)) / 2), Math.round(H / 2) - 12, 1, "#1f5c2a", 1)
            PF.draw(ctx, l2, Math.round((W - PF.measure(l2, 1, 1)) / 2), Math.round(H / 2) + 2, 1, "#143c1b", 1)
        }

        // scrollbar
        if (total * rowH > viewH + 0.5) {
            var thumbH = Math.max(18, Math.round(viewH * viewH / (total * rowH)))
            var maxTop = viewH - thumbH
            var top = maxScroll() > 0 ? Math.round((scroll / maxScroll()) * maxTop) : 0
            ctx.fillStyle = "#030603"
            ctx.fillRect(W - 8, 12, 8, viewH)
            ctx.fillStyle = "#0d1c0f"
            ctx.fillRect(W - 8, 12, 1, viewH)
            ctx.fillStyle = "#5b5b5b"
            ctx.fillRect(W - 7, 12 + top, 7, thumbH)
            ctx.fillStyle = "#8f8f8f"
            ctx.fillRect(W - 7, 12 + top, 1, thumbH)
            ctx.fillStyle = "#333333"
            ctx.fillRect(W - 2, 12 + top, 1, thumbH)
        }
    }

    function paintStatus(ctx, W, H) {
        ctx.clearRect(0, 0, W, H)
        ctx.fillStyle = "#050805"
        ctx.fillRect(0, 0, W, H)
        var n = ctrl.tracksModel.count
        var left = n + (n === 1 ? " TRACK" : " TRACKS")
        if (ctrl.selectionCount > 1) left += " / " + ctrl.selectionCount + " SEL"
        PF.draw(ctx, left, 8, 3, 1, "#4d7a4d", 1)
        var totalStr = Theme.fmt(ctrl.totalDuration)
        var tw = PF.measure(totalStr, 1, 1)
        PF.draw(ctx, totalStr, W - 10 - tw, 3, 1, "#4d7a4d", 1)
    }
}
