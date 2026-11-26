#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Print Screen copies the whole screen to the clipboard as a PNG at device
# resolution, as NT did; Alt+Print Screen (daytona-shell --print-window)
# copies the active window's screen. With a file name the image is saved
# there instead.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

width=${DAYTONA_SCREENSHOT_SIZE%x*}
height=${DAYTONA_SCREENSHOT_SIZE#*x}
taskbar=$((height - 14))
scale="${DAYTONA_SCALE:-1}"

# png_size FILE: the image's width and height, from its IHDR chunk.
png_size() {
    local w h
    w=$(od -An -tu1 -j16 -N4 "$1" | awk '{print $1*16777216 + $2*65536 + $3*256 + $4}')
    h=$(od -An -tu1 -j20 -N4 "$1" | awk '{print $1*16777216 + $2*65536 + $3*256 + $4}')
    echo "${w}x${h}"
}

# check_capture FILE X Y #RRGGBB: a PNG of the whole screen at device
# resolution, with the taskbar and that logical pixel.
check_capture() {
    local size
    size="$(png_size "$1")"
    if [[ "$size" != "$((width * scale))x$((height * scale))" ]]; then
        echo "$1 is $size, not the screen's device size" >&2
        exit 1
    fi
    daytona-pixelcheck "$1" $((512 * scale)) $((taskbar * scale)) '#c0c0c0' \
        $(($2 * scale)) $(($3 * scale)) "$4"
}

wait_for_pixels 15 512 $taskbar '#c0c0c0' 5 5 '#008080'

# To a file, relative to the current directory.
cd "$XDG_RUNTIME_DIR"
daytona-shell --print-screen screen.png
check_capture "$XDG_RUNTIME_DIR/screen.png" 5 5 '#008080'

# To the clipboard.
daytona-shell --print-screen
wl-paste --type image/png >"$XDG_RUNTIME_DIR/clipboard.png"
check_capture "$XDG_RUNTIME_DIR/clipboard.png" 5 5 '#008080'

# Alt+Print Screen with a window active: its screen (the only one here).
daytona-smoke-window &
window=$!
trap 'kill $window 2>/dev/null || true' EXIT
wait_for_pixels 15 512 400 '#ff00ff'
daytona-shell --print-window
wl-paste --type image/png >"$XDG_RUNTIME_DIR/window.png"
check_capture "$XDG_RUNTIME_DIR/window.png" 512 400 '#ff00ff'
