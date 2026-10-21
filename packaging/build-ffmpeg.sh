#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Build a minimal *static* FFmpeg (audio playback only) for Linamp.
#
#   packaging/build-ffmpeg.sh <install-prefix>
#
# Only the demuxers/decoders Linamp needs are compiled in, which keeps the
# resulting binary small (no x265/codec2/aom/rav1e/ICU/librsvg chain).
# Works on Linux, macOS and MSYS2/MinGW.
# ---------------------------------------------------------------------------
set -euo pipefail

PREFIX="${1:?usage: build-ffmpeg.sh <install-prefix>}"
VERSION="${FFMPEG_VERSION:-7.1.1}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
WORK="${FFMPEG_BUILD_DIR:-$PWD/.ffmpeg-build}"

mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

TARBALL="ffmpeg-$VERSION.tar.xz"
if [ ! -f "$TARBALL" ]; then
    URL="https://ffmpeg.org/releases/$TARBALL"
    echo "==> downloading $URL"
    if command -v curl >/dev/null 2>&1; then
        curl -fL -o "$TARBALL" "$URL"
    else
        wget -O "$TARBALL" "$URL"
    fi
fi

SRC="ffmpeg-$VERSION"
if [ ! -d "$SRC" ]; then
    tar xf "$TARBALL"
fi
cd "$SRC"

DECODERS="mp3,mp3float,mp2,aac,aac_latm,flac,vorbis,opus,alac,wavpack,wmalossless,wmapro,wmav1,wmav2,pcm_u8,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s24be,pcm_s32le,pcm_s32be,pcm_f32le,pcm_f32be,pcm_alaw,pcm_mulaw"
DEMUXERS="mp3,mp2,aac,flac,ogg,wav,w64,mov,matroska,avi,asf,aiff,aac"
PARSERS="mpegaudio,aac,flac,vorbis,opus"
PROTOCOLS="file,pipe"

echo "==> configuring (static, audio only)"
./configure \
    --prefix="$PREFIX" \
    --pkgconfigdir="$PREFIX/lib/pkgconfig" \
    --enable-static \
    --disable-shared \
    --disable-everything \
    --disable-programs \
    --disable-doc \
    --disable-network \
    --disable-autodetect \
    --disable-asm \
    --enable-avcodec \
    --enable-avformat \
    --enable-avutil \
    --enable-swresample \
    --enable-decoder="$DECODERS" \
    --enable-demuxer="$DEMUXERS" \
    --enable-parser="$PARSERS" \
    --enable-protocol="$PROTOCOLS"

echo "==> building with $JOBS jobs"
make -j"$JOBS"
make install

echo "==> done: $PREFIX"
