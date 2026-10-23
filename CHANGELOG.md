# Changelog

## 2.1.1 — path now-playing di Windows

### Diperbaiki
- Di Windows, `nowplaying.json` dulu ditulis ke path relatif (`linamp-cache/`)
  karena `XDG_RUNTIME_DIR`/`HOME` tidak ada. Sekarang memakai
  `%LOCALAPPDATA%\linamp\nowplaying.json`, lokasi yang sama yang dibaca Linyrics.
- `state.json`, cache demo, dan config juga pindah ke `%APPDATA%\linamp` /
  `%LOCALAPPDATA%\linamp` di Windows, jadi tidak lagi bergantung pada direktori
  kerja saat aplikasi dijalankan. State lama di folder relatif tidak dimigrasi.

## 2.1.0 — now-playing untuk aplikasi pendamping

### Ditambah
- **Now-playing publisher**: Linamp menulis snapshot kecil ke
  `$XDG_RUNTIME_DIR/linamp/nowplaying.json` (judul, artis, path, durasi, posisi,
  status, `seq`, plus stempel `CLOCK_MONOTONIC`) ±10×/detik. Dipakai aplikasi
  pendamping seperti [Linyrics](../linyrics) untuk menampilkan lirik sinkron,
  tanpa perlu menambah D-Bus/MPRIS. Ditulis atomik (tmp + rename) dan dibersihkan
  saat keluar.

## 2.0.0 — SDL3 rewrite

Ditulis ulang tanpa Qt. Tampilan, DSP, dan perilaku skin tetap sama, tapi
AppImage turun dari **103 MB** ke **±3,5 MB**.

### Berubah
- **Renderer baru**: C++ / SDL3. Semua panel digambar ke satu canvas ARGB lalu
  di-upload ke satu texture SDL (nearest-neighbour, tanpa scene graph GPU).
  Menggantikan Qt Quick + QML (2815 baris QML → ~3300 baris C++).
- **Audio**: output pindah dari `QAudioSink` ke SDL audio stream
  (PipeWire/PulseAudio/ALSA di Linux, WASAPI di Windows, CoreAudio di macOS).
  Jalur DSP tidak berubah: FFmpeg → resample 48 kHz float → ring buffer →
  preamp → 10 biquad peaking EQ → volume/balance → FFT 2048 (31 band) + waveform.
- **FFmpeg static slim**: hanya demuxer/decoder audio (MP3, AAC, FLAC, Vorbis,
  Opus, WAV, WMA, ALAC, AIFF). Codec video, x265/codec2/aom/rav1e, librsvg dan
  ICU tidak ikut lagi.
- **Window**: frameless + transparan lewat `SDL_SetWindowHitTest` — jalan di
  Wayland (`xdg_toplevel.move`), Windows (`WM_NCHITTEST`) dan macOS
  (`setMovableByWindowBackground`).
- **HiDPI**: canvas dirender di resolusi fisik, jadi teks tetap tajam di skala
  pecahan (1.25×/1.5×).
- **Menu, help, tooltip** memakai font UI sistem (stb_truetype, tanpa membundel
  font); skin tetap memakai bitmap font 5×7.
- **File**: dialog lewat XDG desktop portal (fallback `zenity`), drag & drop
  lewat `SDL_EVENT_DROP_FILE`.
- **State** disimpan di `~/.config/linamp/state.json` (sebelumnya QSettings
  `linamp.conf`). Playlist lama tidak dimigrasi.
- Demo track di-generate saat pertama dipakai ke `~/.cache/linamp/demos/`
  (tidak ada file audio yang dibundel).

### Ditambah
- **Double size** (`Ctrl+D`, atau menu) — skin 2×, seperti Winamp 2.x.
- **Navigasi playlist dengan keyboard**: `↑`/`↓` (plus `Shift` untuk seleksi
  rentang), `PgUp`/`PgDn`, `Home`/`End`, `Enter` untuk memutar track terpilih.
  Fokus playlist mengikuti pointer / panel yang terakhir diklik.
- Skrip build rilis: `packaging/build-sdl3.sh`, `packaging/build-ffmpeg.sh`.
- CI: AppImage Linux ±3,5 MB, zip Windows ±2 MB, dmg macOS, plus smoke test
  render offscreen di ketiga platform.
- Dev helper: `LINAMP_SCREENSHOT`, `LINAMP_SCENE`, `LINAMP_TEST`,
  `LINAMP_PIXEL_SCALE`.

### Catatan upgrade
- Playlist, volume, EQ, dan layout panel mulai dari kosong (format state baru).
- Window rule Hyprland tidak berubah: `class`/app id tetap `linamp`.

## 1.0.0

Rilis pertama: pemutar gaya Winamp 2.x dengan Qt 6 / Qt Quick + FFmpeg,
EQ 10 band, spectrum analyzer, tiga panel (main / equalizer / playlist).
