#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$DIR/build.sh"
chmod +x "$DIR/run.sh" "$DIR/build.sh"

APPS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/512x512/apps"
mkdir -p "$APPS" "$ICONS"
cp "$DIR/assets/icon.png" "$ICONS/linamp.png"
rm -f "$APPS/winamp-classic.desktop" "$ICONS/winamp-classic.png"

cat > "$APPS/linamp.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Linamp
Comment=Native Winamp 2.x style music player (Linamp)
Exec="$DIR/run.sh"
Icon=linamp
Terminal=false
Categories=AudioVideo;Audio;Player;
Keywords=music;audio;player;linamp;winamp;
StartupWMClass=linamp
StartupNotify=true
DESKTOP

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$APPS" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -q "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" >/dev/null 2>&1 || true
fi

echo "Launcher terpasang: $APPS/linamp.desktop"
echo "Jalankan dari app launcher, atau: $DIR/run.sh"
