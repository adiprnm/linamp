import QtQuick
import "Theme.js" as Theme
import "Presets.js" as Presets

WinPanel {
    id: eqPanel
    property var ctrl
    title: "EQUALIZER"
    bodyHeight: 104

    property string presetName: "FLAT"
    property string readoutCenter: ""
    property var bandLabels: ["60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K"]

    onCloseClicked: ctrl.eqOpen = false
    onMinimizeClicked: ctrl.eqOpen = false

    ListModel { id: eqModel }

    AppMenu {
        id: presetMenu
        width: 180
    }

    Timer {
        id: readoutTimer
        interval: 1600
        onTriggered: { eqPanel.readoutCenter = "" }
    }

    Component.onCompleted: {
        var b = engine.bands
        var nonZero = false
        for (var i = 0; i < 10; ++i) {
            var g = b && b.length > i ? b[i] : 0
            if (Math.abs(g) > 0.01) nonZero = true
            eqModel.append({ gain: g, label: bandLabels[i] })
        }
        eqPanel.presetName = nonZero ? "CUSTOM" : "FLAT"
    }

    function markBand(index, v) {
        eqPanel.presetName = "CUSTOM"
        eqPanel.readoutCenter = bandLabels[index] + " " + Theme.signed(v) + " DB"
        readoutTimer.restart()
    }

    function applyPreset(p) {
        eqPanel.presetName = p.name
        eqPanel.readoutCenter = ""
        for (var i = 0; i < 10; ++i) {
            eqModel.setProperty(i, "gain", p.bands[i])
            engine.setBand(i, p.bands[i])
        }
        engine.preamp = p.preamp
    }

    function openPresets() {
        var items = Presets.list.map(function (p) {
            return { label: p.name, checked: p.name === eqPanel.presetName,
                     action: function () { eqPanel.applyPreset(p) } }
        })
        items.push({ label: "RESET TO FLAT", action: function () { eqPanel.applyPreset(Presets.list[0]) } })
        presetMenu.items = items
        presetMenu.columns = 2
        presetMenu.width = 330
        presetMenu.x = 85
        presetMenu.y = 10
        presetMenu.open()
    }

    // ---------------- toolbar ----------------
    MetalButton {
        x: 8; y: 2
        width: 32; height: 14
        label: "ON"
        showLed: true
        active: engine.eqOn
        tip: "EQ on/off"
        onClicked: engine.eqOn = !engine.eqOn
    }
    MetalButton {
        x: 43; y: 2
        width: 38; height: 14
        label: "AUTO"
        showLed: true
        active: engine.eqAuto
        tip: "Auto gain"
        onClicked: engine.eqAuto = !engine.eqAuto
    }
    MetalButton {
        id: presetButton
        x: 85; y: 2
        width: 135; height: 14
        label: eqPanel.presetName
        trailingIcon: "caret"
        tip: "Presets"
        onClicked: eqPanel.openPresets()
    }
    MetalButton {
        x: 224; y: 2
        width: 35; height: 14
        label: "RESET"
        tip: "Reset ke flat"
        onClicked: eqPanel.applyPreset(Presets.list[0])
    }

    // ---------------- bands ----------------
    Row {
        id: bandsRow
        x: 8 + Math.round((259 - width) / 2)
        y: 20
        height: 72
        spacing: 0

        Item {
            width: 24; height: 72
            Rectangle {
                x: 3; y: 29
                width: 18; height: 1
                color: "#5c5c5c"
            }
            VSlider {
                x: 0; y: 0
                width: 24; height: 58
                value: engine.preamp
                from: -12; to: 12
                defaultValue: 0
                onMoved: (v) => { engine.preamp = v; eqPanel.presetName = "CUSTOM"; eqPanel.readoutCenter = "PREAMP " + Theme.signed(v) + " DB"; readoutTimer.restart() }
            }
            PixelText {
                y: 60
                anchors.horizontalCenter: parent.horizontalCenter
                text: "PRE"
                color: "#8a9c8a"
            }
        }

        Repeater {
            model: eqModel
            delegate: Item {
                width: 22; height: 72
                Rectangle {
                    x: 3; y: 29
                    width: 16; height: 1
                    color: "#5c5c5c"
                }
                VSlider {
                    x: 0; y: 0
                    width: 22; height: 58
                    value: gain
                    from: -12; to: 12
                    defaultValue: 0
                    onMoved: (v) => {
                        eqModel.setProperty(index, "gain", v)
                        engine.setBand(index, v)
                        eqPanel.markBand(index, v)
                    }
                }
                PixelText {
                    y: 60
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: label
                    color: "#8a9c8a"
                }
            }
        }
    }

    // ---------------- readout ----------------
    LcdBox {
        id: readout
        x: 8; y: 92
        width: 259; height: 12

        PixelText {
            x: 4; y: 2
            text: "PREAMP " + Theme.signed(engine.preamp) + " DB"
            color: "#57a35c"
        }
        PixelText {
            width: parent.width
            y: 2
            text: eqPanel.readoutCenter
            color: "#8fe09a"
            align: "center"
        }
        PixelText {
            width: parent.width - 4
            y: 2
            text: engine.eqOn ? "EQ ON" : "EQ OFF"
            color: engine.eqOn ? "#43ff5f" : "#2a5c32"
            align: "right"
        }
    }

}
