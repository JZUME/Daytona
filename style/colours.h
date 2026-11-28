// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QBrush>
#include <QColor>
#include <QMap>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <optional>

namespace Daytona {

// Windows colour names ("ButtonFace", "ActiveTitle", ...) to colours.
using SchemeColours = QMap<QString, QColor>;

// Every Windows colour key, in the order Windows lists them.
const QStringList &colourKeys();

// NT 4.0's "Windows Standard" colours, one for every key.
const SchemeColours &standardColours();

// Parses a Windows "R G B" colour value (0-255 each).
std::optional<QColor> parseRgb(const QString &value);

// Builds the Qt palette for a colour scheme. Missing keys use Windows
// Standard. See docs/colour-schemes.md for the mapping.
QPalette paletteFromColours(const SchemeColours &colours);

// Whether a scheme gives the active (or inactive) title bar two colours,
// ActiveTitle and GradientActiveTitle, which Windows 98 and 2000 blend from left
// to right. NT schemes set them the same and their title bars stay solid.
bool titleBarHasGradient(const SchemeColours &colours, bool active);

// The title bar fill for rect: that blend, or the solid title colour.
QBrush titleBarBrush(const SchemeColours &colours, bool active, const QRect &rect);

// The active colour scheme, as written by daytona-themetool apply and read by
// the shell and the daytona style plugin.
struct ColourFile
{
    QString name;
    SchemeColours colours; // every key; missing or invalid ones are Windows Standard
};

// $XDG_CONFIG_HOME/daytona/colors.ini
QString colourFilePath();

// Reads a colour file. Returns nothing if it is missing or unreadable.
std::optional<ColourFile> readColourFile(const QString &path);

// Writes a colour file atomically (a temporary file renamed into place).
bool writeColourFile(const QString &path, const ColourFile &file, QString *error = nullptr);

} // namespace Daytona
