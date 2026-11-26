// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QHBoxLayout;

namespace Daytona {

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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void addItem(StatusNotifierItem *item);
    void removeItem(StatusNotifierItem *item);

    QHBoxLayout *m_layout;
    Clock *m_clock;
    VolumeIcon *m_volumeIcon = nullptr;
};

} // namespace Daytona
