# SPDX-License-Identifier: GPL-3.0-or-later
# Debian 13 development image for Daytona: build tools plus a headless labwc
# session viewable in a browser through wayvnc and noVNC.
FROM debian:trixie

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential ca-certificates clang-format cmake gdb git ninja-build \
        pkgconf qt6-base-dev liblayershellqtinterface-dev libwayland-dev dbus \
        libwayland-bin qt6-wayland-dev qt6-wayland-dev-tools \
        labwc wlr-randr grim wayvnc novnc python3-websockify \
        qterminal qt6-wayland qt6-svg-plugins fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*
