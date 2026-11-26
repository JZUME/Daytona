// SPDX-License-Identifier: GPL-3.0-or-later

#include "battery.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBattery, "daytona.battery", QtInfoMsg)

namespace Daytona {

namespace {
    const QString Service = QStringLiteral("org.freedesktop.UPower");
    const QString DisplayDevice = QStringLiteral("/org/freedesktop/UPower/devices/DisplayDevice");
    const QString DeviceInterface = QStringLiteral("org.freedesktop.UPower.Device");
    const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
    // UPower's Device.Type for a laptop battery.
    constexpr uint TypeBattery = 2;

    QString duration(qint64 seconds)
    {
        const qint64 minutes = (seconds + 30) / 60;
        if (minutes < 60)
            return QObject::tr("%n min", nullptr, int(minutes));
        return QObject::tr("%1 hr %2 min").arg(minutes / 60).arg(minutes % 60);
    }
} // namespace

QString BatteryState::iconName() const
{
    if (charge == Charge::FullyCharged)
        return QStringLiteral("battery-level-100-charged");
    // Chicago95's empty battery is crossed out, as if missing; below a
    // tenth its battery with an exclamation mark says it better.
    const int tenth = qBound(0, percent, 100) / 10 * 10;
    QString name = tenth == 0 ? QStringLiteral("battery-caution")
                              : QStringLiteral("battery-level-%1").arg(tenth);
    if (!onBattery())
        name += QLatin1String("-charging");
    return name;
}

QString BatteryState::toolTip() const
{
    switch (charge) {
    case Charge::FullyCharged:
        return QObject::tr("Fully charged (%1%)").arg(percent);
    case Charge::Charging:
        return QObject::tr("%1% available (plugged in, charging)").arg(percent);
    case Charge::PendingCharge:
    case Charge::Unknown:
        return QObject::tr("%1% available (plugged in, not charging)").arg(percent);
    case Charge::Discharging:
    case Charge::Empty:
    case Charge::PendingDischarge:
        break;
    }
    if (timeToEmpty > 0)
        return QObject::tr("%1 (%2%) remaining").arg(duration(timeToEmpty)).arg(percent);
    return QObject::tr("%1% remaining").arg(percent);
}

BatteryMonitor::BatteryMonitor(QObject *parent)
    : QObject(parent)
{
}

bool BatteryMonitor::start()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
        return false;
    bus.connect(Service, DisplayDevice, PropertiesInterface, QStringLiteral("PropertiesChanged"),
        this, SLOT(refresh()));
    // UPower starting (or restarting) after the shell.
    auto *watcher
        = new QDBusServiceWatcher(Service, bus, QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &BatteryMonitor::refresh);
    refresh();
    return true;
}

void BatteryMonitor::refresh()
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        Service, DisplayDevice, PropertiesInterface, QStringLiteral("GetAll"));
    call << DeviceInterface;
    auto *watcher
        = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;
        BatteryState state;
        if (reply.isError()) {
            // No UPower: treated as no battery.
            qCDebug(lcBattery) << "Cannot read UPower:" << reply.error().message();
        } else {
            const QVariantMap p = reply.value();
            state.present = p.value(QStringLiteral("IsPresent")).toBool()
                && p.value(QStringLiteral("Type")).toUInt() == TypeBattery;
            state.percent = qRound(p.value(QStringLiteral("Percentage")).toDouble());
            state.charge = BatteryState::Charge(p.value(QStringLiteral("State")).toUInt());
            state.timeToEmpty = p.value(QStringLiteral("TimeToEmpty")).toLongLong();
            state.timeToFull = p.value(QStringLiteral("TimeToFull")).toLongLong();
            state.warning = BatteryState::Warning(p.value(QStringLiteral("WarningLevel")).toUInt());
        }
        setState(state);
    });
}

void BatteryMonitor::setState(const BatteryState &state)
{
    const BatteryState::Warning before = m_state.warning;
    m_state = state;
    Q_EMIT changed();
    if (state.present && state.onBattery() && state.warning >= BatteryState::Warning::Low
        && state.warning > before)
        Q_EMIT lowBattery(state);
}

} // namespace Daytona
