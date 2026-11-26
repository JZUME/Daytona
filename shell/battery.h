// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace Daytona {

// The system's batteries as UPower's display device sums them up.
struct BatteryState
{
    // UPower's Device.State.
    enum class Charge
    {
        Unknown = 0,
        Charging = 1,
        Discharging = 2,
        Empty = 3,
        FullyCharged = 4,
        PendingCharge = 5,
        PendingDischarge = 6
    };
    // UPower's Device.WarningLevel.
    enum class Warning
    {
        Unknown = 0,
        None = 1,
        Discharging = 2,
        Low = 3,
        Critical = 4,
        Action = 5
    };

    bool present = false;
    int percent = 0;
    Charge charge = Charge::Unknown;
    qint64 timeToEmpty = 0; // seconds, 0 if unknown
    qint64 timeToFull = 0;
    Warning warning = Warning::Unknown;

    bool onBattery() const
    {
        return charge == Charge::Discharging || charge == Charge::Empty
            || charge == Charge::PendingDischarge;
    }

    // The Chicago95 icon: a battery filled to the nearest tenth below (a
    // warning sign below 10%), with a plug while charging.
    QString iconName() const;
    // As Windows XP's power meter: "1 hr 23 min (45%) remaining" or "45%
    // available (plugged in, charging)".
    QString toolTip() const;
};

// Follows UPower's display device over the system bus. Nothing is polled:
// UPower's PropertiesChanged signal prompts a fresh read.
class BatteryMonitor : public QObject
{
    Q_OBJECT

public:
    explicit BatteryMonitor(QObject *parent = nullptr);

    // Reads the state now and whenever it changes. Returns false without a
    // system bus.
    bool start();

    BatteryState state() const { return m_state; }

    // Applies a state read from UPower (or a test), emitting changed and,
    // when the battery has just become low or critical, lowBattery.
    void setState(const BatteryState &state);

Q_SIGNALS:
    void changed();
    // The warning level rose to Low or beyond while on battery.
    void lowBattery(const Daytona::BatteryState &state);

private Q_SLOTS:
    void refresh();

private:
    BatteryState m_state;
};

} // namespace Daytona
