#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Starts a headless labwc session for development, viewable in a browser:
#   labwc (headless, software rendering) -> wayvnc (localhost:5900)
#   -> websockify/noVNC (localhost:6080)
#
# Stop with Ctrl+C. Set DAYTONA_OUTPUT_SIZE (default 1024x768) to change the
# virtual screen size.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
size="${DAYTONA_OUTPUT_SIZE:-1024x768}"
# DAYTONA_OUTPUT_SCALE (default 1, may be fractional such as 1.5) sets the
# output scale; the size above is in device pixels.
scale="${DAYTONA_OUTPUT_SCALE:-1}"
runtime="${DAYTONA_RUNTIME_DIR:-/tmp/daytona-dev-$(id -u)}"
novnc_dir="/usr/share/novnc"

# A fixed, private runtime directory lets scripts/screenshot.sh find the
# session without extra arguments.
mkdir -p "$runtime"
chmod 700 "$runtime"
export XDG_RUNTIME_DIR="$runtime"

export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1
# DAYTONA_OUTPUTS (default 1) headless outputs, side by side.
export WLR_HEADLESS_OUTPUTS="${DAYTONA_OUTPUTS:-1}"
# As daytona-session sets them.
export GTK_THEME=Daytona
export QT_STYLE_OVERRIDE=daytona
export XCURSOR_THEME="Chicago95 Standard Cursors"

pids=()
cleanup() {
    trap - EXIT INT TERM
    for pid in "${pids[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    wait 2>/dev/null || true
    rm -f "$runtime/session.env"
}
trap cleanup EXIT INT TERM

labwc_args=()
if [[ -f "$repo/labwc/rc.xml" ]]; then
    labwc_args+=(-C "$repo/labwc")
    echo "Using Daytona labwc configuration from $repo/labwc"
else
    echo "Daytona labwc configuration not found; using labwc defaults"
fi

# Prefer a freshly built shell over an installed one.
for dir in "$repo/build/shell" "$repo/build-trixie/shell"; do
    if [[ -x "$dir/daytona-shell" ]]; then
        export PATH="$dir:$PATH"
        echo "Using daytona-shell from $dir"
        break
    fi
done

before=$(ls "$runtime" 2>/dev/null | grep -E '^wayland-[0-9]+$' || true)
labwc "${labwc_args[@]}" >"$runtime/labwc.log" 2>&1 &
labwc_pid=$!
pids+=($labwc_pid)

display=""
for _ in $(seq 50); do
    for sock in $(ls "$runtime" | grep -E '^wayland-[0-9]+$' || true); do
        if ! grep -qx "$sock" <<<"$before"; then
            display="$sock"
        fi
    done
    [[ -n "$display" ]] && break
    sleep 0.1
done
if [[ -z "$display" ]]; then
    echo "labwc did not start; see $runtime/labwc.log" >&2
    exit 1
fi
export WAYLAND_DISPLAY="$display"

for output in $(wlr-randr | awk '/^[^ ]/ {print $1}'); do
    if ! wlr-randr --output "$output" --custom-mode "$size" --scale "$scale"; then
        echo "Could not set $output to size $size; keeping the default" >&2
    fi
done

wayvnc 127.0.0.1 5900 >"$runtime/wayvnc.log" 2>&1 &
pids+=($!)

websockify --web "$novnc_dir" 127.0.0.1:6080 127.0.0.1:5900 >"$runtime/websockify.log" 2>&1 &
pids+=($!)

cat >"$runtime/session.env" <<ENV
export XDG_RUNTIME_DIR="$runtime"
export WAYLAND_DISPLAY="$display"
export LABWC_PID="$labwc_pid"
export QT_QPA_PLATFORM=wayland
ENV

cat <<MSG

Daytona development session is running ($size).

  Desktop in a browser: open port 6080 (Codespaces: Ports tab), then vnc.html
                        e.g. http://localhost:6080/vnc.html
  Run applications in it from another terminal:
      source $runtime/session.env
      qterminal &
  Screenshot:           scripts/screenshot.sh [file.png]
  Logs:                 $runtime/*.log

Press Ctrl+C to stop.
MSG

wait -n "${pids[@]}"
echo "A session process exited; stopping. See $runtime/*.log" >&2
