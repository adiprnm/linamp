.pragma library

var bg = "#0b1115"
var text = "#d6d6d6"
var textDim = "#8d8d8d"
var lcdGreen = "#38ff55"
var lcdDim = "#1b8f2d"
var lcdSoft = "#57a35c"
var lcdMuted = "#37a24b"
var panel = "#242424"
var frame = "#8a8a8a"
var green = "#46ff5e"
var yellow = "#eae8b4"

function pad2(n) { return n < 10 ? "0" + n : "" + n }
function fmt(ms) {
    if (!isFinite(ms) || ms < 0) ms = 0
    var total = Math.floor(ms / 1000)
    var h = Math.floor(total / 3600)
    var m = Math.floor((total % 3600) / 60)
    var s = total % 60
    if (h > 0) return h + ":" + pad2(m) + ":" + pad2(s)
    return m + ":" + pad2(s)
}
function fmtLong(ms) {
    if (!isFinite(ms) || ms < 0) ms = 0
    var total = Math.floor(ms / 1000)
    var h = Math.floor(total / 3600)
    var m = Math.floor((total % 3600) / 60)
    var s = total % 60
    return pad2(h) + ":" + pad2(m) + ":" + pad2(s)
}
function signed(v) {
    var r = Math.round(v * 10) / 10
    var sign = r > 0 ? "+" : (r < 0 ? "-" : " ")
    return sign + Math.abs(r).toFixed(1)
}
function clamp(v, a, b) { return Math.max(a, Math.min(b, v)) }
