#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A maximised Qt window gets Daytona's active (navy) titlebar.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

daytona-smoke-window &
window=$!
trap 'kill $window 2>/dev/null || true' EXIT

# Client area is magenta; the titlebar strip at the top is navy. The title
# text and buttons are left and right, so the middle of the bar is plain.
wait_for_pixels 15 \
    512 400 '#ff00ff' \
    512 4 '#000080'
