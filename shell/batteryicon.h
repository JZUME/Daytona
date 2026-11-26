// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>

namespace Daytona {

class BatteryMonitor;

// Windows 2000's power meter in the notification area: the battery's
// charge, with a plug while on mains power, and the charge and time left in
// its tooltip. It is hidden on machines without a battery.
class BatteryIcon : public QWidget
{
    Q_OBJECT

public:
    explicit BatteryIcon(BatteryMonitor *monitor, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void stateChanged();

    BatteryMonitor *m_monitor;
};

} // namespace Daytona
