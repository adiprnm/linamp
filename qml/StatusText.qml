import QtQuick

PixelText {
    id: root
    property int kbps: 0
    property int khz: 44
    property bool stereo: true
    property string custom: ""

    text: custom !== "" ? custom
        : (root.kbps > 0 ? String(root.kbps).padStart(3, " ") + "K " + root.khz + "K " + (root.stereo ? "ST" : "MO")
                         : "---K --K --")
    color: "#1f9a34"
    pixelScale: 1
    align: "right"
}
