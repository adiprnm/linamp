import QtQuick
import "Theme.js" as Theme

WinPanel {
    id: mainPanel
    property var ctrl
    title: "LINAMP"
    version: "1.0"
    bodyHeight: 104

    onLogoClicked: openMainMenu()
    onMinimizeClicked: shell.minimize()
    onCloseClicked: shell.quit()

    property string timeText: {
        if (ctrl.remaining && engine.duration > 0)
            return "-" + Theme.fmt(Math.max(0, engine.duration - engine.position))
        return Theme.fmt(engine.position)
    }

    AppMenu {
        id: mainMenu
        width: 222
    }

    function openMainMenu() {
        mainMenu.items = [
            { label: engine.playing ? "Pause" : "Play", hint: "SPACE", action: () => ctrl.togglePlay() },
            { label: "Stop", hint: "V", action: () => ctrl.stopPlayback() },
            { label: "Previous", hint: "Z", action: () => ctrl.prev() },
            { label: "Next", hint: "B", action: () => ctrl.next(false) },
            { sep: true },
            { label: "Add files...", hint: "L", action: () => ctrl.requestFiles() },
            { label: "Add folder...", action: () => ctrl.requestFolder() },
            { sep: true },
            { label: "Equalizer", hint: "E", checked: ctrl.eqOpen, action: () => ctrl.toggleEq() },
            { label: "Playlist", hint: "L", checked: ctrl.playlistOpen, action: () => ctrl.togglePlaylist() },
            { label: "Elapsed / Remaining", hint: "T", checked: ctrl.remaining, action: () => ctrl.toggleRemaining() },
            { label: "Visualization: " + (ctrl.visMode === "bars" ? "Bars" : "Oscilloscope"), hint: "Y", action: () => ctrl.toggleVis() },
            { sep: true },
            { label: "Load demo tracks", action: () => ctrl.addDemos() },
            { label: "Keyboard shortcuts...", hint: "F1", action: () => ctrl.showHelp() },
            { sep: true },
            { label: "Quit Linamp", hint: "CTRL+Q", action: () => shell.quit() }
        ]
        mainMenu.x = 4
        mainMenu.y = 18
        mainMenu.open()
    }

    // ---------------- display ----------------
    Rectangle {
        x: 8; y: 2
        width: 259; height: 38
        color: "#202020"
        border.width: 1
        border.color: "#0a0a0a"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#303030" }
            GradientStop { position: 1.0; color: "#1e1e1e" }
        }
        Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: "#4c4c4c" }
        Rectangle { x: 1; y: parent.height - 2; width: parent.width - 2; height: 1; color: "#0d0d0d" }

        TimeLcd {
            x: 2; y: 2
            width: 92; height: 32
            text: mainPanel.timeText
            dim: !engine.playing
            onToggled: ctrl.toggleRemaining()
        }

        LcdBox {
            id: visBox
            x: 98; y: 2
            width: 155; height: 32
            Spectrum {
                anchors.fill: parent
                spectrum: engine.spectrum
                waveform: engine.waveform
                playing: engine.playing
                mode: ctrl.visMode
            }
            MouseArea {
                anchors.fill: parent
                onDoubleClicked: ctrl.toggleVis()
            }
        }
    }

    // ---------------- marquee ----------------
    LcdBox {
        x: 8; y: 42
        width: 259; height: 14
        Marquee {
            x: 3; y: 0
            width: 181; height: 12
            text: ctrl.marqueeText
            paused: !engine.playing
        }
        StatusText {
            x: 188; y: 3
            width: 66
            kbps: engine.bitrate
            khz: Math.round(engine.sampleRate / 1000)
            stereo: engine.channels > 1
        }
    }

    // ---------------- sliders ----------------
    Item {
        x: 8; y: 58
        width: 259; height: 14

        PixelIcon {
            x: 1; y: 1
            width: 12; height: 12
            icon: "speaker"
            color: "#cfcfcf"
        }

        HSlider {
            id: volSlider
            x: 16; y: 0
            width: 76; height: 14
            value: engine.volume
            defaultValue: 0.82
            onMoved: (v) => engine.volume = v
        }

        PixelText { x: 102; y: 4; text: "L"; color: "#9a9a9a" }
        HSlider {
            x: 112; y: 0
            width: 84; height: 14
            from: -1; to: 1
            value: engine.balance
            defaultValue: 0
            onMoved: (v) => engine.balance = v
        }
        PixelText { x: 198; y: 4; text: "R"; color: "#9a9a9a" }

        MetalButton {
            x: 210; y: 0
            width: 26; height: 14
            label: "EQ"
            showLed: true
            active: ctrl.eqOpen
            tip: "Equalizer (E)"
            onClicked: ctrl.toggleEq()
        }
        MetalButton {
            x: 238; y: 0
            width: 21; height: 14
            label: "PL"
            showLed: true
            active: ctrl.playlistOpen
            tip: "Playlist (L)"
            onClicked: ctrl.togglePlaylist()
        }
    }

    // ---------------- seek ----------------
    SeekBar {
        x: 8; y: 74
        width: 259; height: 8
        position: engine.position
        duration: engine.duration
        onSeekTo: (ms) => engine.seek(ms)
    }

    // ---------------- transport ----------------
    Item {
        x: 8; y: 86
        width: 259; height: 18

        MetalButton { x: 0; width: 24; height: 18; icon: "prev"; tip: "Previous (Z)"; onClicked: ctrl.prev() }
        MetalButton { x: 24; width: 24; height: 18; icon: "play"; tip: "Play (X)"; active: engine.playing; onClicked: ctrl.togglePlay() }
        MetalButton { x: 48; width: 24; height: 18; icon: "pause"; tip: "Pause (C)"; onClicked: engine.pause() }
        MetalButton { x: 72; width: 24; height: 18; icon: "stop"; tip: "Stop (V)"; onClicked: ctrl.stopPlayback() }
        MetalButton { x: 96; width: 24; height: 18; icon: "next"; tip: "Next (B)"; onClicked: ctrl.next(false) }
        MetalButton { x: 120; width: 24; height: 18; icon: "eject"; tip: "Open files (L)"; onClicked: ctrl.requestFiles() }
        MetalButton {
            x: 150; width: 47; height: 18
            icon: "shuffle"
            active: ctrl.shuffleOn
            tip: "Shuffle (S)"
            onClicked: ctrl.toggleShuffle()
        }
        MetalButton {
            x: 204; width: 47; height: 18
            icon: "repeat"
            active: ctrl.repeatOn
            tip: "Repeat (R)"
            onClicked: ctrl.toggleRepeat()
        }
    }
}
