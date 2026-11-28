#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A Windows 98 style .theme (GradientActiveTitle differs from ActiveTitle)
# gives title bars a left-to-right blend, drawn by Daytona's labwc.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

daytona-smoke-window &
window=$!
trap 'kill $window 2>/dev/null || true' EXIT

# NT: the maximised window's title bar is solid navy from end to end.
wait_for_pixels 15 512 400 '#ff00ff' 2 1 '#000080' 960 1 '#000080'

theme="$XDG_RUNTIME_DIR/Windows 98.theme"
printf '[Control Panel\\Colors]\r\nActiveTitle=0 0 128\r\nGradientActiveTitle=16 132 208\r\n' \
    >"$theme"
daytona-themetool apply "$theme" >/dev/null

# Navy on the left, light blue on the right, a blend between.
wait_for_pixels 15 0 1 '#000080' 1023 1 '#1084d0'
daytona-pixelcheck --gradient "$SCREENSHOT" "${DAYTONA_SCALE:-1}" 1 '#000080' '#1084d0'
