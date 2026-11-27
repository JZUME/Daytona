// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QStringList>
#include <functional>

namespace Daytona {

// The system's date, time, time zone and network time, through
// systemd-timedated (org.freedesktop.timedate1). Changes are asynchronous
// and allow interactive authentication, so the polkit agent can ask for an
// administrator's password.
class TimeDate : public QObject
{
    Q_OBJECT

public:
    // The system bus in the session; a private bus in tests.
    explicit TimeDate(const QDBusConnection &bus, QObject *parent = nullptr);

    // Reads timedated's properties (starting it if need be) and follows
    // their changes.
    void start();

    bool isAvailable() const { return m_available; }
    QString timeZone() const { return m_timeZone; }
    // Whether network time is on, and whether it can be turned on at all
    // (a time synchronisation service is installed).
    bool ntp() const { return m_ntp; }
    bool canNtp() const { return m_canNtp; }

    // Each calls done with an empty string on success, else the error.
    using Done = std::function<void(const QString &error)>;
    void setTimeZone(const QString &zone, const Done &done);
    void setNtp(bool on, const Done &done);
    // Sets the clock to a moment, in milliseconds since the epoch.
    void setTime(qint64 msecsSinceEpoch, const Done &done);

    // Makes the C library (and so QDateTime's local time) read the system
    // time zone again: glibc keeps the zone it read first while TZ is
    // unchanged, even after /etc/localtime changes.
    static void reloadLocalTimeZone();

    // The time zones to choose from: the names in tzdata's zone1970.tab (or
    // zone.tab), sorted, plus UTC.
    static QStringList availableZones(
        const QString &zoneInfoDirectory = QStringLiteral("/usr/share/zoneinfo"));

Q_SIGNALS:
    // The properties were read again.
    void changed();
    // The system time zone changed, here or elsewhere (timedatectl).
    void timeZoneChanged(const QString &zone);

private Q_SLOTS:
    void onPropertiesChanged(
        const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    void readProperties();
    void call(const QString &method, const QVariantList &arguments, const Done &done);

    QDBusConnection m_bus;
    bool m_available = false;
    QString m_timeZone;
    bool m_ntp = false;
    bool m_canNtp = false;
};

} // namespace Daytona
