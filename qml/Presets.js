.pragma library

var list = [
    { name: "FLAT", preamp: 0, bands: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0] },
    { name: "CLASSICAL", preamp: 0, bands: [0, 0, 0, 0, 0, 0, -4, -4, -4, -6] },
    { name: "CLUB", preamp: 0, bands: [0, 0, 8, 5, 5, 5, 3, 0, 0, 0] },
    { name: "DANCE", preamp: 0, bands: [5, 7, 6, 0, 0, -3, -4, -4, 0, 0] },
    { name: "FULL BASS", preamp: 0, bands: [9, 9, 9, 5, 1, -4, -8, -11, -11, -11] },
    { name: "FULL BASS+TREBLE", preamp: 0, bands: [6, 6, 6, 2, -2, -6, -8, -8, -8, -8] },
    { name: "FULL TREBLE", preamp: 0, bands: [-9, -9, -9, -4, 2, 8, 11, 12, 12, 12] },
    { name: "LAPTOP", preamp: 0, bands: [4, 3, 1, 1, 0, -1, -2, -3, -4, -5] },
    { name: "LARGE HALL", preamp: 0, bands: [6, 6, 3, 3, 0, -2, -3, -3, -3, -3] },
    { name: "LIVE", preamp: 0, bands: [-2, 0, 2, 3, 3, 3, 2, 1, 1, 1] },
    { name: "PARTY", preamp: 0, bands: [6, 6, 0, 0, 0, 0, 0, 0, 6, 6] },
    { name: "POP", preamp: 0, bands: [-2, -1, 0, 2, 4, 4, 2, 0, -1, -2] },
    { name: "REGGAE", preamp: 0, bands: [0, 0, 0, -4, 0, 4, 4, 0, 0, 0] },
    { name: "ROCK", preamp: 0, bands: [7, 5, 2, -1, -2, 2, 5, 7, 7, 7] },
    { name: "SKA", preamp: 0, bands: [-1, -3, -3, 0, 3, 6, 6, 5, 3, 1] },
    { name: "SOFT", preamp: 0, bands: [3, 1, 0, -2, -2, -1, 0, 0, 1, 1] },
    { name: "SOFT ROCK", preamp: 0, bands: [3, 3, 1, 0, -1, -1, 0, 1, 2, 2] },
    { name: "TECHNO", preamp: 0, bands: [5, 5, 2, -2, -3, -1, 3, 6, 6, 6] },
    { name: "VOCAL", preamp: 0, bands: [-2, -2, -1, 2, 4, 4, 3, 1, 0, -1] }
]
function byName(n) {
    for (var i = 0; i < list.length; ++i) if (list[i].name === n) return list[i]
    return list[0]
}
