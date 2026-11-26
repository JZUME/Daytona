// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QObject>
#include <QPoint>
#include <QTimer>
#include <QVariantMap>

class QDBusArgument;
class QWidget;

namespace Daytona {

class DBusMenuClient;

// Parsers for StatusNotifierItem data, which comes from other programs and
// is treated as untrusted: malformed or oversized values are dropped.

// IconPixmap and friends: a(iiay), ARGB32 in network byte order.
QList<QImage> parseSniPixmaps(const QDBusArgument &argument);

// The pixmap to draw at size (device pixels): the smallest at least that
// big, else the largest. Null if there are none.
QImage bestSniPixmap(const QList<QImage> &pixmaps, int size);

// ToolTip: (sa(iiay)ss). Returns the title, the only part NT-style tooltips
// show; empty if malformed.
QString parseSniToolTipTitle(const QDBusArgument &argument);

// One tray icon, as announced by an application.
class StatusNotifierItem : public QObject
{
    Q_OBJECT

public:
    enum class Status
    {
        Passive,
        Active,
        NeedsAttention
    };

    // id is "busname/objectpath", as the watcher lists items.
    StatusNotifierItem(const QString &id, QDBusConnection connection, QObject *parent = nullptr);

    QString id() const { return m_id; }
    QString service() const { return m_service; }
    QString path() const { return m_path; }

    // False until the item's properties have been read once.
    bool isReady() const { return m_ready; }
    Status status() const { return m_status; }
    QString title() const { return m_title; }
    QString toolTipTitle() const { return m_toolTipTitle; }
    // The icon for the current status, at size device pixels.
    QPixmap pixmap(int size) const;
    // The object path of the item's com.canonical.dbusmenu menu, if any.
    QString menuPath() const { return m_menuPath; }
    // The item is only a menu: a left click shows it too.
    bool itemIsMenu() const { return m_itemIsMenu; }
    // The client for the menu, once it has been shown.
    DBusMenuClient *menuClient() const { return m_menuClient; }

    // Applies properties as returned by org.freedesktop.DBus.Properties.GetAll.
    // Public for tests.
    void applyProperties(const QVariantMap &properties);

public Q_SLOTS:
    // Clicks, forwarded to the application with the pointer position.
    // Items made with libayatana-appindicator (nm-applet, for one) have no
    // Activate method, only a menu: activate() then shows the menu instead,
    // beside anchor, as other desktops do.
    void activate(const QPoint &globalPos, QWidget *anchor = nullptr);
    void secondaryActivate(const QPoint &globalPos);
    void contextMenu(const QPoint &globalPos);
    // The item's menu: its DBusMenu when it has one, else ContextMenu. The
    // DBusMenu is shown beside anchor (see popUpBesideTrayIcon).
    void showMenu(const QPoint &globalPos, QWidget *anchor);

    // Re-reads every property (asynchronously).
    void refresh();

Q_SIGNALS:
    void changed();

private:
    void call(const char *method, const QPoint &globalPos);

    QString m_id;
    QString m_service;
    QString m_path;
    QDBusConnection m_connection;
    QTimer m_refresh;
    bool m_ready = false;

    Status m_status = Status::Active;
    QString m_title;
    QString m_toolTipTitle;
    QString m_iconName;
    QString m_attentionIconName;
    QString m_iconThemePath;
    QString m_menuPath;
    bool m_itemIsMenu = false;
    bool m_noActivate = false; // Activate is not implemented
    DBusMenuClient *m_menuClient = nullptr;
    QList<QImage> m_pixmaps;
    QList<QImage> m_attentionPixmaps;
};

} // namespace Daytona
