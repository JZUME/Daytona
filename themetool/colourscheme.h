// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"

#include <QColor>
#include <QMap>
#include <QString>
#include <QStringList>
#include <optional>

namespace Daytona {

// A Windows colour scheme and appearance settings, as read from a .theme
// file or taken from the built-in schemes.
struct ColourScheme
{
    QString name;
    // Windows colour names ("ButtonFace", "ActiveTitle", ...) to colours.
    // Always contains every key in colourKeys().
    QMap<QString, QColor> colours;
    QString wallpaper; // absolute path inside the theme's directory, or empty
    // daytona.ini's WallpaperMode: "centre", "tile", "stretch", "fit" or "fill".
    QString wallpaperMode = QStringLiteral("centre");
    QMap<QString, QString> cursors; // cursor role -> absolute path, as found
    QStringList warnings; // unsupported or missing parts, for the user

    QColor colour(const QString &key) const { return colours.value(key); }
};

// The built-in schemes: "Daytona Classic" (NT 4.0's solid navy title bars,
// the default), "Daytona Standard" (the same with Windows 98 to XP Classic's
// two-tone gradient title bars) and NT's two high-contrast schemes.
QStringList builtInSchemeNames();
// Case-insensitive; the old name "Windows Standard" gives Daytona Classic.
std::optional<ColourScheme> builtInScheme(const QString &name);
// A scheme name as daytona.ini may hold it, with retired built-in names
// ("Windows Standard") changed to their current ones. Other names unchanged.
QString currentSchemeName(const QString &name);

// Reads a Windows 95/NT/Plus! .theme file. Never fails outright: what cannot
// be read is reported in warnings and falls back to Daytona Classic.
// Returns nothing only if the file cannot be opened.
std::optional<ColourScheme> readThemeFile(const QString &path);

// The same, from bytes; themeDir is where referenced files are looked for.
ColourScheme parseTheme(const QByteArray &data, const QString &name, const QString &themeDir);

} // namespace Daytona
