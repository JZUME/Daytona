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

// NT 4.0's built-in schemes. "Windows Standard" first.
QStringList builtInSchemeNames();
std::optional<ColourScheme> builtInScheme(const QString &name);

// Reads a Windows 95/NT/Plus! .theme file. Never fails outright: what cannot
// be read is reported in warnings and falls back to Windows Standard.
// Returns nothing only if the file cannot be opened.
std::optional<ColourScheme> readThemeFile(const QString &path);

// The same, from bytes; themeDir is where referenced files are looked for.
ColourScheme parseTheme(const QByteArray &data, const QString &name, const QString &themeDir);

} // namespace Daytona
