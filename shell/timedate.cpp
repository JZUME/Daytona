// SPDX-License-Identifier: GPL-3.0-or-later

#include "timedate.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QFile>
#include <QLoggingCategory>
#include <QSet>
#include <ctime>

Q_LOGGING_CATEGORY(lcTimeDate, "daytona.timedate", QtInfoMsg)

namespace Daytona {

namespace {

    const QString Service = QStringLiteral("org.freedesktop.timedate1");
    const QString Path = QStringLiteral("/org/freedesktop/timedate1");
    const QString Interface = QStringLiteral("org.freedesktop.timedate1");
    const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

    // Long enough for someone to type a password into the polkit agent.
    constexpr int InteractiveTimeoutMs = 10 * 60 * 1000;

} // namespace

TimeDate::TimeDate(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
{
}

void TimeDate::start()
{
    m_bus.connect(Service, Path, PropertiesInterface, QStringLiteral("PropertiesChanged"), this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    readProperties();
}

void TimeDate::readProperties()
{
    QDBusMessage get = QDBusMessage::createMethodCall(
        Service, Path, PropertiesInterface, QStringLiteral("GetAll"));
    get << Interface;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(get), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isError()) {
            qCWarning(lcTimeDate) << "Cannot read the time settings from timedated:"
                                  << reply.error().message();
            m_available = false;
            Q_EMIT changed();
            return;
        }
        const QVariantMap properties = reply.value();
        const QString oldZone = m_timeZone;
        m_available = true;
        m_timeZone = properties.value(QStringLiteral("Timezone")).toString();
        m_ntp = properties.value(QStringLiteral("NTP")).toBool();
        m_canNtp = properties.value(QStringLiteral("CanNTP")).toBool();
        Q_EMIT changed();
        if (!oldZone.isEmpty() && oldZone != m_timeZone)
            Q_EMIT timeZoneChanged(m_timeZone);
    });
}

void TimeDate::onPropertiesChanged(
    const QString &interface, const QVariantMap &, const QStringList &)
{
    // Read them all again rather than trust which ones a signal carries.
    if (interface == Interface)
        readProperties();
}

void TimeDate::call(const QString &method, const QVariantList &arguments, const Done &done)
{
    QDBusMessage message = QDBusMessage::createMethodCall(Service, Path, Interface, method);
    message.setArguments(arguments);
    message.setInteractiveAuthorizationAllowed(true);
    auto *watcher
        = new QDBusPendingCallWatcher(m_bus.asyncCall(message, InteractiveTimeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, method, done](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            const QDBusPendingReply<> reply = *w;
            QString error;
            if (reply.isError()) {
                error = reply.error().message();
                qCWarning(lcTimeDate) << "timedated refused" << method << error;
            }
            // Pick up the new state even when a signal does not arrive.
            readProperties();
            if (done)
                done(error);
        });
}

void TimeDate::setTimeZone(const QString &zone, const Done &done)
{
    call(QStringLiteral("SetTimezone"), { zone, true }, done);
}

void TimeDate::setNtp(bool on, const Done &done)
{
    call(QStringLiteral("SetNTP"), { on, true }, done);
}

void TimeDate::setTime(qint64 msecsSinceEpoch, const Done &done)
{
    // Microseconds since the epoch, absolute (not relative), interactive.
    call(QStringLiteral("SetTime"), { qlonglong(msecsSinceEpoch * 1000), false, true }, done);
}

void TimeDate::reloadLocalTimeZone()
{
    // glibc reads the zone again only when TZ differs from last time, so
    // change it briefly and put it back.
    const QByteArray tz = qgetenv("TZ");
    const bool wasSet = qEnvironmentVariableIsSet("TZ");
    qputenv("TZ", "UTC0");
    tzset();
    if (wasSet)
        qputenv("TZ", tz);
    else
        qunsetenv("TZ");
    tzset();
}

QStringList TimeDate::availableZones(const QString &zoneInfoDirectory)
{
    QSet<QString> zones;
    for (const QString &name : { QStringLiteral("zone1970.tab"), QStringLiteral("zone.tab") }) {
        QFile file(zoneInfoDirectory + QLatin1Char('/') + name);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        while (!file.atEnd()) {
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            // Country codes, coordinates, zone name, comment.
            const QStringList fields = line.split(QLatin1Char('\t'));
            if (fields.size() >= 3 && !fields.at(2).isEmpty())
                zones.insert(fields.at(2));
        }
        if (!zones.isEmpty())
            break;
    }
    zones.insert(QStringLiteral("UTC"));
    QStringList sorted(zones.begin(), zones.end());
    sorted.sort();
    return sorted;
}

} // namespace Daytona
