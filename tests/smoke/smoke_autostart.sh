#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# XDG autostart entries start after the shell, through dex, and their tray
# icons appear. Run with DAYTONA_SMOKE_AUTOSTART=autostart (see
# run-session.sh): of the entries there, only autostart/config/autostart/
# tray.desktop (user) and autostart/xdg/autostart/system.desktop (system)
# should start, each showing daytona-sni-test-item's tray icon. The others
# are hidden (overriding a system entry of the same name), not shown in
# Daytona, need a program that is not installed, or fail to start.
#
# The network icon: autostart/bin has stand-ins for nm-applet and nm-tray,
# each showing the same icon, and the system entries start both. Daytona's
# own entries replace them, so only one starts: nm-applet, once, with
# --indicator (its tooltip says how it was started).
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

wait_for_red $((3 * icon)) 30

# No further icons turn up from the entries that should not start.
sleep 3
wait_for_red $((3 * icon)) 0

# The network icon is nm-applet's, started with --indicator.
items=$(gdbus call --session --dest org.kde.StatusNotifierWatcher \
    --object-path /StatusNotifierWatcher \
    --method org.freedesktop.DBus.Properties.Get \
    org.kde.StatusNotifierWatcher RegisteredStatusNotifierItems)
found=0
for item in $(grep -o "'[^']*'" <<<"$items" | tr -d "'"); do
    tip=$(gdbus call --session --dest "${item%%/*}" --object-path "/${item#*/}" \
        --method org.freedesktop.DBus.Properties.Get org.kde.StatusNotifierItem ToolTip)
    if [[ "$tip" == *"nm-applet --indicator"* ]]; then
        found=$((found + 1))
    fi
done
if ((found != 1)); then
    echo "expected one nm-applet --indicator icon, found $found: $items" >&2
    exit 1
fi

# The broken entry did not take the shell with it: the taskbar is still up.
height=${DAYTONA_SCREENSHOT_SIZE#*x}
wait_for_pixels 5 \
    512 $((height - 28)) '#c0c0c0' \
    512 $((height - 27)) '#ffffff'
