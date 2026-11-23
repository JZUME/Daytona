// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

class QHBoxLayout;

namespace Daytona {

class Clock;
class StatusNotifierHost;
class StatusNotifierItem;
class TrayIcon;

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

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void addItem(StatusNotifierItem *item);
    void removeItem(StatusNotifierItem *item);

    QHBoxLayout *m_layout;
    Clock *m_clock;
};

} // namespace Daytona
