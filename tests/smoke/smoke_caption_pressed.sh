#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# A caption button looks pressed in (sunken) while the mouse button is held
# on it, as in NT: raised again when the pointer slides off, sunken when it
# comes back, and releasing over it runs the button (Close).
set -euo pipefail
source "$(dirname "$0")/lib.sh"

daytona-smoke-window &
window=$!
trap 'daytona-smoke-button release 2>/dev/null || true; kill $window 2>/dev/null || true' EXIT

# The maximised window's Close button spans (1008,2)-(1021,15). Its top-left
# pixel is the white of the raised edge, or the black of the sunken one.
wait_for_pixels 15 512 300 '#ff00ff' 1010 2 '#ffffff'

wlrctl pointer move -5000 -5000
wlrctl pointer move 1015 9
daytona-smoke-button press
wait_for_pixels 5 1010 2 '#000000' 1021 2 '#ffffff'

wlrctl pointer move -115 0
wait_for_pixels 5 1010 2 '#ffffff' 512 300 '#ff00ff'

wlrctl pointer move 115 0
wait_for_pixels 5 1010 2 '#000000'

daytona-smoke-button release
wait_for_pixels 5 512 300 '#008080'
