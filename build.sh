#!/usr/bin/env bash
# Local development build (Makefile). CI uses CMake + static FFmpeg/SDL3.
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
make -C "$DIR" -j"$(nproc)"
