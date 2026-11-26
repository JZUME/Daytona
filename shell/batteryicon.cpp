// SPDX-License-Identifier: GPL-3.0-or-later

#include "batteryicon.h"

#include "battery.h"

#include <QIcon>
#include <QPainter>

namespace Daytona {

namespace {
    // As TrayIcon: 16x16 with a 2 pixel gap.
    constexpr int IconSize = 16;
    constexpr int Gap = 2;
} // namespace

BatteryIcon::BatteryIcon(BatteryMonitor *monitor, QWidget *parent)
    : QWidget(parent)
    , m_monitor(monitor)
{
    setAttribute(Qt::WA_AlwaysShowToolTips);
    connect(monitor, &BatteryMonitor::changed, this, &BatteryIcon::stateChanged);
    stateChanged();
}

QSize BatteryIcon::sizeHint() const
{
    return { IconSize + Gap, IconSize };
}

void BatteryIcon::stateChanged()
{
    const BatteryState state = m_monitor->state();
    setVisible(state.present);
    setToolTip(state.toolTip());
    setAccessibleName(tr("Power: %1").arg(state.toolTip()));
    update();
}

void BatteryIcon::paintEvent(QPaintEvent *)
{
    const BatteryState state = m_monitor->state();
    const QIcon icon
        = QIcon::fromTheme(state.iconName(), QIcon::fromTheme(QStringLiteral("battery")));
    QPainter painter(this);
    icon.paint(&painter, QRect(Gap / 2, (height() - IconSize) / 2, IconSize, IconSize));
}

} // namespace Daytona
