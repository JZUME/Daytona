// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>

namespace Daytona {

// GTK 3 applications in a Daytona session use the "Daytona" GTK theme
// (the session sets GTK_THEME=Daytona). The shipped one imports Chicago95
// unchanged; for other colour schemes daytona-themetool writes a user copy
// that imports Chicago95 and redefines its named colours.

// The Chicago95 GTK 3 stylesheet, found in the data directories, or empty.
QString findChicago95GtkCss();

// gtk.css for the scheme: an import of chicagoCss and @define-color rules.
QByteArray gtkCss(const SchemeColours &colours, const QString &chicagoCss);

// $XDG_DATA_HOME/themes/Daytona/gtk-3.0
QString gtkUserThemeDir();

// Writes or removes the generated theme in dir. As with the labwc theme, only
// a directory marked as generated is ever replaced or removed.
bool writeGtkTheme(
    const QString &dir, const SchemeColours &colours, const QString &chicagoCss, QString *error);
bool removeGtkTheme(const QString &dir, QString *error);

// $XDG_CONFIG_HOME/gtk-3.0/settings.ini
QString gtkSettingsPath();

// Sets the icon and cursor themes in a GTK settings.ini, keeping everything
// else in it. The first time Daytona changes an existing file, the original
// is kept next to it as settings.ini.daytona-backup.
bool setGtkSettings(const QString &path, const QString &iconTheme, const QString &cursorTheme,
    QString *error = nullptr);
// Sets the cursor theme and size in a GTK settings.ini in the same way.
bool setGtkCursorSettings(
    const QString &path, const QString &cursorTheme, int size, QString *error = nullptr);
// Sets the given keys in a GTK settings.ini's [Settings] group in the same
// way. A file that already has these values is not written.
bool setGtkSettingValues(const QString &path, const QList<QPair<QByteArray, QByteArray>> &values,
    QString *error = nullptr);

} // namespace Daytona
