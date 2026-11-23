// SPDX-License-Identifier: GPL-3.0-or-later

#include "notificationarea.h"

#include "bevel.h"
#include "clock.h"
#include "statusnotifierhost.h"
#include "statusnotifieritem.h"
#include "trayicon.h"

#include <QHBoxLayout>
#include <QPainter>

namespace Daytona {

NotificationArea::NotificationArea(QWidget *parent)
    : QWidget(parent)
{
    setAccessibleName(tr("Notification area"));
    m_layout = new QHBoxLayout(this);
    // Inside the 1 pixel sunken edge.
    m_layout->setContentsMargins(1, 1, 1, 1);
    m_layout->setSpacing(0);
    m_clock = new Clock(this);
    m_layout->addWidget(m_clock);
}

void NotificationArea::setHost(StatusNotifierHost *host)
{
    for (StatusNotifierItem *item : host->items())
        addItem(item);
    connect(host, &StatusNotifierHost::itemAdded, this, &NotificationArea::addItem);
    connect(host, &StatusNotifierHost::itemRemoved, this, &NotificationArea::removeItem);
}

QList<TrayIcon *> NotificationArea::trayIcons() const
{
    QList<TrayIcon *> icons;
    for (int i = 0; i < m_layout->count(); ++i) {
        if (auto *icon = qobject_cast<TrayIcon *>(m_layout->itemAt(i)->widget()))
            icons << icon;
    }
    return icons;
}

void NotificationArea::addItem(StatusNotifierItem *item)
{
    // New icons go to the right of existing ones; the clock stays last.
    m_layout->insertWidget(m_layout->indexOf(m_clock), new TrayIcon(item, this));
}

void NotificationArea::removeItem(StatusNotifierItem *item)
{
    for (TrayIcon *icon : trayIcons()) {
        if (icon->item() == item)
            delete icon;
    }
}

void NotificationArea::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    Bevel::sunkenThin(&painter, rect(), palette());
}

} // namespace Daytona
