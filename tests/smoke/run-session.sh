#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs a command inside a headless Daytona labwc session and returns its exit
# status. Used by the smoke tests.
#
# Usage: run-session.sh PREFIX TIMEOUT_SECONDS COMMAND [ARGS...]
#   PREFIX is a Daytona install prefix (the tests install into the build tree).
#
# The command runs with WAYLAND_DISPLAY, XDG_RUNTIME_DIR,
# DAYTONA_SCREENSHOT_SIZE (logical) and DAYTONA_SCALE set. Everything started
# here is stopped on exit.
#
# DAYTONA_SMOKE_SCALE (default 1) runs the output at that whole-number scale:
# the logical size stays 1024x768, so tests use the same coordinates, and
# screenshots are that many times larger.
#
# The session has its own empty configuration directory, and its own empty
# Desktop folder ($XDG_RUNTIME_DIR/Desktop). DAYTONA_SMOKE_CONFIG
# names a daytona.ini to start with instead; the output is then left as labwc
# created it (no size or scale is set), as the test is about what the shell
# does with that configuration.
#
# DAYTONA_SMOKE_OUTPUTS (default 1) is how many headless outputs the session
# has (HEADLESS-1, HEADLESS-2...).
#
# DAYTONA_SMOKE_ACTIVATION=1 has labwc update the D-Bus activation
# environment, as in a real session, with the theme variables daytona-session
# sets. They are set only after the bus has started, so activated programs
# get them only through that update.
#
# DAYTONA_SMOKE_AUTOSTART names a directory of XDG autostart entries for the
# session: its config/autostart is copied to the user's autostart directory,
# and its xdg directory is the only system configuration directory
# (XDG_CONFIG_DIRS), so its xdg/autostart entries are the only system ones.
set -euo pipefail

# The shell uses the D-Bus session bus; give the session its own bus.
if [[ -z "${DAYTONA_SMOKE_DBUS:-}" ]] && command -v dbus-run-session >/dev/null; then
    export DAYTONA_SMOKE_DBUS=1
    exec dbus-run-session -- "$0" "$@"
fi

prefix="$1"
limit="$2"
shift 2

width=1024
height=768
scale="${DAYTONA_SMOKE_SCALE:-1}"
export DAYTONA_SCALE="$scale"

runtime="$(mktemp -d)"
chmod 700 "$runtime"
export XDG_RUNTIME_DIR="$runtime"
export XDG_CONFIG_HOME="$runtime/config"
mkdir -p "$XDG_CONFIG_HOME/daytona"
# The desktop icons' Desktop folder, apart from the user's own.
mkdir -p "$runtime/Desktop"
echo "XDG_DESKTOP_DIR=\"$runtime/Desktop\"" >"$XDG_CONFIG_HOME/user-dirs.dirs"
if [[ -n "${DAYTONA_SMOKE_CONFIG:-}" ]]; then
    cp "$DAYTONA_SMOKE_CONFIG" "$XDG_CONFIG_HOME/daytona/daytona.ini"
fi
if [[ -n "${DAYTONA_SMOKE_AUTOSTART:-}" ]]; then
    cp -r "$DAYTONA_SMOKE_AUTOSTART/config/autostart" "$XDG_CONFIG_HOME/"
    export XDG_CONFIG_DIRS="$DAYTONA_SMOKE_AUTOSTART/xdg"
fi
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_LIBINPUT_NO_DEVICES=1
export WLR_HEADLESS_OUTPUTS="${DAYTONA_SMOKE_OUTPUTS:-1}"
export XDG_DATA_DIRS="$prefix/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export PATH="$prefix/bin:$PATH"
export QT_QPA_PLATFORM=wayland
# Daytona's cursors at the standard size, as daytona-session sets them.
export XCURSOR_THEME="Daytona Cursors"
export XCURSOR_SIZE=32
export XCURSOR_PATH="$prefix/share/icons:/usr/share/icons"
export DAYTONA_SCREENSHOT_SIZE="${width}x${height}"
if [[ "${DAYTONA_SMOKE_ACTIVATION:-}" == 1 ]]; then
    export LABWC_UPDATE_ACTIVATION_ENV=true
    export GTK_THEME=Daytona QT_STYLE_OVERRIDE=daytona XDG_SESSION_DESKTOP=Daytona
fi

labwc_pid=""
cleanup() {
    if [[ -n "$labwc_pid" ]]; then
        kill "$labwc_pid" 2>/dev/null || true
        wait "$labwc_pid" 2>/dev/null || true
    fi
    rm -rf "$runtime"
}
trap cleanup EXIT

# Daytona's patched labwc when the build includes it, as daytona-session does.
labwc="$prefix/libexec/daytona/labwc"
[[ -x "$labwc" ]] || labwc=labwc
"$labwc" --config-dir "$prefix/share/daytona/labwc" >"$runtime/labwc.log" 2>&1 &
labwc_pid=$!

for _ in $(seq 100); do
    [[ -S "$runtime/wayland-0" ]] && break
    if ! kill -0 "$labwc_pid" 2>/dev/null; then
        echo "labwc exited during startup:" >&2
        cat "$runtime/labwc.log" >&2
        exit 1
    fi
    sleep 0.1
done
if [[ ! -S "$runtime/wayland-0" ]]; then
    echo "labwc did not create a Wayland socket" >&2
    exit 1
fi
export WAYLAND_DISPLAY=wayland-0

if [[ -z "${DAYTONA_SMOKE_CONFIG:-}" ]]; then
    wlr-randr --output HEADLESS-1 --custom-mode "$((width * scale))x$((height * scale))" \
        --scale "$scale" >/dev/null
fi

status=0
timeout --kill-after=5 "$limit" "$@" || status=$?
if [[ $status -eq 124 ]]; then
    echo "smoke test timed out after ${limit}s" >&2
fi
if [[ $status -ne 0 ]]; then
    echo "--- labwc log ---" >&2
    tail -n 50 "$runtime/labwc.log" >&2
fi
exit $status
