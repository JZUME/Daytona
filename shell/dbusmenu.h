// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QList>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <functional>

class QDBusArgument;

class QWidget;

namespace Daytona {

// One entry of a com.canonical.dbusmenu layout.
struct DBusMenuNode
{
    int id = 0;
    QVariantMap properties;
    QList<DBusMenuNode> children;
};

// Parses a GetLayout reply's (ia{sv}av) structure. Menus come from other
// programs: depth and size are bounded, and malformed parts are dropped.
// Returns false if the root itself is malformed.
bool parseDBusMenuLayout(const QDBusArgument &argument, DBusMenuNode *root);

// dbusmenu labels mark mnemonics with "_" ("__" is a literal underscore);
// Qt uses "&".
QString dbusMenuLabelToQt(const QString &label);

// Fills menu with node's children: labels, enabled and visible states,
// separators, submenus, check and radio toggles and theme icons. Choosing an
// item calls clicked with its id.
void populateDBusMenu(QMenu *menu, const DBusMenuNode &node,
    const std::function<void(int id)> &clicked,
    const std::function<void(QMenu *submenu, int id)> &aboutToShow = {});

// A client for an application's com.canonical.dbusmenu menu, as tray icons
// export it. All calls are asynchronous with a timeout, so a slow or broken
// application never blocks the shell.
class DBusMenuClient : public QObject
{
    Q_OBJECT

public:
    DBusMenuClient(const QString &service, const QString &path, QDBusConnection connection,
        QObject *parent = nullptr);

    // Fetches the menu and pops it up at globalPos, parented to anchor.
    // If the application does not answer in time, nothing is shown.
    void popup(const QPoint &globalPos, QWidget *anchor);

    // The menu last shown (for tests); null before the first popup.
    QMenu *menu() const { return m_menu; }

Q_SIGNALS:
    // The menu was fetched and shown, or could not be.
    void shown();
    void failed();

private Q_SLOTS:
    void layoutChanged();

private:
    void fetch(int parentId, const std::function<void(const DBusMenuNode &)> &done);
    void sendEvent(int id);
    void aboutToShow(QMenu *submenu, int id);
    void fill(QMenu *menu, const DBusMenuNode &node);

    QString m_service;
    QString m_path;
    QDBusConnection m_connection;
    QPointer<QMenu> m_menu;
    QTimer m_relayout;
};

} // namespace Daytona
