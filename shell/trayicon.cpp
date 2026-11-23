// SPDX-License-Identifier: GPL-3.0-or-later

#include "trayicon.h"

#include "statusnotifieritem.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTextDocument>

namespace Daytona {

namespace {
    // NT draws tray icons at 16x16 with a 2 pixel gap.
    constexpr int IconSize = 16;
    constexpr int Gap = 2;
} // namespace

TrayIcon::TrayIcon(StatusNotifierItem *item, QWidget *parent)
    : QWidget(parent)
    , m_item(item)
{
    setAttribute(Qt::WA_AlwaysShowToolTips);
    connect(item, &StatusNotifierItem::changed, this, &TrayIcon::itemChanged);
    itemChanged();
}

QSize TrayIcon::sizeHint() const
{
    return { IconSize + Gap, IconSize };
}

void TrayIcon::itemChanged()
{
    if (!m_item)
        return;
    // Passive items are hidden, as the specification asks.
    setVisible(m_item->isReady() && m_item->status() != StatusNotifierItem::Status::Passive);
    QString tip = m_item->toolTipTitle();
    if (tip.isEmpty())
        tip = m_item->title();
    // Shown as plain text, whatever markup the application sends.
    setToolTip(tip.isEmpty() ? QString() : Qt::convertFromPlainText(tip, Qt::WhiteSpaceNormal));
    setAccessibleName(tip);
    update();
}

void TrayIcon::paintEvent(QPaintEvent *)
{
    if (!m_item)
        return;
    const qreal ratio = devicePixelRatioF();
    QPixmap pixmap = m_item->pixmap(qRound(IconSize * ratio));
    if (pixmap.isNull())
        return;
    pixmap.setDevicePixelRatio(ratio);
    QPainter painter(this);
    const QRect target(Gap / 2, (height() - IconSize) / 2, IconSize, IconSize);
    painter.drawPixmap(target, pixmap);
}

void TrayIcon::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_item || !rect().contains(event->position().toPoint()))
        return;
    const QPoint global = event->globalPosition().toPoint();
    switch (event->button()) {
    case Qt::LeftButton:
        if (m_item->itemIsMenu())
            m_item->showMenu(global, this);
        else
            m_item->activate(global);
        break;
    case Qt::MiddleButton:
        m_item->secondaryActivate(global);
        break;
    case Qt::RightButton:
        m_item->showMenu(global, this);
        break;
    default:
        break;
    }
}

} // namespace Daytona
