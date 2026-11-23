#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Two windows give two taskbar buttons after the Start button: the older one
# raised, the active (newest) one latched down.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

height=${DAYTONA_SCREENSHOT_SIZE#*x}
top=$((height - 28))

daytona-smoke-window &
first=$!
trap 'kill $first $second 2>/dev/null || true' EXIT
second=""
# Wait for the first window's button before opening the second, so their
# order is certain. Buttons start about 62px in and are 160px wide.
wait_for_pixels 15 150 $((top + 4)) '#000000'
daytona-smoke-window &
second=$!

# Top edge of each button: white when raised, black when pressed.
wait_for_pixels 15 \
    150 $((top + 4)) '#ffffff' \
    300 $((top + 4)) '#000000'
