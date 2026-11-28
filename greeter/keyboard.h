// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace Daytona {

// The system's keyboard, as XKB names.
struct SystemKeyboard
{
    QString model;
    QString layout;
    QString variant;
    QString options;

    bool isEmpty() const { return layout.isEmpty(); }
};

// Reads /etc/default/keyboard's format (XKBMODEL="pc105" and so on).
SystemKeyboard parseKeyboardDefaults(const QByteArray &text);

// The system keyboard: systemd-localed's X11 settings (org.freedesktop.locale1),
// else /etc/default/keyboard, which is where localed keeps them on Debian.
SystemKeyboard systemKeyboard();

// labwc's environment file for keyboard: XKB_DEFAULT_* lines.
QString labwcEnvironment(const SystemKeyboard &keyboard);

} // namespace Daytona
