// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"

#include <QProxyStyle>
#include <QTimer>

class QFileSystemWatcher;

class QStyleOptionMenuItem;

namespace Daytona {

// The Windows NT 4.0 look for Qt Widgets.
//
// Built on Qt's own "windows" style, which already lays out and draws
// controls in the Windows 95 manner from QPalette. Daytona replaces the parts
// that differ from NT (bevels, default button frame, pressed and latched
// buttons, focus rectangles, disabled text) and turns off animation.
class Style : public QProxyStyle
{
    Q_OBJECT

public:
    Style();

    // The Windows Standard colour scheme.
    static QPalette ntPalette();
    // The palette of the current colour scheme.
    QPalette standardPalette() const override;

    // A colour of the current scheme by its Windows key, including those
    // with no palette role ("ActiveTitle", "Background", ...).
    static QColor schemeColour(const QString &key);
    // The current scheme's active title bar fill: solid, or the Windows
    // 98/2000 gradient where the scheme sets one. See titleBarBrush().
    static QBrush titleBarBrush(const QRect &rect);

    // Where the colour scheme is read from; colourFilePath() by default.
    // Call before the style is set on the application.
    void setColourFile(const QString &path);

    // Set this dynamic property (an int, in logical pixels) on a QMenu to
    // change its icon size, e.g. 32 for the Start menu. Default is 16.
    static constexpr const char *MenuIconSizeProperty = "daytonaMenuIconSize";

    // Loads the colour scheme, applies it as the application palette and
    // follows changes to the file.
    void polish(QApplication *application) override;
    using QProxyStyle::polish;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
        const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
        const QWidget *widget = nullptr) const override;
    void drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &palette,
        bool enabled, const QString &text,
        QPalette::ColorRole textRole = QPalette::NoRole) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contents,
        const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
        const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override;

Q_SIGNALS:
    // The colour scheme changed and the new palette has been applied.
    void coloursChanged();

private:
    void loadColourFile();
    void watchColourFile();

    static SchemeColours s_colours;
    QString m_colourFile;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer m_reload;
    bool m_applied = false;

    void drawMenuItem(const QStyleOptionMenuItem *item, QPainter *painter) const;
    void drawMenuBarItem(
        const QStyleOptionMenuItem *item, QPainter *painter, const QWidget *widget) const;
};

} // namespace Daytona
