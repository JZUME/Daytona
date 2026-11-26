// SPDX-License-Identifier: GPL-3.0-or-later

// Presses or releases the left mouse button through a virtual pointer and
// leaves it that way, for smoke tests of what is drawn while it is held
// (wlrctl can only click):
//     daytona-smoke-button press|release

#include "wayland-wlr-virtual-pointer-unstable-v1-client-protocol.h"

#include <cstdio>
#include <cstring>
#include <linux/input-event-codes.h>
#include <wayland-client.h>

namespace {

zwlr_virtual_pointer_manager_v1 *manager = nullptr;
wl_seat *seat = nullptr;

void global(void *, wl_registry *registry, uint32_t name, const char *interface, uint32_t)
{
    if (std::strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0)
        manager = static_cast<zwlr_virtual_pointer_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1));
    else if (std::strcmp(interface, wl_seat_interface.name) == 0 && !seat)
        seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
}

void globalRemove(void *, wl_registry *, uint32_t) { }

const wl_registry_listener registryListener = { global, globalRemove };

} // namespace

int main(int argc, char *argv[])
{
    const bool press = argc == 2 && std::strcmp(argv[1], "press") == 0;
    if (argc != 2 || (!press && std::strcmp(argv[1], "release") != 0)) {
        std::fprintf(stderr, "usage: %s press|release\n", argv[0]);
        return 2;
    }
    wl_display *display = wl_display_connect(nullptr);
    if (!display) {
        std::fprintf(stderr, "cannot connect to the Wayland display\n");
        return 1;
    }
    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, nullptr);
    wl_display_roundtrip(display);
    if (!manager || !seat) {
        std::fprintf(stderr, "the compositor has no virtual pointer support\n");
        return 1;
    }
    zwlr_virtual_pointer_v1 *pointer
        = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, seat);
    zwlr_virtual_pointer_v1_button(pointer, 0, BTN_LEFT,
        press ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
    zwlr_virtual_pointer_v1_frame(pointer);
    wl_display_roundtrip(display);
    wl_display_disconnect(display);
    return 0;
}
