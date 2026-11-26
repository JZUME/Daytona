// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QHBoxLayout;

namespace Daytona {

class BatteryIcon;
class BatteryMonitor;
class Clock;
class StatusNotifierHost;
class StatusNotifierItem;
class TrayIcon;
class VolumeControl;
class VolumeIcon;

// The sunken area at the right of the taskbar. Tray icons are added to its
// left; the clock is always rightmost.
class NotificationArea : public QWidget
{
    Q_OBJECT

public:
    explicit NotificationArea(QWidget *parent = nullptr);

    Clock *clock() const { return m_clock; }

    // Shows the host's tray icons, in registration order, left of the clock.
    void setHost(StatusNotifierHost *host);
    QList<TrayIcon *> trayIcons() const;

    // Shows the speaker icon for control, between the tray icons and the
    // clock.
    void setVolumeControl(VolumeControl *control);
    VolumeIcon *volumeIcon() const { return m_volumeIcon; }

    // Shows the battery icon for monitor, left of the speaker, while the
    // machine has a battery.
    void setBatteryMonitor(BatteryMonitor *monitor);
    BatteryIcon *batteryIcon() const { return m_batteryIcon; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void addItem(StatusNotifierItem *item);
    void removeItem(StatusNotifierItem *item);

    QHBoxLayout *m_layout;
    Clock *m_clock;
    // The first of the shell's own icons, which stay right of tray icons.
    QWidget *firstOwnIcon() const;

    VolumeIcon *m_volumeIcon = nullptr;
    BatteryIcon *m_batteryIcon = nullptr;
};

} // namespace Daytona
