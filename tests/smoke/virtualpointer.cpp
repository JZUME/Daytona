// SPDX-License-Identifier: GPL-3.0-or-later

// A mouse that stays plugged in, for smoke tests that need the seat to keep
// a pointer between clicks (Xwayland only delivers input to X11 programs
// while the seat has one; wlrctl's pointer goes away after each command).
// Reads commands from standard input until it ends:
//     move X Y          (logical pixels, on a WIDTHxHEIGHT layout)
//     click left|right|middle
//     sleep MILLISECONDS
//     daytona-smoke-pointer WIDTH HEIGHT

#include "wayland-wlr-virtual-pointer-unstable-v1-client-protocol.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <initializer_list>
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

uint32_t now()
{
    timespec ts {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint32_t(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

} // namespace

int main(int argc, char *argv[])
{
    unsigned width = 0;
    unsigned height = 0;
    if (argc != 3 || std::sscanf(argv[1], "%u", &width) != 1
        || std::sscanf(argv[2], "%u", &height) != 1) {
        std::fprintf(stderr, "usage: %s WIDTH HEIGHT < commands\n", argv[0]);
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
    wl_display_roundtrip(display);

    char line[128];
    while (std::fgets(line, sizeof line, stdin)) {
        unsigned x = 0;
        unsigned y = 0;
        unsigned ms = 0;
        char name[16] = {};
        if (std::sscanf(line, "move %u %u", &x, &y) == 2) {
            zwlr_virtual_pointer_v1_motion_absolute(pointer, now(), x, y, width, height);
            zwlr_virtual_pointer_v1_frame(pointer);
        } else if (std::sscanf(line, "click %15s", name) == 1) {
            const uint32_t button = std::strcmp(name, "right") == 0 ? BTN_RIGHT
                : std::strcmp(name, "middle") == 0                  ? BTN_MIDDLE
                                                                    : BTN_LEFT;
            for (const auto state :
                { WL_POINTER_BUTTON_STATE_PRESSED, WL_POINTER_BUTTON_STATE_RELEASED }) {
                zwlr_virtual_pointer_v1_button(pointer, now(), button, state);
                zwlr_virtual_pointer_v1_frame(pointer);
            }
        } else if (std::sscanf(line, "sleep %u", &ms) == 1) {
            wl_display_roundtrip(display);
            timespec pause { time_t(ms / 1000), long(ms % 1000) * 1000000 };
            nanosleep(&pause, nullptr);
        }
        wl_display_roundtrip(display);
    }
    zwlr_virtual_pointer_v1_destroy(pointer);
    wl_display_roundtrip(display);
    wl_display_disconnect(display);
    return 0;
}
