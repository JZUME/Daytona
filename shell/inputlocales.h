// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keyboardlayout.h"

#include <QObject>

struct wl_keyboard;
struct wl_registry;
struct wl_seat;

namespace Daytona {

class Settings;

// The session's input locales and which one is in use, for the
// notification area's indicator.
//
// XKB switches between layouts itself (the hot key is an XKB option), so
// the active one is the keyboard's layout group. labwc sends keyboard
// modifiers, which carry the group, to every client, focused or not, so
// the shell follows it through its own wl_keyboard.
class InputLocales : public QObject
{
    Q_OBJECT

public:
    // labwcDir is where labwc reads its environment file
    // (labwcConfigDir()); without one, changes wait for the next log on.
    InputLocales(Settings *settings, const QString &labwcDir, QObject *parent = nullptr);
    ~InputLocales() override;

    // Reads the session's locales and starts following the keyboard.
    void start();
    // The system's keyboard file, for tests (defaultSystemKeyboardFile()).
    void setSystemKeyboardFile(const QString &path) { m_systemFile = path; }

    // In labwc's order: the group number is the index.
    const QList<InputLocale> &locales() const { return m_config.locales; }
    int currentIndex() const { return m_group; }
    InputLocale current() const { return m_config.locales.value(m_group); }

public Q_SLOTS:
    // Makes locales().at(index) the one in use, as the indicator's menu
    // does: there is no request to change the group, so labwc is given the
    // layouts again starting with that one.
    void select(int index);
    // Passes the settings to labwc after Keyboard Properties changed them.
    void reload();
    // The keyboard's layout group changed.
    void setGroup(int group);

Q_SIGNALS:
    void changed();

private:
    void bindKeyboard();
    void apply();

    Settings *m_settings;
    QString m_labwcDir;
    QString m_systemFile = defaultSystemKeyboardFile();
    XkbConfig m_config;
    int m_group = 0;

    struct wl_registry *m_registry = nullptr;
    struct wl_seat *m_seat = nullptr;
    struct wl_keyboard *m_keyboard = nullptr;
    friend struct InputLocalesWayland;
};

} // namespace Daytona
