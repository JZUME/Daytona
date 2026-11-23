// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings.h"

#include <QFutureWatcher>
#include <QImage>
#include <QWidget>

class QMenu;

namespace Daytona {

// How the wallpaper is placed, in device pixels, as NT drew it: centred on
// the desktop colour, or tiled from the top-left corner. Returns where the
// image's top-left corner goes for centre mode.
QPoint centredWallpaperPosition(const QSize &desktopPixels, const QSize &imagePixels);

// The desktop: NT's desktop colour and wallpaper on a background layer
// surface, with a right-click menu.
class Desktop : public QWidget
{
    Q_OBJECT

public:
    explicit Desktop(Settings *settings, QWidget *parent = nullptr);

    // Makes this a layer-shell background surface. Call before show().
    void attachToLayerShell();

    QMenu *contextMenu() const { return m_menu; }
    // The wallpaper being shown; null while there is none or it is loading.
    QImage wallpaper() const { return m_wallpaper; }

Q_SIGNALS:
    // A new wallpaper finished loading (or failed to, and is null).
    void wallpaperLoaded();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void loadWallpaper();

    Settings *m_settings;
    QMenu *m_menu;
    QString m_wallpaperPath;
    WallpaperMode m_mode = WallpaperMode::Centre;
    QImage m_wallpaper;
    QFutureWatcher<QImage> m_loader;
};

} // namespace Daytona
