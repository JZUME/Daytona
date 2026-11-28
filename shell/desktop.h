// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings.h"

#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QWidget>

class QMenu;
class QScreen;

namespace Daytona {

class DesktopIconView;

// Where the wallpaper goes on the desktop, in device pixels, for every mode
// but Tile (which repeats it from the top-left corner):
// - Centre: unscaled, centred on the desktop colour, as NT drew it.
// - Stretch: scaled to cover the desktop exactly, ignoring its proportions.
// - Fit: scaled to fit inside the desktop, with desktop colour either side.
// - Fill: scaled to cover the desktop, cropped evenly where it overflows.
QRect wallpaperRect(const QSize &desktopPixels, const QSize &imagePixels, WallpaperMode mode);

// The desktop: NT's desktop colour and wallpaper on a background layer
// surface, with a right-click menu.
class Desktop : public QWidget
{
    Q_OBJECT

public:
    explicit Desktop(Settings *settings, QWidget *parent = nullptr);

    // Makes this a layer-shell background surface on screen (the window's
    // own screen if null). Call before show().
    void attachToLayerShell(QScreen *screen = nullptr);

    // Shows NT's desktop icons on this desktop (the primary monitor's), or
    // takes them away. While shown, the desktop takes keyboard focus when
    // clicked, and the icons' menu replaces contextMenu().
    void setShowIcons(bool show);
    DesktopIconView *icons() const { return m_icons; }
    // The edges icons keep clear of (the taskbar), in logical pixels.
    void setIconMargins(const QMargins &margins);

    QMenu *contextMenu() const { return m_menu; }
    // The wallpaper being shown; null while there is none or it is loading.
    QImage wallpaper() const { return m_wallpaper; }

Q_SIGNALS:
    // Properties was chosen from the desktop menu.
    void propertiesRequested();
    // Properties was chosen on My Computer.
    void systemPropertiesRequested();
    // A new wallpaper finished loading (or failed to, and is null).
    void wallpaperLoaded();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void loadWallpaper();

    Settings *m_settings;
    QMenu *m_menu;
    QAction *m_properties;
    DesktopIconView *m_icons = nullptr;
    QMargins m_iconMargins;
    QString m_wallpaperPath;
    WallpaperMode m_mode = WallpaperMode::Centre;
    QImage m_wallpaper;
    // The wallpaper scaled for Stretch, Fit or Fill, kept between paints.
    QImage m_scaled;
    QFutureWatcher<QImage> m_loader;
};

// One desktop per output, following outputs as they come and go.
class DesktopManager : public QObject
{
    Q_OBJECT

public:
    explicit DesktopManager(Settings *settings, QObject *parent = nullptr);
    ~DesktopManager() override;

    // Creates a desktop for every screen and follows screen changes.
    void start();
    QList<Desktop *> desktops() const { return m_desktops.values(); }
    Desktop *desktop(QScreen *screen) const { return m_desktops.value(screen); }

    // Shows the desktop icons on screen's desktop (the taskbar's), keeping
    // clear of margins.
    void setIconScreen(QScreen *screen, const QMargins &margins);

Q_SIGNALS:
    // Properties was chosen on any desktop.
    void propertiesRequested();
    // Properties was chosen on My Computer.
    void systemPropertiesRequested();

private:
    void add(QScreen *screen);
    void remove(QScreen *screen);

    Settings *m_settings;
    QHash<QScreen *, Desktop *> m_desktops;
    QScreen *m_iconScreen = nullptr;
    QMargins m_iconMargins;
};

} // namespace Daytona
