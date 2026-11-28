// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// daytona-xembed-proxy: the X11 system tray (the XEmbed system tray
// protocol, _NET_SYSTEM_TRAY_S0) on the Xwayland display. Each icon an X11
// program docks is kept in a hidden window and shown in the notification
// area as a StatusNotifierItem; clicks on the item are replayed on the X11
// icon.

#include <QByteArray>
#include <QDBusArgument>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QTimer>
#include <xcb/xcb.h>

namespace Daytona {

struct SniPixmap
{
    int width = 0;
    int height = 0;
    QByteArray data; // ARGB32, network byte order
};

QDBusArgument &operator<<(QDBusArgument &argument, const SniPixmap &pixmap);
const QDBusArgument &operator>>(const QDBusArgument &argument, SniPixmap &pixmap);

class XEmbedProxy;

// One docked X11 icon and the StatusNotifierItem that stands for it. Each
// item has a D-Bus connection of its own, so that closing it removes the
// item from the notification area.
class XEmbedIcon : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category READ category)
    Q_PROPERTY(QString Id READ id)
    Q_PROPERTY(QString Title READ title)
    Q_PROPERTY(QString Status READ status)
    Q_PROPERTY(QList<Daytona::SniPixmap> IconPixmap READ iconPixmap)
    Q_PROPERTY(bool ItemIsMenu READ itemIsMenu)

public:
    XEmbedIcon(XEmbedProxy *proxy, xcb_window_t client);
    ~XEmbedIcon() override;

    xcb_window_t client() const { return m_client; }
    xcb_window_t container() const { return m_container; }
    bool isEmbedded() const { return m_container != XCB_WINDOW_NONE; }

    QString category() const { return QStringLiteral("ApplicationStatus"); }
    QString id() const { return m_id; }
    QString title() const { return m_title; }
    QString status() const
    {
        return m_mapped ? QStringLiteral("Active") : QStringLiteral("Passive");
    }
    QList<SniPixmap> iconPixmap() const { return m_pixmaps; }
    bool itemIsMenu() const { return false; }

    // Called by the proxy for the client's X events.
    void damaged();
    void propertyChanged(xcb_atom_t atom);
    void configured(int width, int height);
    void setMapped(bool mapped);

    // (Re)registers the item with the StatusNotifierWatcher.
    void registerItem();

public Q_SLOTS:
    Q_SCRIPTABLE void Activate(int x, int y);
    Q_SCRIPTABLE void SecondaryActivate(int x, int y);
    Q_SCRIPTABLE void ContextMenu(int x, int y);
    Q_SCRIPTABLE void Scroll(int delta, const QString &orientation);

Q_SIGNALS:
    Q_SCRIPTABLE void NewIcon();
    Q_SCRIPTABLE void NewTitle();
    Q_SCRIPTABLE void NewStatus(const QString &status);

private:
    void embed();
    void readTitle();
    void updateIcon();

    XEmbedProxy *m_proxy;
    xcb_window_t m_client;
    xcb_window_t m_container = XCB_WINDOW_NONE;
    uint32_t m_damage = 0;
    QString m_connectionName;
    QString m_id;
    QString m_title;
    QList<SniPixmap> m_pixmaps;
    bool m_mapped = false;
    QTimer m_iconTimer;
};

class XEmbedProxy : public QObject
{
    Q_OBJECT

public:
    explicit XEmbedProxy(QObject *parent = nullptr);
    ~XEmbedProxy() override;

    // Connects to the X display and becomes its system tray. False when
    // there is no X display, its extensions are missing or another program
    // is the tray already.
    bool start();

    xcb_connection_t *connection() const { return m_connection; }
    xcb_window_t root() const { return m_screen->root; }
    xcb_atom_t atom(const char *name);

    // The icon size: NT's 16 pixels. Xwayland programs draw at scale 1.
    static constexpr int IconSize = 16;

    // Container windows: 32-bit, so the icons' transparent parts are
    // transparent on screen too, and out of sight while not clicked.
    xcb_window_t createContainer();
    // Puts the container under (x, y) and clicks button on its client.
    void click(xcb_window_t container, xcb_window_t client, uint8_t button, QPoint pos);
    void hideContainer(xcb_window_t container);

private:
    void processEvents();
    void handleEvent(xcb_generic_event_t *event);
    void dock(xcb_window_t client);
    void undock(xcb_window_t client);
    XEmbedIcon *iconFor(xcb_window_t window) const;
    void findVisual();

    xcb_connection_t *m_connection = nullptr;
    xcb_screen_t *m_screen = nullptr;
    xcb_window_t m_selectionOwner = XCB_WINDOW_NONE;
    xcb_atom_t m_selection = XCB_ATOM_NONE;
    xcb_visualid_t m_visual = 0;
    uint8_t m_depth = 0;
    xcb_colormap_t m_colormap = 0;
    uint8_t m_damageEvent = 0;
    QHash<QByteArray, xcb_atom_t> m_atoms;
    QHash<xcb_window_t, XEmbedIcon *> m_icons;
    // Clicks on each container, to hide it after the last one.
    QHash<xcb_window_t, int> m_clicks;
    // The X server's time is the time of an event at start-up plus the
    // time since then.
    xcb_timestamp_t serverTime() const;
    xcb_timestamp_t m_serverTimeBase = 0;
    QElapsedTimer m_serverTimer;
    // The menu opened by a click on an icon, and the window that catches
    // clicks beside it.
    void popupMapped(xcb_window_t window);
    void hideCatcher();
    static constexpr int MenuTime = 2000; // ms after a click
    QElapsedTimer m_menuTimer;
    xcb_window_t m_menu = XCB_WINDOW_NONE;
    xcb_window_t m_catcher = XCB_WINDOW_NONE;
    // The root window's size, which follows the screens.
    QSize m_rootSize;
    static constexpr int ClickTime = 250; // ms
};

} // namespace Daytona

Q_DECLARE_METATYPE(Daytona::SniPixmap)
