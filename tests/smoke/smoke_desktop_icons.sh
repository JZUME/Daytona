#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A file put in the Desktop folder appears as an icon in NT's grid (after My
# Computer, Network Neighborhood and Recycle Bin), and goes when it is
# removed. A .desktop file shows its own icon: here a red square.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

scale="${DAYTONA_SCALE:-1}"
height=${DAYTONA_SCREENSHOT_SIZE#*x}
icon=$((32 * 32 * scale * scale))

# wait_for_red COUNT SECONDS: screenshot until exactly COUNT red pixels show.
wait_for_red() {
    local end=$((SECONDS + $2))
    while true; do
        grim "$XDG_RUNTIME_DIR/screenshot.png"
        if daytona-pixelcheck --count "$XDG_RUNTIME_DIR/screenshot.png" '#ff0000' "$1" \
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

# Wait for the taskbar, so the shell is running.
wait_for_pixels 15 512 $((height - 14)) '#c0c0c0'

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

# The fourth place down the first column: cells are 75 pixels, and the
# 32-pixel icon is centred, 2 pixels from the top of its cell.
wait_for_red "$icon" 15
wait_for_pixels 5 37 $((3 * 75 + 18)) '#ff0000'

rm "$XDG_RUNTIME_DIR/Desktop/red.desktop"
wait_for_red 0 15
