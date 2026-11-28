// SPDX-License-Identifier: GPL-3.0-or-later

#include "trayicon.h"

#include "statusnotifieritem.h"
#include "taskbar.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
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
    const QPoint global = screenPosition(event->position().toPoint());
    switch (event->button()) {
    case Qt::LeftButton:
        if (m_item->itemIsMenu())
            m_item->showMenu(global, this);
        else
            m_item->activate(global, this);
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

QPoint TrayIcon::screenPosition(const QPoint &pos) const
{
    // Programs are told where on screen the click was, in the compositor's
    // layout, and open their menus there (X11 programs behind
    // daytona-xembed-proxy among them). Qt cannot know where a layer-shell
    // surface is, so its global positions in the taskbar are wrong: work it
    // out from the edge of the screen the taskbar is docked to. The point
    // is moved to just outside the taskbar's inner edge, so that menus open
    // beside the taskbar, as NT's did, rather than under it.
    const QWidget *bar = window();
    const auto *taskbar = qobject_cast<const Taskbar *>(bar);
    if (!taskbar || !bar->screen())
        return mapToGlobal(pos);
    const QRect screen = bar->screen()->geometry();
    QRect rect(screen.topLeft(), bar->size());
    if (taskbar->edge() == TaskbarEdge::Bottom)
        rect.moveBottom(screen.bottom());
    else if (taskbar->edge() == TaskbarEdge::Right)
        rect.moveRight(screen.right());
    QPoint point = rect.topLeft() + mapTo(bar, pos);
    switch (taskbar->edge()) {
    case TaskbarEdge::Bottom:
        point.setY(rect.top() - 1);
        break;
    case TaskbarEdge::Top:
        point.setY(rect.bottom() + 1);
        break;
    case TaskbarEdge::Left:
        point.setX(rect.right() + 1);
        break;
    case TaskbarEdge::Right:
        point.setX(rect.left() - 1);
        break;
    }
    return point;
}

void TrayIcon::contextMenuEvent(QContextMenuEvent *event)
{
    // The right button's release shows the item's menu; the taskbar's own
    // menu must not open as well.
    event->accept();
}

} // namespace Daytona
