#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ ! -x "$DIR/build/linamp" ]; then
    "$DIR/build.sh"
fi
exec "$DIR/build/linamp" "$@"
