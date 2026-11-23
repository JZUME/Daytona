// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QDBusContext>
#include <QObject>
#include <QStringList>

class QDBusServiceWatcher;

namespace Daytona {

// org.kde.StatusNotifierWatcher: the registry tray icons (StatusNotifierItems)
// announce themselves to, and hosts (the notification area) read from.
// Daytona runs one only when no other watcher is on the bus.
class StatusNotifierWatcher : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredItems)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isHostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)

public:
    static constexpr const char *ServiceName = "org.kde.StatusNotifierWatcher";
    static constexpr const char *ObjectPath = "/StatusNotifierWatcher";

    explicit StatusNotifierWatcher(QDBusConnection connection, QObject *parent = nullptr);

    // Takes the well-known name. Returns false if another watcher has it.
    bool registerOnBus();

    QStringList registeredItems() const { return m_items; }
    bool isHostRegistered() const { return !m_hosts.isEmpty(); }
    int protocolVersion() const { return 0; }

    // The item id for a registration: "busname/objectpath". service is what
    // the item passed (a bus name or an object path); sender is its unique
    // bus name. Returns empty if service is not acceptable.
    static QString itemId(const QString &service, const QString &sender);

public Q_SLOTS:
    Q_SCRIPTABLE void RegisterStatusNotifierItem(const QString &service);
    Q_SCRIPTABLE void RegisterStatusNotifierHost(const QString &service);

Q_SIGNALS:
    Q_SCRIPTABLE void StatusNotifierItemRegistered(const QString &item);
    Q_SCRIPTABLE void StatusNotifierItemUnregistered(const QString &item);
    Q_SCRIPTABLE void StatusNotifierHostRegistered();
    Q_SCRIPTABLE void StatusNotifierHostUnregistered();

private:
    void serviceGone(const QString &service);

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_serviceWatcher;
    QStringList m_items;
    QStringList m_hosts;
};

} // namespace Daytona
