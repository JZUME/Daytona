#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Pointing at Start menu items opens their submenus, also after the pointer
# has been inside another submenu (Qt gives an open submenu every mouse
# event; the Start menu must take back the ones over itself). Drives a
# virtual pointer with wlrctl.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

height=${DAYTONA_SCREENSHOT_SIZE#*x}
top=$((height - 28))

# The menu's items, bottom taskbar, 32 pixel icons: 36 pixels each from
# y = top - 225. Their centres:
documents=$((top - 171))
settings=$((top - 135))
run=$((top - 63))

# Pointer moves are relative; start from the top-left corner.
pointer_to() {
    wlrctl pointer move -5000 -5000
    wlrctl pointer move "$(($1 - 1))" "$2"
    # Then move a pixel: a jump onto a surface only enters it.
    sleep 0.1
    wlrctl pointer move 1 0
}
# Move in small steps, as a real pointer does.
pointer_step() {
    local i
    for i in $(seq 10); do
        wlrctl pointer move $(($1 / 10)) $(($2 / 10))
        sleep 0.03
    done
}

wait_for_pixels 15 512 $((top + 14)) '#c0c0c0'
pointer_to 20 $((top + 14))
wlrctl pointer click left
# Open: the banner.
wait_for_pixels 15 10 $((top - 20)) '#808080'

# Settings opens its submenu; go into it, then back and up to Documents.
pointer_to 90 "$settings"
wait_for_pixels 5 130 $((settings + 12)) '#000080'
pointer_step 120 0
sleep 0.3
pointer_step -120 0
pointer_step 0 $((documents - settings))
# Documents is highlighted and Settings is not.
wait_for_pixels 5 130 $((documents + 12)) '#000080' 130 $((settings + 12)) '#c0c0c0'
# Its submenu ("(Empty)") opens beside it, past the menu's right edge (163):
# its background left of the text (whose pixels depend on the font).
wait_for_pixels 5 170 $((documents - 5)) '#c0c0c0'

# Into the Documents submenu and back, then a click on Run... runs it: the
# Start menu closes.
pointer_step 120 0
sleep 0.3
pointer_step -120 0
pointer_step 0 $((run - documents))
wlrctl pointer click left
wait_for_pixels 15 10 $((top - 20)) '#008080'
