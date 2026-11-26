#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# The shell runs swayidle with the screen saver's wait; the screen saver
# (Blank Screen) covers every screen in black and ends when the pointer
# moves; Lock Workstation covers the screen with swaylock in the desktop
# colour.
set -euo pipefail
source "$(dirname "$0")/lib.sh"

height=${DAYTONA_SCREENSHOT_SIZE#*x}
taskbar=$((height - 14))

wait_for_pixels 15 512 $taskbar '#c0c0c0' 512 300 '#008080'

# swayidle, started by the shell, with the default 15 minute wait.
swayidle_arguments() {
    local proc line
    for proc in /proc/[0-9]*; do
        [[ "$(cat "$proc/comm" 2>/dev/null)" == swayidle ]] || continue
        # (An exited one left unreaped has an empty command line.)
        line="$(tr '\0' ' ' <"$proc/cmdline" 2>/dev/null)"
        if [[ -n "$line" ]]; then
            echo "$line"
            return 0
        fi
    done
    return 1
}
end=$((SECONDS + 10))
until arguments="$(swayidle_arguments)"; do
    if ((SECONDS >= end)); then
        echo "swayidle is not running" >&2
        exit 1
    fi
    sleep 0.2
done
if [[ "$arguments" != *"timeout 900 "*"--screen-saver before-sleep "*"--lock lock "*"--lock"* ]]; then
    echo "unexpected swayidle arguments: $arguments" >&2
    exit 1
fi

# swayidle's timeout: the whole screen goes black, taskbar included.
daytona-shell --screen-saver
wait_for_pixels 10 512 $taskbar '#000000' 512 300 '#000000' 0 0 '#000000'

# Moving the pointer ends it. (The first movement after it starts may only
# be noted as where the pointer is.)
end=$((SECONDS + 10))
until wlrctl pointer move 40 0 && sleep 0.3 && grim "$XDG_RUNTIME_DIR/s.png" \
    && daytona-pixelcheck "$XDG_RUNTIME_DIR/s.png" \
        $((512 * DAYTONA_SCALE)) $((taskbar * DAYTONA_SCALE)) '#c0c0c0' 2>/dev/null; do
    if ((SECONDS >= end)); then
        echo "the screen saver did not end when the pointer moved" >&2
        exit 1
    fi
done

# Lock Workstation returns once the screen is locked: swaylock covers the
# taskbar in the desktop colour.
daytona-shell --lock
wait_for_pixels 10 512 $taskbar '#008080' 5 5 '#008080'
