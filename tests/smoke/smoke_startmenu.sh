#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The Start menu key (daytona-shell --start-menu over D-Bus) opens the Start
# menu right above the taskbar, and pressing it again closes it. The Start
# button opens it as soon as the mouse button goes down, as NT's did, and a
# click outside the menu closes it.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

height=${DAYTONA_SCREENSHOT_SIZE#*x}
top=$((height - 28))

# Wait for the taskbar, so the shell is running.
wait_for_pixels 15 512 $((top + 14)) '#c0c0c0'

# The menu's banner is a dark strip inside its frame, just above the
# taskbar at the left: x 4..24 (2px margin, 2px frame).
daytona-shell --start-menu
wait_for_pixels 15 10 $((top - 20)) '#808080'

# Closed again: the desktop shows through.
daytona-shell --start-menu
wait_for_pixels 15 10 $((top - 20)) '#008080'

# The Start button: the menu is open while the mouse button is still down.
# (Past the moment after closing in which a click on it must not reopen it.)
trap 'daytona-smoke-button release 2>/dev/null || true' EXIT
sleep 0.5
wlrctl pointer move -5000 -5000
wlrctl pointer move 19 $((top + 14))
sleep 0.1
wlrctl pointer move 1 0
daytona-smoke-button press
wait_for_pixels 15 10 $((top - 20)) '#808080'
daytona-smoke-button release
# Still open after the release.
sleep 0.3
wait_for_pixels 5 10 $((top - 20)) '#808080'

# A click on the desktop closes it.
wlrctl pointer move 400 -300
wlrctl pointer click left
wait_for_pixels 15 10 $((top - 20)) '#008080'
