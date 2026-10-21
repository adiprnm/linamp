#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QMAKE="${QMAKE:-qmake6}"
command -v "$QMAKE" >/dev/null 2>&1 || QMAKE=qmake
mkdir -p "$DIR/build"
cd "$DIR/build"
"$QMAKE" "$DIR/linamp.pro"
make -j"$(nproc)"
