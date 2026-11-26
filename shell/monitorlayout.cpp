// SPDX-License-Identifier: GPL-3.0-or-later

#include "monitorlayout.h"

#include "bevel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace Daytona {

QPoint snapMonitor(const QRect &moving, const QList<QRect> &others, int snap)
{
    const QPoint dropped = moving.topLeft();
    const int w = moving.width();
    const int h = moving.height();
    // Within the range that keeps part of an edge shared, nearest to where
    // it was dropped, lined up with near and far edges when close to them.
    const auto place = [snap](int wanted, int start, int length, int size) {
        int value = std::clamp(wanted, start - size + 1, start + length - 1);
        if (std::abs(value - start) <= snap)
            value = start;
        else if (std::abs(value + size - (start + length)) <= snap)
            value = start + length - size;
        return value;
    };
    QList<QPoint> candidates;
    for (const QRect &other : others) {
        const int y = place(dropped.y(), other.y(), other.height(), h);
        const int x = place(dropped.x(), other.x(), other.width(), w);
        candidates.append({ other.x() - w, y }); // to its left
        candidates.append({ other.x() + other.width(), y }); // to its right
        candidates.append({ x, other.y() - h }); // above it
        candidates.append({ x, other.y() + other.height() }); // below it
    }
    QPoint best = dropped;
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    for (const QPoint &candidate : std::as_const(candidates)) {
        const QRect placed(candidate, moving.size());
        const bool overlaps = std::any_of(others.begin(), others.end(),
            [&placed](const QRect &other) { return other.intersects(placed); });
        if (overlaps)
            continue;
        const qint64 dx = candidate.x() - dropped.x();
        const qint64 dy = candidate.y() - dropped.y();
        const qint64 distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }
    return best;
}

QList<QRect> normaliseMonitors(const QList<QRect> &monitors)
{
    if (monitors.isEmpty())
        return monitors;
    int left = std::numeric_limits<int>::max();
    int top = std::numeric_limits<int>::max();
    for (const QRect &monitor : monitors) {
        left = std::min(left, monitor.x());
        top = std::min(top, monitor.y());
    }
    QList<QRect> result;
    for (const QRect &monitor : monitors)
        result.append(monitor.translated(-left, -top));
    return result;
}

QList<QRect> resizeMonitor(const QList<QRect> &monitors, int index, const QSize &size)
{
    if (index < 0 || index >= monitors.size())
        return monitors;
    const QRect old = monitors.at(index);
    const int dx = size.width() - old.width();
    const int dy = size.height() - old.height();
    const int right = old.x() + old.width();
    const int bottom = old.y() + old.height();
    QList<QRect> result = monitors;
    for (int i = 0; i < result.size(); ++i) {
        if (i == index)
            continue;
        QRect &monitor = result[i];
        // Beyond its right or bottom edge: moves with that edge.
        const int x = monitor.x() >= right ? monitor.x() + dx : monitor.x();
        const int y = monitor.y() >= bottom ? monitor.y() + dy : monitor.y();
        monitor.moveTo(x, y);
    }
    result[index].setSize(size);
    return result;
}

MonitorArrangement::MonitorArrangement(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(110);
    setFocusPolicy(Qt::StrongFocus);
}

void MonitorArrangement::setMonitors(const QList<Monitor> &monitors)
{
    m_monitors = monitors;
    m_dragging = -1;
    if (m_selected >= m_monitors.size())
        m_selected = m_monitors.isEmpty() ? -1 : 0;
    updateScale();
    update();
}

void MonitorArrangement::setSelected(int index)
{
    if (index < -1 || index >= m_monitors.size() || index == m_selected)
        return;
    m_selected = index;
    update();
    Q_EMIT selectedChanged(index);
}

void MonitorArrangement::setDesktopColour(const QColor &colour)
{
    m_desktop = colour;
    update();
}

QSize MonitorArrangement::sizeHint() const
{
    return { 360, 140 };
}

QList<QRect> MonitorArrangement::drawnGeometry() const
{
    QList<QRect> result;
    QRect desktop;
    int tallest = 0;
    for (const Monitor &monitor : m_monitors) {
        if (monitor.enabled)
            desktop = desktop.united(monitor.geometry);
        tallest = std::max(tallest, monitor.geometry.height());
    }
    // Switched-off monitors go in a row below the desktop.
    const int gap = std::max(1, tallest / 5);
    int x = desktop.isNull() ? 0 : desktop.x();
    const int y = desktop.isNull() ? 0 : desktop.y() + desktop.height() + gap;
    for (const Monitor &monitor : m_monitors) {
        if (monitor.enabled) {
            result.append(monitor.geometry);
            continue;
        }
        result.append(QRect(QPoint(x, y), monitor.geometry.size()));
        x += monitor.geometry.width() + gap;
    }
    return result;
}

void MonitorArrangement::updateScale()
{
    QRect bounds;
    for (const QRect &rect : drawnGeometry())
        bounds = bounds.united(rect);
    m_bounds = bounds;
    if (bounds.isEmpty()) {
        m_scale = 0.1;
        m_origin = {};
        return;
    }
    // Room around the desktop, so a monitor can be dragged beside it.
    const QRectF area = QRectF(rect()).adjusted(12, 12, -12, -12);
    m_scale = std::min(area.width() / (bounds.width() * 1.6), area.height() / bounds.height());
    m_scale = std::max(m_scale, 0.001);
    const QSizeF drawn = QSizeF(bounds.size()) * m_scale;
    m_origin = area.center() - QPointF(drawn.width() / 2, drawn.height() / 2)
        - QPointF(bounds.topLeft()) * m_scale;
}

QRect MonitorArrangement::monitorRect(int index) const
{
    const QList<QRect> geometry = drawnGeometry();
    if (index < 0 || index >= geometry.size())
        return {};
    const QRect &desktop = geometry.at(index);
    QRect rect
        = QRectF(m_origin + QPointF(desktop.topLeft()) * m_scale, QSizeF(desktop.size()) * m_scale)
              .toRect();
    if (index == m_dragging)
        rect.translate(m_dragOffset);
    return rect;
}

int MonitorArrangement::monitorAt(const QPoint &point) const
{
    // The selected one is drawn on top, so it wins.
    if (monitorRect(m_selected).contains(point))
        return m_selected;
    for (int i = m_monitors.size() - 1; i >= 0; --i) {
        if (monitorRect(i).contains(point))
            return i;
    }
    return -1;
}

void MonitorArrangement::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QPalette &pal = palette();
    Bevel::sunkenThin(&painter, rect(), pal);

    QList<int> order;
    for (int i = 0; i < m_monitors.size(); ++i) {
        if (i != m_selected)
            order.append(i);
    }
    if (m_selected >= 0)
        order.append(m_selected);
    for (int i : std::as_const(order)) {
        const Monitor &monitor = m_monitors.at(i);
        const QRect r = monitorRect(i);
        if (r.width() < 4 || r.height() < 4)
            continue;
        const bool selected = i == m_selected;
        painter.fillRect(r, monitor.enabled ? m_desktop : pal.color(QPalette::Button));
        // Outlined in black; the selected one in the selection colour.
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::NoBrush);
        const QColor outline = selected ? pal.color(QPalette::Highlight)
            : monitor.enabled           ? QColor(Qt::black)
                                        : pal.color(QPalette::Dark);
        const int width = selected ? 3 : 1;
        painter.fillRect(r.left(), r.top(), r.width(), width, outline);
        painter.fillRect(r.left(), r.bottom() - width + 1, r.width(), width, outline);
        painter.fillRect(r.left(), r.top(), width, r.height(), outline);
        painter.fillRect(r.right() - width + 1, r.top(), width, r.height(), outline);

        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(std::clamp(r.height() * 2 / 5, 8, 48));
        painter.setFont(font);
        if (monitor.enabled) {
            painter.setPen(Qt::black);
            painter.drawText(r.translated(1, 1), Qt::AlignCenter, monitor.number);
            painter.setPen(Qt::white);
        } else {
            painter.setPen(pal.color(QPalette::Dark));
        }
        painter.drawText(r, Qt::AlignCenter, monitor.number);
    }
    if (hasFocus() && m_selected >= 0) {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.rect = monitorRect(m_selected).adjusted(-3, -3, 3, 3);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
    }
}

void MonitorArrangement::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int index = monitorAt(event->position().toPoint());
    if (index < 0)
        return;
    setSelected(index);
    if (m_monitors.at(index).enabled) {
        m_dragging = index;
        m_pressPoint = event->position().toPoint();
        m_dragOffset = {};
        m_moved = false;
    }
}

void MonitorArrangement::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging < 0)
        return;
    m_dragOffset = event->position().toPoint() - m_pressPoint;
    m_moved = m_moved || m_dragOffset.manhattanLength() >= 3;
    update();
}

void MonitorArrangement::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_dragging < 0)
        return;
    const int index = m_dragging;
    const QPoint offset = event->position().toPoint() - m_pressPoint;
    const bool moved = m_moved;
    m_dragging = -1;
    m_dragOffset = {};
    update();
    if (!moved)
        return;
    const Monitor &monitor = m_monitors.at(index);
    const QPoint dropped = monitor.geometry.topLeft() + (QPointF(offset) / m_scale).toPoint();
    QList<QRect> others;
    for (int i = 0; i < m_monitors.size(); ++i) {
        if (i != index && m_monitors.at(i).enabled)
            others.append(m_monitors.at(i).geometry);
    }
    // Within 8 drawn pixels of lining up, it lines up.
    const int snap = qRound(8 / m_scale);
    const QPoint position = snapMonitor(QRect(dropped, monitor.geometry.size()), others, snap);
    if (position != monitor.geometry.topLeft())
        Q_EMIT monitorMoved(index, position);
}

void MonitorArrangement::resizeEvent(QResizeEvent *)
{
    updateScale();
}

} // namespace Daytona
