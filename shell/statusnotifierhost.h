// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QList>
#include <QObject>

namespace Daytona {

class StatusNotifierItem;
class StatusNotifierWatcher;

// The notification area's side of the tray protocol: registers as a
// StatusNotifierHost, runs a watcher if the session has none, and keeps one
// StatusNotifierItem per registered tray icon, in registration order.
class StatusNotifierHost : public QObject
{
    Q_OBJECT

public:
    explicit StatusNotifierHost(QDBusConnection connection, QObject *parent = nullptr);

    // Connects to the watcher (starting Daytona's own if needed). Returns
    // false if there is no usable session bus.
    bool start();

    const QList<StatusNotifierItem *> &items() const { return m_items; }
    // True if this host runs the session's watcher.
    bool ownsWatcher() const { return m_watcher != nullptr; }

Q_SIGNALS:
    // Fully qualified so queued connections and QSignalSpy can find the type.
    void itemAdded(Daytona::StatusNotifierItem *item);
    // Emitted before the item is deleted.
    void itemRemoved(Daytona::StatusNotifierItem *item);

private Q_SLOTS:
    void addItem(const QString &id);
    void removeItem(const QString &id);

private:
    QDBusConnection m_connection;
    StatusNotifierWatcher *m_watcher = nullptr;
    QString m_hostName;
    QList<StatusNotifierItem *> m_items;
};

} // namespace Daytona
