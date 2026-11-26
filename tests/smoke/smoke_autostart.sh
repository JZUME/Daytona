#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# XDG autostart entries start after the shell, through dex, and their tray
# icons appear. Run with DAYTONA_SMOKE_AUTOSTART=autostart (see
# run-session.sh): of the entries there, only autostart/config/autostart/
# tray.desktop (user) and autostart/xdg/autostart/system.desktop (system)
# should start, each showing daytona-sni-test-item's tray icon. The others
# are hidden (overriding a system entry of the same name), not shown in
# Daytona, need a program that is not installed, or fail to start.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

# The test item's icon is a 16x16 red square with an 8x8 white centre:
# 192 red pixels each.
scale="${DAYTONA_SCALE:-1}"
icon=$((192 * scale * scale))

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

wait_for_red $((2 * icon)) 30

# No further icons turn up from the entries that should not start.
sleep 3
wait_for_red $((2 * icon)) 0

# The broken entry did not take the shell with it: the taskbar is still up.
height=${DAYTONA_SCREENSHOT_SIZE#*x}
wait_for_pixels 5 \
    512 $((height - 28)) '#c0c0c0' \
    512 $((height - 27)) '#ffffff'
