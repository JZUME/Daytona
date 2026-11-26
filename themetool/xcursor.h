// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QList>
#include <QPoint>
#include <QString>

namespace Daytona {

// One image of an Xcursor file: the size it is for (its "nominal size"),
// the picture, the hotspot and, for animations, the frame delay in ms.
struct XcursorImage
{
    int nominal = 0;
    QImage image; // ARGB32 premultiplied
    QPoint hotspot;
    quint32 delay = 0;
};

// Reads the images of an Xcursor file. Returns no images and sets error
// when the data is not a valid Xcursor file.
QList<XcursorImage> readXcursor(const QByteArray &data, QString *error = nullptr);
// The Xcursor file holding images, in order.
QByteArray writeXcursor(const QList<XcursorImage> &images);

// The images of the given nominal size, or else those of the nearest size
// (as libXcursor picks them).
QList<XcursorImage> imagesForSize(const QList<XcursorImage> &images, int nominal);

// The cursor redrawn factor times larger in the style of Windows' large
// cursors: the shape's body is enlarged and given a new 1 pixel outline,
// instead of every pixel becoming a block (which would make the outline
// factor pixels thick). Thin strokes that have no body around them, such
// as a crosshair's lines, are enlarged as blocks so they do not vanish.
// The result is nominal pixels square; the hotspot stays on the shape.
XcursorImage scaleCursorImage(const XcursorImage &source, int nominal);

// Pointer schemes, as NT's Mouse Properties offered them: the nominal
// cursor size each one asks for (Chicago95's cursors are 32 pixels).
enum class PointerScheme
{
    Standard,
    Large,
    ExtraLarge
};
int pointerSchemeSize(PointerScheme scheme);

// The sizes Daytona's cursor theme holds: each scheme at screen scales
// 1, 1.5, 2 and 3 (128 and 192 also serve the larger schemes at 1.5 and 3
// as the nearest sizes).
QList<int> cursorThemeSizes();

// Daytona's cursor theme, generated at build time from Chicago95's.
inline constexpr char CursorThemeName[] = "Daytona Cursors";

// The file of a cursor in an installed cursor theme, looked up as cursor
// libraries do (XCURSOR_PATH, or the user's and system icon directories),
// and also in the XDG data directories; empty if there is none.
QString findCursorFile(const QString &theme, const QString &name);

// Cursor names whose Chicago95 image is not used: X_cursor and X11's
// novelty cursors share a parody of the Windows logo, and Daytona ships no
// Microsoft trademarks (AGENT.md). They are shown as the normal arrow,
// which X shows when nothing has set a cursor.
QStringList replacedCursorNames();

// Writes Daytona's cursor theme into targetDir from the Xcursor files in
// sourceCursorsDir: every cursor at every size in cursorThemeSizes(),
// identical cursors written once and the others made symbolic links to it,
// the names in replace linked to the cursor named by their value, and an
// index.theme naming the theme.
bool generateCursorTheme(const QString &sourceCursorsDir, const QString &targetDir,
    const QHash<QString, QString> &replace, QString *error = nullptr);

} // namespace Daytona
