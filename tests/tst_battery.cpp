// SPDX-License-Identifier: GPL-3.0-or-later

#include "battery.h"
#include "batteryicon.h"
#include "notificationarea.h"

#include <QSignalSpy>
#include <QTest>

using namespace Daytona;

namespace {

BatteryState battery(int percent, BatteryState::Charge charge,
    BatteryState::Warning warning = BatteryState::Warning::None)
{
    BatteryState state;
    state.present = true;
    state.percent = percent;
    state.charge = charge;
    state.warning = warning;
    return state;
}

} // namespace

class TestBattery : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void iconNames()
    {
        using Charge = BatteryState::Charge;
        QCOMPARE(battery(45, Charge::Discharging).iconName(), QStringLiteral("battery-level-40"));
        QCOMPARE(battery(7, Charge::Discharging).iconName(), QStringLiteral("battery-caution"));
        QCOMPARE(
            battery(7, Charge::Charging).iconName(), QStringLiteral("battery-caution-charging"));
        QCOMPARE(
            battery(45, Charge::Charging).iconName(), QStringLiteral("battery-level-40-charging"));
        QCOMPARE(battery(100, Charge::Discharging).iconName(), QStringLiteral("battery-level-100"));
        QCOMPARE(battery(100, Charge::FullyCharged).iconName(),
            QStringLiteral("battery-level-100-charged"));
        // Plugged in but not charging (a charge limit) still shows the plug.
        QCOMPARE(battery(80, Charge::PendingCharge).iconName(),
            QStringLiteral("battery-level-80-charging"));
    }

    void toolTips()
    {
        using Charge = BatteryState::Charge;
        BatteryState state = battery(45, Charge::Discharging);
        QCOMPARE(state.toolTip(), QStringLiteral("45% remaining"));
        state.timeToEmpty = 83 * 60;
        QCOMPARE(state.toolTip(), QStringLiteral("1 hr 23 min (45%) remaining"));
        state.timeToEmpty = 40 * 60;
        QCOMPARE(state.toolTip(), QStringLiteral("40 min (45%) remaining"));
        QCOMPARE(battery(45, Charge::Charging).toolTip(),
            QStringLiteral("45% available (plugged in, charging)"));
        QCOMPARE(battery(80, Charge::PendingCharge).toolTip(),
            QStringLiteral("80% available (plugged in, not charging)"));
        QCOMPARE(
            battery(100, Charge::FullyCharged).toolTip(), QStringLiteral("Fully charged (100%)"));
    }

    void lowBatteryWarnsOncePerLevel()
    {
        using Charge = BatteryState::Charge;
        using Warning = BatteryState::Warning;
        BatteryMonitor monitor;
        QSignalSpy spy(&monitor, &BatteryMonitor::lowBattery);
        monitor.setState(battery(50, Charge::Discharging));
        monitor.setState(battery(10, Charge::Discharging, Warning::Low));
        QCOMPARE(spy.count(), 1);
        monitor.setState(battery(9, Charge::Discharging, Warning::Low));
        QCOMPARE(spy.count(), 1);
        monitor.setState(battery(3, Charge::Discharging, Warning::Critical));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().first().value<BatteryState>().warning, Warning::Critical);
        // Plugged in, then unplugged again while still low: warned again.
        monitor.setState(battery(4, Charge::Charging, Warning::None));
        monitor.setState(battery(4, Charge::Discharging, Warning::Critical));
        QCOMPARE(spy.count(), 3);
        // Never while charging.
        monitor.setState(battery(4, Charge::Charging, Warning::None));
        monitor.setState(battery(4, Charge::Charging, Warning::Low));
        QCOMPARE(spy.count(), 3);
    }

    void iconOnlyWithBattery()
    {
        BatteryMonitor monitor;
        NotificationArea area;
        area.setBatteryMonitor(&monitor);
        area.show();
        BatteryIcon *icon = area.batteryIcon();
        QVERIFY(icon);
        QVERIFY(icon->isHidden());
        monitor.setState(battery(45, BatteryState::Charge::Charging));
        QVERIFY(!icon->isHidden());
        QCOMPARE(icon->toolTip(), QStringLiteral("45% available (plugged in, charging)"));
        monitor.setState({});
        QVERIFY(icon->isHidden());
    }
};

QTEST_MAIN(TestBattery)
#include "tst_battery.moc"
