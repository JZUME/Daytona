// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusnotifierhost.h"

#include "statusnotifieritem.h"
#include "statusnotifierwatcher.h"

#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcSniHost, "daytona.sni.host", QtInfoMsg)

namespace Daytona {

namespace {
    const QString WatcherInterface = QStringLiteral("org.kde.StatusNotifierWatcher");
} // namespace

StatusNotifierHost::StatusNotifierHost(QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
{
}

bool StatusNotifierHost::start()
{
    if (!m_connection.isConnected() || !m_connection.interface())
        return false;

    const QString watcherName = QString::fromLatin1(StatusNotifierWatcher::ServiceName);
    if (!m_connection.interface()->isServiceRegistered(watcherName)) {
        auto *watcher = new StatusNotifierWatcher(m_connection, this);
        if (watcher->registerOnBus()) {
            m_watcher = watcher;
            qCInfo(lcSniHost) << "Running the session's StatusNotifierWatcher";
        } else {
            delete watcher; // another one appeared meanwhile
        }
    }

    m_hostName
        = QStringLiteral("org.kde.StatusNotifierHost-%1").arg(QCoreApplication::applicationPid());
    m_connection.registerService(m_hostName);

    const QString path = QString::fromLatin1(StatusNotifierWatcher::ObjectPath);
    m_connection.connect(watcherName, path, WatcherInterface,
        QStringLiteral("StatusNotifierItemRegistered"), this, SLOT(addItem(QString)));
    m_connection.connect(watcherName, path, WatcherInterface,
        QStringLiteral("StatusNotifierItemUnregistered"), this, SLOT(removeItem(QString)));

    if (m_watcher) {
        m_watcher->RegisterStatusNotifierHost(m_hostName);
    } else {
        QDBusMessage message = QDBusMessage::createMethodCall(
            watcherName, path, WatcherInterface, QStringLiteral("RegisterStatusNotifierHost"));
        message << m_hostName;
        m_connection.asyncCall(message, 5000);
    }

    // Items registered before the host started.
    QDBusMessage get = QDBusMessage::createMethodCall(watcherName, path,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    get << WatcherInterface << QStringLiteral("RegisteredStatusNotifierItems");
    auto *pending = new QDBusPendingCallWatcher(m_connection.asyncCall(get, 5000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QDBusVariant> reply = *w;
        if (reply.isError())
            return;
        const QVariant value = reply.value().variant();
        if (value.typeId() != QMetaType::QStringList)
            return;
        for (const QString &id : value.toStringList())
            addItem(id);
    });
    return true;
}

void StatusNotifierHost::addItem(const QString &id)
{
    // The same checks the watcher applies, for an external watcher.
    if (StatusNotifierWatcher::itemId(
            QLatin1Char('/') + id.section(QLatin1Char('/'), 1), id.section(QLatin1Char('/'), 0, 0))
        != id) {
        qCWarning(lcSniHost) << "Ignoring invalid tray item" << id.left(100);
        return;
    }
    for (StatusNotifierItem *item : std::as_const(m_items)) {
        if (item->id() == id)
            return;
    }
    if (m_items.size() >= 64)
        return;
    auto *item = new StatusNotifierItem(id, m_connection, this);
    m_items << item;
    item->refresh();
    Q_EMIT itemAdded(item);
}

void StatusNotifierHost::removeItem(const QString &id)
{
    for (StatusNotifierItem *item : std::as_const(m_items)) {
        if (item->id() == id) {
            m_items.removeOne(item);
            Q_EMIT itemRemoved(item);
            item->deleteLater();
            return;
        }
    }
}

} // namespace Daytona
