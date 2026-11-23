#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# labwc's autostart starts daytona-shell; its taskbar runs along the bottom
# edge and maximised windows stop above it.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

height=${DAYTONA_SCREENSHOT_SIZE#*x}
top=$((height - 28)) # first row of the 28 pixel taskbar

# Taskbar: NT raised top edge (button light, which is the face colour in
# Windows Standard, then highlight) over the face,
# the raised Start button 2 pixels in from the left, 4 pixels down, and the
# notification area's 1 pixel sunken edge at the right.
wait_for_pixels 15 \
    512 $top '#c0c0c0' \
    512 $((top + 1)) '#ffffff' \
    512 $((top + 14)) '#c0c0c0' \
    2 $((top + 4)) '#ffffff' \
    3 $((top + 5)) '#c0c0c0' \
    2 $((top + 25)) '#000000' \
    1010 $((top + 4)) '#808080' \
    1021 $((top + 14)) '#ffffff'

# Desktop: Windows Standard's desktop colour fills the screen behind windows.
wait_for_pixels 15 \
    0 0 '#008080' \
    1023 400 '#008080'

daytona-smoke-window &
window=$!
trap 'kill $window 2>/dev/null || true' EXIT

# The maximised window's client area ends right above the taskbar.
wait_for_pixels 15 \
    512 400 '#ff00ff' \
    512 $((top - 1)) '#ff00ff' \
    512 $top '#c0c0c0'
