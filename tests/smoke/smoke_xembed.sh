#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Old-style X11 tray icons (the XEmbed system tray) show in the notification
# area through daytona-xembed-proxy, which the session starts when Xwayland
# is installed, and clicks on them reach the program: a left click, a right
# click and a double click on daytona-smoke-xembed-icon's red icon.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

if ! command -v Xwayland >/dev/null; then
    echo "Xwayland is not installed" >&2
    exit 77
fi

# This session's proxy (other sessions' may be running), and its display.
proxy=""
end=$((SECONDS + 15))
while [[ -z "$proxy" ]]; do
    for pid in $(pgrep -f libexec/daytona/daytona-xembed-proxy || true); do
        if tr '\0' '\n' <"/proc/$pid/environ" 2>/dev/null \
            | grep -qxF "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"; then
            proxy=$pid
        fi
    done
    [[ -n "$proxy" ]] && break
    if ((SECONDS >= end)); then
        echo "the session did not start daytona-xembed-proxy" >&2
        exit 1
    fi
    sleep 0.2
done
DISPLAY="$(tr '\0' '\n' <"/proc/$proxy/environ" | sed -n 's/^DISPLAY=//p')"
export DISPLAY

log="$XDG_RUNTIME_DIR/xembed-icon.log"
daytona-smoke-xembed-icon >"$log" 2>&1 &
icon=$!
trap 'kill $icon 2>/dev/null || true' EXIT

# The icon: 16x16 red pixels, enlarged pixel for pixel at higher scales.
scale="${DAYTONA_SCALE:-1}"
end=$((SECONDS + 15))
while true; do
    grim "$XDG_RUNTIME_DIR/screenshot.png"
    if daytona-pixelcheck --count "$XDG_RUNTIME_DIR/screenshot.png" '#ff0000' \
        $((256 * scale * scale)) 2>"$XDG_RUNTIME_DIR/pixelcheck.err"; then
        break
    fi
    if ((SECONDS >= end)); then
        cat "$XDG_RUNTIME_DIR/pixelcheck.err" "$log" >&2
        exit 1
    fi
    sleep 0.2
done
read -r x y < <(daytona-pixelcheck --find "$XDG_RUNTIME_DIR/screenshot.png" '#ff0000' "$scale")

# Click it with a mouse that stays plugged in throughout, as a real one does.
{
    echo "move $x $y"
    echo "sleep 300"
    echo "click left"
    echo "sleep 600"
    echo "click right"
    echo "sleep 600"
    echo "click left"
    echo "sleep 100"
    echo "click left"
    echo "sleep 600"
} | daytona-smoke-pointer "${DAYTONA_SCREENSHOT_SIZE%x*}" "${DAYTONA_SCREENSHOT_SIZE#*x}"

mapfile -t presses < <(grep '^press ' "$log" || true)
buttons="$(printf '%s\n' "${presses[@]}" | cut -d' ' -f2 | tr '\n' ' ')"
if [[ "$buttons" != "1 3 1 1 " ]]; then
    echo "expected presses of buttons 1 3 1 1, got: $buttons" >&2
    cat "$log" >&2
    exit 1
fi
# The double click's presses are close together in the X server's time, so
# programs see a double click.
first="$(cut -d' ' -f3 <<<"${presses[2]}")"
second="$(cut -d' ' -f3 <<<"${presses[3]}")"
if ((second - first > 400)); then
    echo "the double click's presses are $((second - first)) ms apart" >&2
    exit 1
fi
