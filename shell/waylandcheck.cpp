// SPDX-License-Identifier: GPL-3.0-or-later

#include "waylandcheck.h"

#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <cstring>
#include <wayland-client.h>

namespace Daytona {

namespace {

    struct Search
    {
        const char *name;
        bool found;
    };

    void onGlobal(void *data, wl_registry *, uint32_t, const char *interface, uint32_t)
    {
        auto *search = static_cast<Search *>(data);
        if (std::strcmp(interface, search->name) == 0)
            search->found = true;
    }

    void onGlobalRemove(void *, wl_registry *, uint32_t) { }

    const wl_registry_listener RegistryListener = { onGlobal, onGlobalRemove };

} // namespace

bool compositorHasGlobal(const char *interfaceName)
{
    auto *waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!waylandApp || !waylandApp->display())
        return false;
    // A private registry and one round trip; Qt's own registry is not
    // affected.
    wl_display *display = waylandApp->display();
    wl_event_queue *queue = wl_display_create_queue(display);
    auto *wrapped = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapped), queue);
    wl_registry *registry = wl_display_get_registry(wrapped);
    Search search { interfaceName, false };
    wl_registry_add_listener(registry, &RegistryListener, &search);
    wl_display_roundtrip_queue(display, queue);
    wl_registry_destroy(registry);
    wl_proxy_wrapper_destroy(wrapped);
    wl_event_queue_destroy(queue);
    return search.found;
}

} // namespace Daytona
