#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The Start menu key (daytona-shell --start-menu over D-Bus) opens the Start
# menu right above the taskbar, and pressing it again closes it.
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
