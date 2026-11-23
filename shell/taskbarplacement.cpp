// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskbarplacement.h"

#include <algorithm>

namespace Daytona {

TaskbarEdge nearestEdge(const QPoint &point, const QSize &screen)
{
    if (screen.isEmpty())
        return TaskbarEdge::Bottom;
    // Distances to each edge, scaled so the screen counts as square: the
    // diagonals then split it into the four triangles.
    const qreal x = qBound(0.0, qreal(point.x()) / screen.width(), 1.0);
    const qreal y = qBound(0.0, qreal(point.y()) / screen.height(), 1.0);
    const qreal bottom = 1.0 - y;
    const qreal top = y;
    const qreal left = x;
    const qreal right = 1.0 - x;
    const qreal nearest = std::min({ bottom, top, left, right });
    if (bottom == nearest)
        return TaskbarEdge::Bottom;
    if (top == nearest)
        return TaskbarEdge::Top;
    if (left == nearest)
        return TaskbarEdge::Left;
    return TaskbarEdge::Right;
}

QRect taskbarRect(TaskbarEdge edge, const QSize &screen, int thickness)
{
    switch (edge) {
    case TaskbarEdge::Top:
        return { 0, 0, screen.width(), thickness };
    case TaskbarEdge::Left:
        return { 0, 0, thickness, screen.height() };
    case TaskbarEdge::Right:
        return { screen.width() - thickness, 0, thickness, screen.height() };
    case TaskbarEdge::Bottom:
        break;
    }
    return { 0, screen.height() - thickness, screen.width(), thickness };
}

int maxTaskbarRows(int screenHeight)
{
    // NT let the taskbar take up to half the screen.
    const int half = screenHeight / 2;
    return qMax(1, (half - TaskbarThickness::FirstRow) / TaskbarThickness::RowStep + 1);
}

int snapTaskbarRows(int height, int screenHeight)
{
    const qreal rows = 1.0 + qreal(height - TaskbarThickness::FirstRow) / TaskbarThickness::RowStep;
    return qBound(1, qRound(rows), maxTaskbarRows(screenHeight));
}

int clampTaskbarWidth(int width, int minimum, int screenWidth)
{
    return qBound(minimum, width, qMax(minimum, screenWidth / 2));
}

int draggedThickness(TaskbarEdge edge, const QSize &screen, const QPoint &point)
{
    switch (edge) {
    case TaskbarEdge::Top:
        return point.y();
    case TaskbarEdge::Left:
        return point.x();
    case TaskbarEdge::Right:
        return screen.width() - point.x();
    case TaskbarEdge::Bottom:
        break;
    }
    return screen.height() - point.y();
}

} // namespace Daytona
