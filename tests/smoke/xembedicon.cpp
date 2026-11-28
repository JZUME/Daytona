// SPDX-License-Identifier: GPL-3.0-or-later

// An X11 program with an old-style (XEmbed system tray) icon, for the
// smoke test of daytona-xembed-proxy: docks a red 16x16 icon and prints
// "press BUTTON TIME" for each mouse button pressed on it (TIME in the X
// server's milliseconds).
//     daytona-smoke-xembed-icon

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <xcb/xcb.h>

namespace {

xcb_atom_t intern(xcb_connection_t *c, const char *name)
{
    xcb_intern_atom_reply_t *reply
        = xcb_intern_atom_reply(c, xcb_intern_atom(c, false, std::strlen(name), name), nullptr);
    const xcb_atom_t atom = reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
    std::free(reply);
    return atom;
}

} // namespace

int main()
{
    int screenNumber = 0;
    xcb_connection_t *c = xcb_connect(nullptr, &screenNumber);
    if (xcb_connection_has_error(c)) {
        std::fprintf(stderr, "cannot connect to the X display\n");
        return 1;
    }
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;

    const xcb_window_t icon = xcb_generate_id(c);
    const uint32_t values[] = { 0xff0000,
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY };
    xcb_create_window(c, XCB_COPY_FROM_PARENT, icon, screen->root, 0, 0, 16, 16, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        values);
    const char wmClass[] = "xembedicon\0XEmbedIcon";
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, icon, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
        sizeof wmClass, wmClass);
    const char title[] = "XEmbed test icon";
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, icon, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
        sizeof title - 1, title);
    const xcb_atom_t xembedInfo = intern(c, "_XEMBED_INFO");
    const uint32_t info[] = { 0, 1 }; // version 0, mapped
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, icon, xembedInfo, xembedInfo, 32, 2, info);

    // Dock with the tray (waiting for one if there is none yet).
    char selectionName[32];
    std::snprintf(selectionName, sizeof selectionName, "_NET_SYSTEM_TRAY_S%d", screenNumber);
    const xcb_atom_t selection = intern(c, selectionName);
    xcb_window_t tray = XCB_WINDOW_NONE;
    for (int i = 0; i < 100 && tray == XCB_WINDOW_NONE; ++i) {
        xcb_get_selection_owner_reply_t *owner
            = xcb_get_selection_owner_reply(c, xcb_get_selection_owner(c, selection), nullptr);
        if (owner)
            tray = owner->owner;
        std::free(owner);
        if (tray == XCB_WINDOW_NONE) {
            struct timespec pause = { 0, 100 * 1000 * 1000 };
            nanosleep(&pause, nullptr);
        }
    }
    if (tray == XCB_WINDOW_NONE) {
        std::fprintf(stderr, "no system tray\n");
        return 1;
    }
    xcb_client_message_event_t dock = {};
    dock.response_type = XCB_CLIENT_MESSAGE;
    dock.format = 32;
    dock.window = tray;
    dock.type = intern(c, "_NET_SYSTEM_TRAY_OPCODE");
    dock.data.data32[0] = XCB_CURRENT_TIME;
    dock.data.data32[1] = 0; // SYSTEM_TRAY_REQUEST_DOCK
    dock.data.data32[2] = icon;
    xcb_send_event(c, false, tray, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<char *>(&dock));
    xcb_flush(c);
    std::printf("docked\n");
    std::fflush(stdout);

    while (xcb_generic_event_t *event = xcb_wait_for_event(c)) {
        if ((event->response_type & ~0x80) == XCB_BUTTON_PRESS) {
            const auto *press = reinterpret_cast<xcb_button_press_event_t *>(event);
            std::printf("press %d %u\n", press->detail, unsigned(press->time));
            std::fflush(stdout);
        }
        std::free(event);
    }
    return 0;
}
