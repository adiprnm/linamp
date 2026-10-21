import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import "Theme.js" as Theme

Window {
    id: win
    width: 640
    height: 440
    visible: true
    color: "transparent"
    title: "LINAMP"
    flags: Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint

    property int zCounter: 20

    Item {
        id: root
        anchors.fill: parent
        focus: true

        // controller state
        property var selected: ({})
        property int selectionCount: 0
        property int anchor: -1
        property int currentIndex: -1
        property int listRevision: 0
        property int totalDuration: 0
        property string marqueeText: ""
        property bool eqOpen: false
        property bool playlistOpen: true
        property bool helpOpen: false
        property bool shuffleOn: false
        property bool repeatOn: false
        property bool remaining: false
        property string visMode: "bars"
        property real savedVolume: 0.82
        property string dragInfo: ""
        property string lastSort: ""
        property string frontName: "mainPanel"
        property var pendingLayout: null

        ListModel { id: tracks }
        property var tracksModel: tracks

        MouseArea {
            anchors.fill: parent
            z: 0
            onPressed: shell.startMove()
        }

        // ---------------- panels ----------------
        MainPanel {
            id: mainPanel
            objectName: "mainPanel"
            ctrl: root
            z: 20
            x: 40; y: 60
            snapCallback: root.snapPanel
            raiseCallback: root.raisePanel
            onMoved: root.scheduleSave()
            onShadeToggled: root.scheduleSave()
        }

        PlaylistPanel {
            id: playlistPanel
            objectName: "playlistPanel"
            ctrl: root
            z: 21
            x: 331; y: 60
            visible: root.playlistOpen
            maxTotalHeight: root.height - 20
            snapCallback: root.snapPanel
            raiseCallback: root.raisePanel
            onMoved: root.scheduleSave()
            onShadeToggled: root.scheduleSave()
            onTotalHeightChanged: root.scheduleSave()
        }

        EqPanel {
            id: eqPanel
            objectName: "eqPanel"
            ctrl: root
            z: 22
            x: 40; y: 191
            visible: root.eqOpen
            snapCallback: root.snapPanel
            raiseCallback: root.raisePanel
            onMoved: root.scheduleSave()
            onShadeToggled: root.scheduleSave()
        }

        property bool arranged: false
        property bool playlistReady: false

        Timer {
            id: initialLayout
            interval: 120
            running: true
            onTriggered: {
                if (!root.pendingLayout)
                    root.layoutPanels()
                root.clampPanels()
                root.arranged = true
                root.finishRestore()
            }
        }

        // ---------------- file dialogs ----------------
        FileDialog {
            id: fileDialog
            title: "Add audio files"
            fileMode: FileDialog.OpenFiles
            nameFilters: ["Audio (*.mp3 *.wav *.ogg *.oga *.flac *.m4a *.aac *.opus *.wma)", "All files (*)"]
            onAccepted: root.addUrls(selectedFiles, true)
        }
        FolderDialog {
            id: folderDialog
            title: "Add folder"
            onAccepted: root.addUrls(shell.audioFilesInFolder(selectedFolder), true)
        }

        // ---------------- drag & drop ----------------
        DropArea {
            anchors.fill: parent
            z: 9000
            onEntered: root.dragInfo = "DROP AUDIO FILES TO PLAYLIST"
            onExited: root.dragInfo = ""
            onDropped: (drop) => {
                root.dragInfo = ""
                if (drop.hasUrls) root.addUrls(drop.urls, true)
            }
        }
        Rectangle {
            anchors.fill: parent
            visible: root.dragInfo !== ""
            z: 8990
            color: "#B3030905"
            Rectangle {
                anchors.centerIn: parent
                width: 430; height: 150
                color: "#07110a"
                border.width: 2
                border.color: "#2f8b3f"
                PixelText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 44
                    text: "DROP AUDIO FILES TO PLAYLIST"
                    pixelScale: 2
                    color: "#38ff55"
                }
                PixelText {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 90
                    text: "MP3 · WAV · OGG · FLAC · M4A"
                    color: "#5d8a63"
                }
            }
        }

        // ---------------- help overlay ----------------
        Rectangle {
            anchors.fill: parent
            z: 9500
            visible: root.helpOpen
            color: "#B3020603"
            MouseArea {
                anchors.fill: parent
                onClicked: root.helpOpen = false
            }
            Rectangle {
                width: 440
                height: 360
                anchors.centerIn: parent
                color: "#1c1c1c"
                border.width: 1
                border.color: "#000000"
                Rectangle { anchors.fill: parent; anchors.margins: 1; color: "transparent"; border.width: 1; border.color: "#6d6d6d" }
                MouseArea { anchors.fill: parent }

                Rectangle {
                    x: 1; y: 1; width: parent.width - 2; height: 15
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#6d6d6d" }
                        GradientStop { position: 1.0; color: "#3d3d3d" }
                    }
                    PixelText { x: 8; y: 4; text: "LINAMP"; color: "#38ff55" }
                    MetalButton {
                        x: parent.width - 16; y: 2
                        width: 12; height: 12
                        icon: "close"
                        onClicked: root.helpOpen = false
                    }
                }

                Column {
                    x: 24; y: 30
                    spacing: 7
                    Repeater {
                        model: [
                            ["SPACE", "Play / pause"],
                            ["X / C", "Play / pause"],
                            ["V", "Stop"],
                            ["Z / B", "Previous / next"],
                            ["← / →", "Seek ±5 detik (Shift: 30)"],
                            ["↑ / ↓", "Volume"],
                            ["M", "Mute"],
                            ["S / R", "Shuffle / repeat"],
                            ["T", "Elapsed / remaining"],
                            ["Y", "Bars / oscilloscope"],
                            ["E / L", "Equalizer / playlist"],
                            ["DEL", "Remove track terpilih"],
                            ["CTRL+Q / F11", "Quit / fullscreen"]
                        ]
                        delegate: Row {
                            spacing: 24
                            width: 392
                            Text {
                                width: 110
                                text: modelData[0]
                                color: "#38ff55"
                                font.pixelSize: 12
                                font.family: "Courier New, monospace"
                            }
                            Text {
                                text: modelData[1]
                                color: "#cfe4d2"
                                font.pixelSize: 12
                                font.family: "Courier New, monospace"
                            }
                        }
                    }
                }
                PixelText {
                    x: 24; y: parent.height - 24
                    text: "DRAG TITLEBAR · DOBEL-KLIK UNTUK SHADE"
                    color: "#5d7a62"
                }
            }
        }

        Timer {
            id: playlistSaveTimer
            interval: 400
            onTriggered: root.saveStateNow()
        }

        Connections {
            target: shell
            function onAboutToQuit() { root.saveStateNow() }
        }

        // ---------------- keyboard ----------------
        Keys.onPressed: (event) => {
            if (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier)) {
                if (event.key === Qt.Key_Q) { shell.quit(); event.accepted = true }
                else if (event.key === Qt.Key_M) { shell.minimize(); event.accepted = true }
                else if (event.key === Qt.Key_A) { root.selectAll(); event.accepted = true }
                return
            }
            if (event.key === Qt.Key_F11) { shell.toggleFullscreen(); event.accepted = true; return }
            if (event.key === Qt.Key_F1) { root.helpOpen = !root.helpOpen; event.accepted = true; return }
            if (event.key === Qt.Key_Escape) { root.helpOpen = false; event.accepted = true; return }

            switch (event.key) {
            case Qt.Key_Space:
                root.togglePlay(); event.accepted = true; break
            case Qt.Key_X:
                root.togglePlay(); event.accepted = true; break
            case Qt.Key_C:
                engine.pause(); event.accepted = true; break
            case Qt.Key_V:
                root.stopPlayback(); event.accepted = true; break
            case Qt.Key_B:
                root.next(false); event.accepted = true; break
            case Qt.Key_Z:
                root.prev(); event.accepted = true; break
            case Qt.Key_T:
                root.toggleRemaining(); event.accepted = true; break
            case Qt.Key_Y:
                root.toggleVis(); event.accepted = true; break
            case Qt.Key_E:
                root.toggleEq(); event.accepted = true; break
            case Qt.Key_L:
                root.togglePlaylist(); event.accepted = true; break
            case Qt.Key_S:
                root.toggleShuffle(); event.accepted = true; break
            case Qt.Key_R:
                root.toggleRepeat(); event.accepted = true; break
            case Qt.Key_M:
                root.toggleMute(); event.accepted = true; break
            case Qt.Key_Delete:
                root.removeSelected(); event.accepted = true; break
            case Qt.Key_Left:
                engine.seek(engine.position - ((event.modifiers & Qt.ShiftModifier) ? 30000 : 5000))
                event.accepted = true; break
            case Qt.Key_Right:
                engine.seek(engine.position + ((event.modifiers & Qt.ShiftModifier) ? 30000 : 5000))
                event.accepted = true; break
            case Qt.Key_Up:
                engine.volume = Theme.clamp(engine.volume + 0.05, 0, 1)
                event.accepted = true; break
            case Qt.Key_Down:
                engine.volume = Theme.clamp(engine.volume - 0.05, 0, 1)
                event.accepted = true; break
            }
        }

        // ---------------- lifecycle ----------------
        Component.onCompleted: {
            layoutPanels()
            root.frontName = "mainPanel"
            mainPanel.z = 23
            root.restorePlaylist()
        }

        onCurrentIndexChanged: root.scheduleSave()
        onWidthChanged: { bg.requestPaint(); if (root.arranged) root.clampPanels() }
        onHeightChanged: { bg.requestPaint(); if (root.arranged) root.clampPanels() }

        // ---------------- controller ----------------
        function layoutPanels() {
            var vw = (win && win.width > 0) ? win.width : width
            var vh = (win && win.height > 0) ? win.height : height
            var gap = 8
            var groupW = 275 * 2 + gap
            var groupH = 118 + gap + 118
            var left = Math.max(8, Math.round((vw - groupW) / 2))
            var top = Math.max(28, Math.round((vh - groupH) / 2))
            mainPanel.x = left
            mainPanel.y = top
            playlistPanel.x = left + 275 + gap
            playlistPanel.y = top
            eqPanel.x = left
            eqPanel.y = top + 118 + gap
        }

        function clampPanels() {
            var list = [mainPanel, playlistPanel, eqPanel]
            for (var i = 0; i < list.length; ++i) {
                var p = list[i]
                if (!p) continue
                p.x = Theme.clamp(p.x, 0, Math.max(0, width - p.width))
                p.y = Theme.clamp(p.y, 0, Math.max(0, height - p.height))
            }
            root.scheduleSave()
        }

        function raisePanel(panel) {
            panel.z = ++win.zCounter + 10
            if (panel.objectName)
                root.frontName = panel.objectName
            root.scheduleSave()
        }

        function snapPanel(panel, nx, ny) {
            var th = 10
            var xs = [0, width - panel.width]
            var ys = [0, height - panel.height]
            var list = [mainPanel, playlistPanel, eqPanel]
            for (var i = 0; i < list.length; ++i) {
                var o = list[i]
                if (!o || o === panel || !o.visible) continue
                xs.push(o.x, o.x + o.width - panel.width, o.x - panel.width, o.x + o.width)
                ys.push(o.y, o.y + o.height - panel.height, o.y - panel.height, o.y + o.height)
            }
            var bx = nx, by = ny, dx = th + 1, dy = th + 1
            for (var a = 0; a < xs.length; ++a) {
                var d = Math.abs(xs[a] - nx)
                if (d < dx) { dx = d; bx = xs[a] }
            }
            for (var b = 0; b < ys.length; ++b) {
                var d2 = Math.abs(ys[b] - ny)
                if (d2 < dy) { dy = d2; by = ys[b] }
            }
            bx = Theme.clamp(bx, 0, Math.max(0, width - panel.width))
            by = Theme.clamp(by, 0, Math.max(0, height - panel.height))
            return { x: bx, y: by }
        }

        function bump() {
            var total = 0
            var sel = 0
            for (var i = 0; i < tracks.count; ++i) {
                total += tracks.get(i).duration || 0
                if (root.selected["" + i]) sel++
            }
            root.totalDuration = total
            root.selectionCount = sel
            root.listRevision++
            root.scheduleSave()
        }

        function scheduleSave() {
            if (root.playlistReady)
                playlistSaveTimer.restart()
        }

        function saveStateNow() {
            if (!root.playlistReady)
                return
            playlistSaveTimer.stop()
            var arr = []
            for (var i = 0; i < tracks.count; ++i) {
                var r = tracks.get(i)
                arr.push({
                    url: r.url, name: r.name, title: r.title,
                    artist: r.artist, duration: r.duration
                })
            }
            shell.savePlaylist(JSON.stringify({
                version: 2,
                currentIndex: root.currentIndex,
                lastSort: root.lastSort,
                visMode: root.visMode,
                remaining: root.remaining,
                shuffle: root.shuffleOn,
                repeat: root.repeatOn,
                eqOpen: root.eqOpen,
                playlistOpen: root.playlistOpen,
                layout: {
                    main: {
                        x: Math.round(mainPanel.x), y: Math.round(mainPanel.y),
                        shaded: mainPanel.shaded
                    },
                    eq: {
                        x: Math.round(eqPanel.x), y: Math.round(eqPanel.y),
                        shaded: eqPanel.shaded
                    },
                    playlist: {
                        x: Math.round(playlistPanel.x), y: Math.round(playlistPanel.y),
                        shaded: playlistPanel.shaded,
                        height: Math.round(playlistPanel.totalHeight)
                    },
                    front: root.frontName
                },
                tracks: arr
            }))
        }

        function applyLayout(layout) {
            if (!layout) return
            if (layout.main) {
                if (typeof layout.main.x === "number" && isFinite(layout.main.x)) mainPanel.x = layout.main.x
                if (typeof layout.main.y === "number" && isFinite(layout.main.y)) mainPanel.y = layout.main.y
                mainPanel.shaded = !!layout.main.shaded
            }
            if (layout.eq) {
                if (typeof layout.eq.x === "number" && isFinite(layout.eq.x)) eqPanel.x = layout.eq.x
                if (typeof layout.eq.y === "number" && isFinite(layout.eq.y)) eqPanel.y = layout.eq.y
                eqPanel.shaded = !!layout.eq.shaded
            }
            if (layout.playlist) {
                if (typeof layout.playlist.x === "number" && isFinite(layout.playlist.x)) playlistPanel.x = layout.playlist.x
                if (typeof layout.playlist.y === "number" && isFinite(layout.playlist.y)) playlistPanel.y = layout.playlist.y
                playlistPanel.shaded = !!layout.playlist.shaded
                var h = Number(layout.playlist.height)
                if (isFinite(h) && h > 0) {
                    var wh = (win && win.height > 0) ? win.height : root.height
                    var maxH = Math.max(120, wh - 20)
                    playlistPanel.totalHeight = Theme.clamp(h, 120, maxH)
                }
            }
            if (layout.front === "eqPanel" || layout.front === "playlistPanel" || layout.front === "mainPanel") {
                var p = layout.front === "eqPanel" ? eqPanel
                      : layout.front === "playlistPanel" ? playlistPanel : mainPanel
                root.raisePanel(p)
            }
        }

        function finishRestore() {
            root.playlistReady = true
            root.bump()
            root.saveStateNow()
        }

        function restorePlaylist() {
            root.playlistReady = false
            var raw = shell.loadPlaylist()
            var data = null
            if (raw && raw.length > 0) {
                try { data = JSON.parse(raw) } catch (e) { data = null }
            }

            tracks.clear()
            root.selected = {}
            root.anchor = -1
            root.pendingLayout = null

            if (data && data.tracks && data.tracks.length !== undefined) {
                var list = data.tracks
                for (var i = 0; i < list.length; ++i) {
                    var r = list[i]
                    if (!r || r.url === undefined || r.url === "")
                        continue
                    var url = String(r.url)
                    tracks.append({
                        name: r.name || r.title || "UNTITLED",
                        title: r.title || r.name || "UNTITLED",
                        artist: r.artist || "",
                        url: url,
                        duration: r.duration > 0 ? r.duration : 0
                    })
                    if (!(r.duration > 0))
                        prober.probe(url)
                }

                root.currentIndex = (typeof data.currentIndex === "number" &&
                                     data.currentIndex >= 0 &&
                                     data.currentIndex < tracks.count)
                    ? data.currentIndex
                    : (tracks.count > 0 ? 0 : -1)
                root.lastSort = typeof data.lastSort === "string" ? data.lastSort : ""
                root.visMode = data.visMode === "osc" ? "osc" : "bars"
                root.remaining = data.remaining === true
                root.shuffleOn = data.shuffle === true
                root.repeatOn = data.repeat === true
                root.eqOpen = data.eqOpen === true
                root.playlistOpen = data.playlistOpen !== false
                root.pendingLayout = data.layout || null
                root.updateMarquee()
                root.bump()
                if (root.pendingLayout)
                    root.applyLayout(root.pendingLayout)
            } else {
                root.addDemos()
                root.currentIndex = tracks.count > 0 ? 0 : -1
                root.updateMarquee()
                root.bump()
                root.pendingLayout = null
            }
        }

        function addDemos() {
            var demos = [
                { n: 1, artist: "LINAMP", title: "DEMO LOOP 1" },
                { n: 2, artist: "LINAMP", title: "DEMO LOOP 2" }
            ]
            for (var i = 0; i < demos.length; ++i) {
                var u = String(shell.demoUrl(demos[i].n))
                var exists = false
                for (var j = 0; j < tracks.count; ++j)
                    if (tracks.get(j).url === u) { exists = true; break }
                if (exists) continue
                tracks.append({
                    name: demos[i].artist + " - " + demos[i].title,
                    title: demos[i].title,
                    artist: demos[i].artist,
                    url: u,
                    duration: 0
                })
                prober.probe(u)
            }
            if (root.currentIndex < 0 && tracks.count > 0)
                root.currentIndex = 0
            root.updateMarquee()
            root.bump()
        }

        function addUrls(urls, autoplay) {
            if (!urls || urls.length === undefined) return
            var first = -1
            var added = 0
            for (var i = 0; i < urls.length; ++i) {
                var u = String(urls[i])
                if (!/\.(mp3|wav|ogg|oga|flac|m4a|aac|opus|wma|aiff?)(\?.*)?$/i.test(u)) continue
                var exists = false
                for (var j = 0; j < tracks.count; ++j)
                    if (tracks.get(j).url === u) { exists = true; break }
                if (exists) continue
                var fname = decodeURIComponent(u.split("/").pop())
                var base = fname.replace(/\.[^.]+$/, "").replace(/_/g, " ").trim()
                var artist = ""
                var title = base
                var idx = base.indexOf(" - ")
                if (idx > 0) {
                    artist = base.substring(0, idx).trim()
                    title = base.substring(idx + 3).trim()
                }
                tracks.append({ name: base, title: title, artist: artist, url: u, duration: 0 })
                prober.probe(u)
                if (first < 0) first = tracks.count - 1
                added++
            }
            if (added > 0) {
                if (root.currentIndex < 0) root.currentIndex = first
                root.updateMarquee()
                root.bump()
                if (autoplay && !engine.playing) root.playIndex(first)
            }
        }

        function playIndex(i) {
            if (i < 0 || i >= tracks.count) return
            root.currentIndex = i
            var row = tracks.get(i)
            engine.open(row.url)
            engine.play()
            root.updateMarquee()
            root.bump()
        }

        function displayName(row) {
            if (!row) return ""
            var artist = row.artist ? String(row.artist).trim() : ""
            var title = row.title ? String(row.title).trim() : (row.name ? String(row.name).trim() : "")
            return artist !== "" ? artist + " - " + title : title
        }

        function sortKey(row) {
            var s = root.displayName(row)
            try { s = s.normalize("NFKD").replace(/[\u0300-\u036f]/g, "") } catch (e) {}
            return s.toLowerCase()
        }

        function naturalCompare(x, y) {
            var ax = String(x).match(/(\d+|\D+)/g) || []
            var bx = String(y).match(/(\d+|\D+)/g) || []
            var n = Math.min(ax.length, bx.length)
            for (var i = 0; i < n; ++i) {
                var a = ax[i], b = bx[i]
                var an = parseInt(a, 10), bn = parseInt(b, 10)
                if (!isNaN(an) && !isNaN(bn)) {
                    if (an !== bn) return an - bn
                } else {
                    if (a < b) return -1
                    if (a > b) return 1
                }
            }
            return ax.length - bx.length
        }

        function updateMarquee() {
            var row = root.currentIndex >= 0 && root.currentIndex < tracks.count ? tracks.get(root.currentIndex) : null
            root.marqueeText = row ? root.displayName(row) : "LINAMP 1.0  -  DROP FILES HERE"
        }

        function togglePlay() {
            if (engine.playing) { engine.pause(); return }
            if (engine.source !== "" && engine.duration > 0 && engine.position < engine.duration - 200) {
                engine.play()
                return
            }
            if (root.currentIndex >= 0) root.playIndex(root.currentIndex)
            else if (tracks.count > 0) root.playIndex(0)
        }

        function stopPlayback() {
            engine.stop()
        }

        function next(auto) {
            var n = tracks.count
            if (n === 0) return
            if (root.shuffleOn && n > 1) {
                var r
                do { r = Math.floor(Math.random() * n) } while (r === root.currentIndex)
                root.playIndex(r)
                return
            }
            if (root.currentIndex + 1 < n) root.playIndex(root.currentIndex + 1)
            else if (root.repeatOn) root.playIndex(0)
            else if (!auto) root.playIndex(0)
            else engine.stop()
        }

        function prev() {
            if (tracks.count === 0) return
            if (engine.position > 3000) { engine.seek(0); return }
            root.playIndex(root.currentIndex > 0 ? root.currentIndex - 1 : tracks.count - 1)
        }

        function selectRow(i, modifiers) {
            if (i < 0 || i >= tracks.count) return
            var shift = (modifiers & Qt.ShiftModifier) !== 0
            var ctl = (modifiers & Qt.ControlModifier) !== 0
            var sel = {}
            if (shift && root.anchor >= 0) {
                var a = Math.min(root.anchor, i)
                var b = Math.max(root.anchor, i)
                for (var k = a; k <= b; ++k) sel["" + k] = true
            } else if (ctl) {
                for (var key in root.selected) sel[key] = true
                if (sel["" + i]) delete sel["" + i]
                else sel["" + i] = true
                root.anchor = i
            } else {
                sel["" + i] = true
                root.anchor = i
            }
            root.selected = sel
            root.bump()
        }

        function selectAll() {
            var sel = {}
            for (var i = 0; i < tracks.count; ++i) sel["" + i] = true
            root.selected = sel
            root.bump()
        }

        function selectNone() {
            root.selected = {}
            root.anchor = -1
            root.bump()
        }

        function invertSelection() {
            var sel = {}
            for (var i = 0; i < tracks.count; ++i)
                if (!root.selected["" + i]) sel["" + i] = true
            root.selected = sel
            root.bump()
        }

        function removeSelected() {
            var indices = []
            for (var key in root.selected) {
                var i = parseInt(key)
                if (i >= 0 && i < tracks.count) indices.push(i)
            }
            if (indices.length === 0) return
            indices.sort(function (a, b) { return a - b })
            var removedCurrent = false
            var before = 0
            for (var k = 0; k < indices.length; ++k)
                if (indices[k] < root.currentIndex) before++
            for (var j = indices.length - 1; j >= 0; --j) {
                if (indices[j] === root.currentIndex) removedCurrent = true
                tracks.remove(indices[j], 1)
            }
            if (removedCurrent) {
                engine.stop()
                root.currentIndex = -1
            } else if (root.currentIndex >= 0) {
                root.currentIndex -= before
            }
            root.selected = {}
            root.anchor = -1
            root.updateMarquee()
            root.bump()
        }

        function clearPlaylist() {
            engine.stop()
            tracks.clear()
            root.currentIndex = -1
            root.selected = {}
            root.anchor = -1
            root.updateMarquee()
            root.bump()
        }

        function sortBy(mode) {
            var arr = []
            for (var i = 0; i < tracks.count; ++i) {
                var r = tracks.get(i)
                arr.push({ name: r.name, title: r.title, artist: r.artist, url: r.url, duration: r.duration })
            }
            var curUrl = root.currentIndex >= 0 ? tracks.get(root.currentIndex).url : ""
            if (mode === "shuffle") {
                for (var s = arr.length - 1; s > 0; --s) {
                    var t = Math.floor(Math.random() * (s + 1))
                    var tmp = arr[s]; arr[s] = arr[t]; arr[t] = tmp
                }
            } else if (mode === "reverse") {
                arr.reverse()
            } else if (mode === "duration") {
                arr.sort(function (a, b) {
                    var da = a.duration > 0 ? a.duration : Number.MAX_SAFE_INTEGER
                    var db = b.duration > 0 ? b.duration : Number.MAX_SAFE_INTEGER
                    return da - db
                })
            } else {
                arr.sort(function (a, b) {
                    return root.naturalCompare(root.sortKey(a), root.sortKey(b))
                })
            }
            tracks.clear()
            for (var k = 0; k < arr.length; ++k) tracks.append(arr[k])
            root.currentIndex = -1
            if (curUrl !== "") {
                for (var m = 0; m < tracks.count; ++m)
                    if (tracks.get(m).url === curUrl) { root.currentIndex = m; break }
            }
            root.selected = {}
            root.anchor = -1
            root.updateMarquee()
            root.lastSort = mode
            root.bump()
        }

        function toggleEq() { root.eqOpen = !root.eqOpen; root.scheduleSave() }
        function togglePlaylist() { root.playlistOpen = !root.playlistOpen; root.scheduleSave() }
        function toggleShuffle() {
            root.shuffleOn = !root.shuffleOn
            root.scheduleSave()
        }
        function toggleRepeat() {
            root.repeatOn = !root.repeatOn
            root.scheduleSave()
        }
        function toggleRemaining() { root.remaining = !root.remaining; root.scheduleSave() }
        function toggleVis() {
            root.visMode = root.visMode === "bars" ? "osc" : "bars"
            root.scheduleSave()
        }
        function toggleMute() {
            if (engine.volume > 0.001) {
                root.savedVolume = engine.volume
                engine.volume = 0
            } else {
                engine.volume = root.savedVolume > 0.05 ? root.savedVolume : 0.82
            }
        }
        function requestFiles() { fileDialog.open() }
        function requestFolder() { folderDialog.open() }
        function showHelp() { root.helpOpen = true }

        Connections {
            target: engine
            function onMetadataChanged() {
                if (root.currentIndex >= 0 && root.currentIndex < tracks.count) {
                    if (engine.title && engine.title !== "")
                        tracks.setProperty(root.currentIndex, "title", engine.title)
                    if (engine.artist !== undefined && engine.artist !== "")
                        tracks.setProperty(root.currentIndex, "artist", engine.artist)
                    if (engine.duration > 0)
                        tracks.setProperty(root.currentIndex, "duration", engine.duration)
                }
                root.updateMarquee()
                root.bump()
            }
            function onDurationChanged() {
                if (root.currentIndex >= 0 && root.currentIndex < tracks.count && engine.duration > 0) {
                    tracks.setProperty(root.currentIndex, "duration", engine.duration)
                    root.bump()
                }
            }
            function onFinished() { root.next(true) }
            function onErrorOccurred(message) { console.warn("Audio error:", message) }
        }

        Connections {
            target: prober
            function onProbed(url, durationMs) {
                if (durationMs <= 0) return
                for (var i = 0; i < tracks.count; ++i) {
                    if (tracks.get(i).url === url) {
                        tracks.setProperty(i, "duration", durationMs)
                        root.bump()
                        break
                    }
                }
            }
        }
    }
}
