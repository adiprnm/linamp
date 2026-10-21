# LINAMP

**Linamp** (Linux + Winamp) adalah pemutar musik desktop native bergaya **Winamp 2.x**,
dibuat dengan **Qt 6 / Qt Quick (C++ + QML)** dan **FFmpeg**. Tanpa Electron, tanpa
Node, tanpa browser yang dibundel.

![Linamp](screenshot.png)

*Screenshot memakai background gradient agar panel terlihat — di desktop, area kosong
sepenuhnya transparan (hanya panel Winamp yang tampil).*

## Fitur

**Player**
- Play / pause / stop / next / previous / eject, shuffle, repeat.
- Seek bar, volume, balance, mute, elapsed/remaining.
- Bitrate, sample rate, dan indikator stereo asli dari stream.
- Format mengikuti FFmpeg: MP3, FLAC, OGG/Opus, M4A/AAC, WAV, WMA, dan lainnya.
- Drag & drop file/folder ke jendela, atau tombol **ADD** / eject.
- File dialog native (Qt Quick Dialogs / xdg-desktop-portal).

**Audio engine native**
- Decode FFmpeg → resample 48 kHz stereo float (`libswresample`) → ring buffer → `QAudioSink`.
- Equalizer **10 band asli** (biquad peaking 60 Hz–16 kHz) + preamp, diproses per-sample.
- AUTO gain untuk mengompensasi boost EQ, plus clip guard.
- FFT 2048-point → spectrum analyzer 31 band + oscilloscope, sinkron dengan audio.
- **Semua state terakhir** disimpan di `~/.config/linamp/`: playlist + urutan/sort terakhir, track aktif, posisi & ukuran panel, tinggi playlist, panel yang terbuka/shaded, panel terdepan, mode visualizer, elapsed/remaining, shuffle/repeat, EQ, volume, dan balance.

**UI ala Winamp**
- Tiga panel: main window, equalizer, playlist — bisa digeser, di-snap, dan di-shade.
- Selalu terbuka sebagai jendela **floating** di Hyprland (window rule otomatis), tanpa border/shadow/rounding.
- **Background sepenuhnya transparan** — yang terlihat hanya panel Winamp, langsung di atas desktop.
- Titlebar metalik, LCD hijau dengan scanline, tombol bevel, tooltip kuning.
- Bitmap font 5×7 buatan sendiri, dirender Canvas.
- Playlist canvas dengan seleksi (Ctrl/Shift), scroll, sorting, shuffle, resize tinggi panel.
- Menu klasik Win95 (klik logo LINAMP), help overlay (F1), drag & drop overlay.
- Frameless + transparent window: drag area kosong (transparan) untuk memindahkan jendela aplikasi.

## Build & jalankan

Kebutuhan (Arch/Omarchy):

```bash
sudo pacman -S --needed qt6-base qt6-declarative qt6-multimedia ffmpeg \
    pkgconf gcc make cmake
```

PipeWire atau PulseAudio harus aktif untuk output audio.

```bash
# build
./build.sh

# jalankan
./run.sh

# pasang launcher aplikasi + icon (build otomatis)
./install-desktop.sh
```

Setelah install, cari **Linamp** di launcher Omarchy.

Build manual dengan qmake:

```bash
cd build
qmake6 ../linamp.pro && make -j$(nproc)
```

### Build CMake (release / lintas platform)

Build CMake meng-embed **QML + demo MP3 + icon** ke dalam binary lewat
`resources.qrc`, jadi hasil build tidak butuh folder `qml/` atau `assets/` di
sampingnya. Build qmake di atas tetap memakai QML dari `qml/` untuk development.

Kebutuhan: CMake 3.21+, Qt 6.5+ (Quick, QuickControls2, QuickDialogs2,
Multimedia), dan FFmpeg dev library.

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --parallel "$(nproc)"
./build-cmake/linamp
```

- **Windows**: pakai FFmpeg shared build dan arahkan `-DFFMPEG_ROOT=C:\path\ke\ffmpeg`
  (berisi `include/`, `lib/`, `bin/`), lalu deploy dengan `windeployqt`.
- **macOS**: `brew install ffmpeg dylibbundler`, lalu `macdeployqt` + `dylibbundler`.

## Rilis multi-platform

Workflow GitHub Actions ada di `.github/workflows/release.yml` dan dipicu oleh
tag `v*` (bisa juga dijalankan manual dari tab **Actions**; build artifact tetap
tersedia, release hanya dibuat untuk tag):

```bash
git tag v1.0.0
git push origin v1.0.0
```

Workflow membangun tiga artifact dan mempublikasikannya sebagai GitHub Release:

| Platform | Artifact | Catatan |
| --- | --- | --- |
| Linux x86_64 (glibc 2.39+) | `linamp-linux-x86_64.AppImage` | Qt + FFmpeg dibundel; window rule Hyprland hanya untuk Hyprland |
| Windows x64 | `linamp-windows-x64.zip` | Ekstrak lalu jalankan `linamp.exe`; DLL Qt + FFmpeg ikut |
| macOS arm64 | `linamp-macos-arm64.dmg` | Apple Silicon; ad-hoc signed (belum notarized) |

`SHA256SUMS.txt` ikut diunggah.

Catatan rilis:

- Build Windows/macOS belum code-signed/notarized. SmartScreen/Gatekeeper bisa
  menampilkan peringatan; di macOS klik kanan → **Open**.
- Build macOS saat ini **arm64** (Apple Silicon). Intel butuh build terpisah.
- “Selalu floating” adalah perilaku Hyprland. Di Windows/macOS jendela berjalan
  sebagai window biasa (tidak ada tiling seperti Hyprland).
- QML di-embed saat build CMake; untuk mengubah QML di build release perlu rebuild.

## Selalu floating

Window rule Hyprland sudah dipasang di `~/.config/hypr/hyprland.lua`:

```lua
-- Linamp music player: floating, transparan, tanpa border/shadow/rounding.
o.window({ class = "^[lL]inamp$" }, {
    float = true,
    size = { 640, 440 },
    border_size = 0,
    rounding = 0,
    no_shadow = true,
    no_blur = true,
    tag = "-default-opacity",
    opacity = "1.0 override 1.0 override"
})
```

Rule ini membuat Linamp selalu terbuka sebagai floating window transparan berukuran
640×440 — tanpa border, shadow, rounding, atau blur dari compositor.
Hapus blok tersebut jika ingin ikut tiling seperti jendela lain.
Kalau class-nya berbeda di sistem lain, cek dengan `hyprctl clients`.

Catatan: rule ini khusus **Hyprland/Linux**. Di Windows/macOS tidak ada window
rule tersebut; aplikasi tetap jalan sebagai window biasa. Di Linux dengan WM lain,
tambahkan rule floating sesuai WM masing-masing.

## Shortcut

| Tombol | Fungsi |
| --- | --- |
| `Space` | Play / pause |
| `X` / `C` | Play / pause |
| `V` | Stop |
| `Z` / `B` | Previous / next |
| `←` / `→` | Seek ±5 detik (`Shift` = ±30 detik) |
| `↑` / `↓` | Volume |
| `M` | Mute |
| `S` / `R` | Shuffle / repeat |
| `T` | Elapsed / remaining |
| `Y` | Bars / oscilloscope |
| `E` / `L` | Equalizer / playlist |
| `Del` | Hapus track terpilih |
| `F1` | Help |
| `F11` | Fullscreen |
| `Ctrl+Q` | Quit |

## Struktur

```
  linamp.pro                     qmake project (development lokal)
  CMakeLists.txt                 build CMake (release / Windows / macOS)
  resources.qrc                  QML + demo mp3 + icon untuk build release
  build.sh                       build qmake out-of-source ke build/
  run.sh                         build-if-needed + jalankan
  install-desktop.sh             pasang icon + .desktop (Linux/Hyprland)
  packaging/
    linamp.desktop               desktop entry untuk AppImage/install
    linamp.rc.in                 resource icon Windows
  .github/workflows/release.yml  CI build Linux/Windows/macOS + release
  src/
    main.cpp                     QGuiApplication + QML engine
    audioengine.h/.cpp           FFmpeg decode, ring buffer, EQ, QAudioSink, FFT
    prober.h/.cpp                probe durasi file di background
    shell.h/.cpp                 startSystemMove, minimize, fullscreen, demo URL
  qml/
    Main.qml                     window, controller, playlist model, dialogs, keyboard
    MainPanel.qml                display, marquee, seek, transport
    PlaylistPanel.qml            playlist canvas + tombol + resize
    EqPanel.qml                  EQ 10 band + preset
    WinPanel.qml                 frame, titlebar, drag, shade
    PixelFont.js                 bitmap font 5×7
    Theme.js / Presets.js / komponen lain
  assets/                        demo mp3 + icon
```

## Cara kerja singkat

1. **AudioEngine** membuka file lewat libavformat, decode dengan libavcodec,
   lalu resample ke float stereo 48 kHz. Decoder berjalan di thread sendiri dan
   mengisi ring buffer 3 detik.
2. Thread audio `QAudioSink` menarik PCM dari ring buffer, menerapkan
   preamp → 10 biquad peaking → volume/balance, menghitung FFT, lalu menulis ke output.
3. Hasil FFT/waveform dikirim ke QML (queued signal) untuk spectrum/oscilloscope.
4. QML menggambar LCD, marquee, daftar playlist, dan panel metalik dengan
   `Canvas` + bitmap font supaya tetap tajam seperti skin Winamp asli.
5. QSettings menyimpan volume, balance, dan setting EQ.

## Demo audio

Dua track chiptune pendek (`assets/demo1.mp3`, `demo2.mp3`) di-generate
khusus untuk proyek ini dan di-tag sebagai **Linamp**. Di build CMake keduanya
di-embed dan diekstrak ke cache saat pertama dipakai.

Semua konfigurasi terakhir dipulihkan saat aplikasi dibuka lagi:

- daftar track, urutan hasil sort, dan track aktif
- posisi & ukuran panel, tinggi playlist, panel terbuka/shaded, panel terdepan
- mode visualizer (bars/oscilloscope), elapsed/remaining, shuffle/repeat
- volume, balance, dan setting EQ
- aksi sort terakhir (ditandai centang di menu SORT)

Jendela aplikasi tetap floating 640×440 di tengah monitor mengikuti window rule
Hyprland; yang dipulihkan adalah layout panel-panel Winamp di dalamnya.

Demo hanya dimuat otomatis saat belum ada playlist tersimpan; setelah itu
playlist tersimpan yang dipakai. Mengosongkan playlist juga tersimpan, jadi
tidak akan diisi demo lagi kecuali lewat menu **Load demo tracks**.

Proyek fan-made, tidak berafiliasi dengan Winamp/Nullsoft. Nama dan nuansa visual
dipakai sebagai penghormatan untuk player legendaris tersebut.

Build release membundel Qt (LGPL) dan FFmpeg (LGPL/GPL tergantung build FFmpeg
yang dipakai). Sertakan lisensi masing-masing saat mendistribusikan binary.
