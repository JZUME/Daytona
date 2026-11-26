// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QSize>
#include <QString>

namespace Daytona {

// A screen mode: its size in pixels and refresh rate in millihertz (0 when
// not known or not chosen).
struct ScreenMode
{
    QSize size;
    int refresh = 0;
    bool preferred = false; // the screen's own best mode

    bool isValid() const { return size.isValid() && !size.isEmpty(); }
    bool sameAs(const ScreenMode &other) const
    {
        return size == other.size && refresh == other.refresh;
    }
};

// "1920x1080@60000", as saved in daytona.ini; parseModeKey() reads it back
// (an invalid mode if the text is not one).
QString modeKey(const ScreenMode &mode);
ScreenMode parseModeKey(const QString &text);

// Of modes, the one to use for a wanted size and refresh rate: the exact
// match, or for refresh 0 (or one no longer offered) the preferred mode of
// that size, else its highest refresh rate. Invalid if the size is not
// offered.
ScreenMode matchMode(const QList<ScreenMode> &modes, const ScreenMode &wanted);

} // namespace Daytona
