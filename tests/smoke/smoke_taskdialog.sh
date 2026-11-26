#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A window with a dialog has one taskbar button, as in NT: the dialog has
# none of its own, and the owner's button is pressed while the dialog is
# active.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

height=${DAYTONA_SCREENSHOT_SIZE#*x}
top=$((height - 28))

daytona-smoke-window --normal --dialog &
window=$!
trap 'kill $window 2>/dev/null || true' EXIT

# The dialog is open and active (navy titlebar, centred over its owner);
# the first button (about 62px in, 160px wide) is pressed (black top edge)
# and there is no second button, only the taskbar face.
wait_for_pixels 15 \
    540 328 '#000080' \
    150 $((top + 4)) '#000000' \
    300 $((top + 4)) '#c0c0c0'
