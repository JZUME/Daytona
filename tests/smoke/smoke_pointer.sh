#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The mouse pointer is NT's size at every output scale: Daytona's cursor
# theme has an image for each scale. (Chicago95's own has only one size, so
# at scale 2 its pointer came out half as big.)
set -euo pipefail
source "$(dirname "$0")/lib.sh"

wait_for_pixels 15 512 754 '#c0c0c0'
# Over the desktop, which shows the default pointer.
wlrctl pointer move -5000 -5000
wlrctl pointer move 299 300
sleep 0.2
wlrctl pointer move 1 0
end=$((SECONDS + 10))
while true; do
    grim "$XDG_RUNTIME_DIR/pointer.png"
    if daytona-pixelcheck --pointer "$XDG_RUNTIME_DIR/pointer.png" 300 300 "$DAYTONA_SCALE"; then
        exit 0
    fi
    ((SECONDS < end)) || exit 1
    sleep 0.3
done
