// SPDX-License-Identifier: GPL-3.0-or-later

#include "inputlocales.h"

#include "settings.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTimer>
#include <QtGui/qguiapplication_platform.h>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <wayland-client.h>

Q_LOGGING_CATEGORY(lcInputLocales, "daytona.inputlocales", QtInfoMsg)

namespace Daytona {

namespace {

    QList<InputLocale> rotated(const QList<InputLocale> &list, int first)
    {
        QList<InputLocale> result = list.mid(first);
        result += list.mid(0, first);
        return result;
    }

    bool sameSet(QList<InputLocale> a, QList<InputLocale> b)
    {
        const auto less
            = [](const InputLocale &x, const InputLocale &y) { return x.code() < y.code(); };
        std::sort(a.begin(), a.end(), less);
        std::sort(b.begin(), b.end(), less);
        return a == b;
    }

} // namespace

// The Wayland listeners. Qt dispatches the default event queue on the main
// thread, so these run there.
struct InputLocalesWayland
{
    static void keymap(void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t) { ::close(fd); }
    static void enter(void *, wl_keyboard *, uint32_t, wl_surface *, wl_array *) { }
    static void leave(void *, wl_keyboard *, uint32_t, wl_surface *) { }
    static void key(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t) { }
    static void modifiers(
        void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t group)
    {
        static_cast<InputLocales *>(data)->setGroup(int(group));
    }
    static void repeatInfo(void *, wl_keyboard *, int32_t, int32_t) { }
    static constexpr wl_keyboard_listener KeyboardListener
        = { keymap, enter, leave, key, modifiers, repeatInfo };

    static void capabilities(void *data, wl_seat *seat, uint32_t caps)
    {
        auto *self = static_cast<InputLocales *>(data);
        const bool hasKeyboard = caps & WL_SEAT_CAPABILITY_KEYBOARD;
        if (hasKeyboard && !self->m_keyboard) {
            self->m_keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(self->m_keyboard, &KeyboardListener, self);
        } else if (!hasKeyboard && self->m_keyboard) {
            wl_keyboard_release(self->m_keyboard);
            self->m_keyboard = nullptr;
        }
    }
    static void name(void *, wl_seat *, const char *) { }
    static constexpr wl_seat_listener SeatListener = { capabilities, name };

    static void global(
        void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
    {
        auto *self = static_cast<InputLocales *>(data);
        if (self->m_seat || std::strcmp(interface, wl_seat_interface.name) != 0)
            return;
        // Version 5 has wl_keyboard.release and nothing this needs later.
        self->m_seat = static_cast<wl_seat *>(
            wl_registry_bind(registry, id, &wl_seat_interface, std::min(version, 5u)));
        wl_seat_add_listener(self->m_seat, &SeatListener, self);
    }
    static void globalRemove(void *, wl_registry *, uint32_t) { }
    static constexpr wl_registry_listener RegistryListener = { global, globalRemove };
};

InputLocales::InputLocales(Settings *settings, const QString &labwcDir, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_labwcDir(labwcDir)
{
}

InputLocales::~InputLocales()
{
    if (m_keyboard)
        wl_keyboard_release(m_keyboard);
    if (m_seat)
        wl_seat_release(m_seat);
    if (m_registry)
        wl_registry_destroy(m_registry);
}

void InputLocales::start()
{
    m_config = sessionKeyboard(m_settings, systemKeyboard(m_systemFile));
    // labwc's order may differ from the settings' when the indicator's menu
    // changed it before the shell restarted.
    if (!m_labwcDir.isEmpty()) {
        QFile file(QDir(m_labwcDir).filePath(QStringLiteral("environment")));
        if (file.open(QIODevice::ReadOnly)) {
            const QList<InputLocale> active = localesFromEnvironment(file.read(64 * 1024));
            if (sameSet(active, m_config.locales))
                m_config.locales = active;
        }
    }
    m_group = 0;
    bindKeyboard();
    Q_EMIT changed();
}

void InputLocales::bindKeyboard()
{
    if (m_registry)
        return;
    auto *waylandApp
        = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    if (!waylandApp || !waylandApp->display())
        return;
    m_registry = wl_display_get_registry(waylandApp->display());
    wl_registry_add_listener(m_registry, &InputLocalesWayland::RegistryListener, this);
}

void InputLocales::setGroup(int group)
{
    if (group < 0 || group >= m_config.locales.size())
        group = 0;
    if (group == m_group)
        return;
    m_group = group;
    Q_EMIT changed();
}

void InputLocales::select(int index)
{
    if (index < 0 || index >= m_config.locales.size() || index == m_group)
        return;
    const QList<InputLocale> wanted = rotated(m_config.locales, index);
    if (wanted == m_config.locales) {
        // Only the group differs, and labwc keeps the group while the
        // keymap stays the same: give it another order first, so the
        // keymap is rebuilt and starts on its first layout.
        XkbConfig other = m_config;
        other.locales = rotated(m_config.locales, 1);
        QString error;
        if (!applyToLabwc(other, m_labwcDir, &error)) {
            qCWarning(lcInputLocales) << "Cannot switch the keyboard layout:" << error;
            return;
        }
        QTimer::singleShot(250, this, [this] { apply(); });
    } else {
        m_config.locales = wanted;
        apply();
    }
    m_group = 0;
    Q_EMIT changed();
}

void InputLocales::reload()
{
    m_config = sessionKeyboard(m_settings, systemKeyboard(m_systemFile));
    m_group = 0;
    apply();
    Q_EMIT changed();
}

void InputLocales::apply()
{
    QString error;
    if (!applyToLabwc(m_config, m_labwcDir, &error))
        qCWarning(lcInputLocales) << "Cannot change the keyboard layout now:" << error;
}

} // namespace Daytona
