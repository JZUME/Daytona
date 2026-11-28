#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A screen going from 100% to 200% at the same size in pixels (2048x1536)
# keeps the desktop's size in pixels too. Qt 6.8 then went on drawing into
# buffers made at 100% in every other frame, so the desktop icons flicked
# between their size and half of it. After the change every repaint shows the
# 32-pixel icon at 64x64 device pixels.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

wlr-randr --output HEADLESS-1 --custom-mode 2048x1536 --scale 1 >/dev/null
# The taskbar at 100%: its face near the bottom of the 1536-pixel screen.
DAYTONA_SCALE=1 wait_for_pixels 15 1024 1522 '#c0c0c0'
wlr-randr --output HEADLESS-1 --scale 2 >/dev/null
export DAYTONA_SCALE=2
wait_for_pixels 15 512 754 '#c0c0c0'

cat >"$XDG_RUNTIME_DIR/red.svg" <<SVG
<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32"><rect width="32" height="32" fill="#ff0000"/></svg>
SVG
cat >"$XDG_RUNTIME_DIR/Desktop/red.desktop" <<ENTRY
[Desktop Entry]
Type=Application
Name=Red
Icon=$XDG_RUNTIME_DIR/red.svg
Exec=true
ENTRY

full=$((64 * 64))
# check_red: the red icon is whole, at 200%.
check_red() {
    local end=$((SECONDS + $1))
    while true; do
        grim "$XDG_RUNTIME_DIR/screenshot.png"
        if daytona-pixelcheck --count "$XDG_RUNTIME_DIR/screenshot.png" '#ff0000' "$full" \
            2>"$XDG_RUNTIME_DIR/pixelcheck.err"; then
            return 0
        fi
        if ((SECONDS >= end)); then
            cat "$XDG_RUNTIME_DIR/pixelcheck.err" >&2
            return 1
        fi
        sleep 0.2
    done
}
check_red 15

# Each new file repaints the desktop; every frame must still be at 200%.
for i in 1 2 3 4 5 6; do
    touch "$XDG_RUNTIME_DIR/Desktop/file$i.txt"
    sleep 0.4
    check_red 0
done
