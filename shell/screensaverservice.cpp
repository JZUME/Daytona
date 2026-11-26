// SPDX-License-Identifier: GPL-3.0-or-later

#include "screensaverservice.h"

#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcScreenSaver, "daytona.screensaver", QtInfoMsg)

namespace Daytona {

namespace {

    // Plenty for real use; stops one program from growing the table forever.
    constexpr int MaxInhibits = 256;

    const QStringList &objectPaths()
    {
        static const QStringList paths { QStringLiteral("/org/freedesktop/ScreenSaver"),
            QStringLiteral("/ScreenSaver") };
        return paths;
    }

} // namespace

ScreenSaverService::ScreenSaverService(QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
    , m_watcher(new QDBusServiceWatcher(
          QString(), connection, QDBusServiceWatcher::WatchForUnregistration, this))
{
    connect(
        m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &name) {
            m_watcher->removeWatchedService(name);
            remove(m_inhibits.keys(name));
        });
}

bool ScreenSaverService::registerOnBus()
{
    if (!m_connection.isConnected())
        return false;
    for (const QString &path : objectPaths()) {
        if (!m_connection.registerObject(path, this, QDBusConnection::ExportScriptableSlots)) {
            for (const QString &registered : objectPaths())
                m_connection.unregisterObject(registered);
            return false;
        }
    }
    if (!m_connection.registerService(QString::fromLatin1(ServiceName))) {
        for (const QString &path : objectPaths())
            m_connection.unregisterObject(path);
        return false;
    }
    return true;
}

uint ScreenSaverService::Inhibit(const QString &applicationName, const QString &reason)
{
    const QString caller = calledFromDBus() ? message().service() : QString();
    if (m_inhibits.size() >= MaxInhibits) {
        if (calledFromDBus())
            sendErrorReply(QDBusError::LimitsExceeded, QStringLiteral("Too many inhibits"));
        return 0;
    }
    uint cookie = m_nextCookie++;
    while (cookie == 0 || m_inhibits.contains(cookie))
        cookie = m_nextCookie++;
    const bool wasInhibited = isInhibited();
    m_inhibits.insert(cookie, caller);
    if (!caller.isEmpty())
        m_watcher->addWatchedService(caller);
    qCInfo(lcScreenSaver).noquote()
        << "Screen saver inhibited by" << applicationName.left(64) << "-" << reason.left(128);
    if (!wasInhibited)
        Q_EMIT inhibitedChanged(true);
    return cookie;
}

void ScreenSaverService::UnInhibit(uint cookie)
{
    // Only the program that took an inhibit may end it.
    const QString caller = calledFromDBus() ? message().service() : QString();
    const auto found = m_inhibits.constFind(cookie);
    if (found == m_inhibits.constEnd() || found.value() != caller)
        return;
    remove({ cookie });
    if (!caller.isEmpty() && m_inhibits.keys(caller).isEmpty())
        m_watcher->removeWatchedService(caller);
}

void ScreenSaverService::remove(const QList<uint> &cookies)
{
    if (cookies.isEmpty())
        return;
    for (uint cookie : cookies)
        m_inhibits.remove(cookie);
    if (!isInhibited()) {
        qCInfo(lcScreenSaver) << "Screen saver no longer inhibited";
        Q_EMIT inhibitedChanged(false);
    }
}

} // namespace Daytona
