// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusnotifierwatcher.h"

#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcSniWatcher, "daytona.sni.watcher", QtInfoMsg)

namespace Daytona {

namespace {

    // Bounds on what other programs can make the watcher hold.
    constexpr int MaxItems = 64;
    constexpr int MaxIdLength = 512;

    bool validBusName(const QString &name)
    {
        static const QRegularExpression unique(QStringLiteral("^:[A-Za-z0-9_.-]+$"));
        static const QRegularExpression wellKnown(
            QStringLiteral("^[A-Za-z_-][A-Za-z0-9_-]*(\\.[A-Za-z_-][A-Za-z0-9_-]*)+$"));
        return name.size() <= 255
            && (unique.match(name).hasMatch() || wellKnown.match(name).hasMatch());
    }

    bool validObjectPath(const QString &path)
    {
        static const QRegularExpression re(QStringLiteral("^/([A-Za-z0-9_]+(/[A-Za-z0-9_]+)*)?$"));
        return path.size() <= 255 && re.match(path).hasMatch();
    }

    QString serviceOf(const QString &item)
    {
        return item.section(QLatin1Char('/'), 0, 0);
    }

} // namespace

StatusNotifierWatcher::StatusNotifierWatcher(QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
{
    m_serviceWatcher->setConnection(m_connection);
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
        &StatusNotifierWatcher::serviceGone);
}

bool StatusNotifierWatcher::registerOnBus()
{
    if (!m_connection.isConnected())
        return false;
    if (!m_connection.registerObject(QString::fromLatin1(ObjectPath), this,
            QDBusConnection::ExportScriptableContents | QDBusConnection::ExportAllProperties)) {
        return false;
    }
    if (!m_connection.registerService(QString::fromLatin1(ServiceName))) {
        m_connection.unregisterObject(QString::fromLatin1(ObjectPath));
        return false;
    }
    return true;
}

QString StatusNotifierWatcher::itemId(const QString &service, const QString &sender)
{
    // Items pass either their bus name or (as Ayatana and others do) an
    // object path on their own connection.
    QString busName = sender;
    QString path = QStringLiteral("/StatusNotifierItem");
    if (service.startsWith(QLatin1Char('/')))
        path = service;
    else if (!service.isEmpty())
        busName = service;
    if (!validBusName(busName) || !validObjectPath(path))
        return {};
    return busName + path;
}

void StatusNotifierWatcher::RegisterStatusNotifierItem(const QString &service)
{
    const QString sender = calledFromDBus() ? message().service() : QString();
    const QString id = itemId(service, sender);
    if (id.isEmpty() || id.size() > MaxIdLength) {
        qCWarning(lcSniWatcher) << "Ignoring an invalid item registration from" << sender;
        return;
    }
    if (m_items.contains(id))
        return;
    if (m_items.size() >= MaxItems) {
        qCWarning(lcSniWatcher) << "Too many tray icons; ignoring" << id;
        return;
    }
    // A well-known name must belong to whoever registers it.
    const QString busName = serviceOf(id);
    if (!busName.startsWith(QLatin1Char(':')) && m_connection.interface()) {
        const QString owner = m_connection.interface()->serviceOwner(busName);
        if (owner.isEmpty() || (!sender.isEmpty() && owner != sender)) {
            qCWarning(lcSniWatcher) << "Ignoring" << id << "registered by another client";
            return;
        }
    }
    m_items << id;
    m_serviceWatcher->addWatchedService(busName);
    qCDebug(lcSniWatcher) << "registered" << id;
    Q_EMIT StatusNotifierItemRegistered(id);
}

void StatusNotifierWatcher::RegisterStatusNotifierHost(const QString &service)
{
    if (!validBusName(service) || m_hosts.contains(service) || m_hosts.size() >= MaxItems)
        return;
    m_hosts << service;
    m_serviceWatcher->addWatchedService(service);
    Q_EMIT StatusNotifierHostRegistered();
}

void StatusNotifierWatcher::serviceGone(const QString &service)
{
    for (const QString &item : QStringList(m_items)) {
        if (serviceOf(item) == service) {
            m_items.removeAll(item);
            Q_EMIT StatusNotifierItemUnregistered(item);
        }
    }
    if (m_hosts.removeAll(service) > 0 && m_hosts.isEmpty())
        Q_EMIT StatusNotifierHostUnregistered();
    m_serviceWatcher->removeWatchedService(service);
}

} // namespace Daytona
