// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings.h"

#include <QPoint>
#include <QRect>
#include <QSize>

namespace Daytona {

// Pure geometry for docking the taskbar to a screen edge.

inline bool isVertical(TaskbarEdge edge)
{
    return edge == TaskbarEdge::Left || edge == TaskbarEdge::Right;
}

// The edge nearest to a point on the screen, as NT snaps a dragged taskbar.
// The screen is split along its diagonals, so each edge owns the triangle
// next to it. Ties go to the bottom, then top, left, right.
TaskbarEdge nearestEdge(const QPoint &point, const QSize &screen);

// Where the taskbar's surface sits on the screen when docked to edge with
// the given thickness (height when horizontal, width when vertical).
QRect taskbarRect(TaskbarEdge edge, const QSize &screen, int thickness);

// A position local to the taskbar's surface, on the screen. Layer-shell
// clients are not told where their surfaces are, and Qt reports layer
// surface positions as local ones (see docs/taskbar-placement.md).
inline QPoint taskbarToScreen(
    TaskbarEdge edge, const QSize &screen, int thickness, const QPoint &local)
{
    return taskbarRect(edge, screen, thickness).topLeft() + local;
}

// Taskbar thickness when resized (AGENT.md -> Taskbar Placement).
struct TaskbarThickness
{
    // A horizontal taskbar's height for a number of rows: one row is 28
    // pixels, each further row adds a 22 pixel button and 3 pixels between.
    static constexpr int FirstRow = 28;
    static constexpr int RowStep = 25;
    static int forRows(int rows) { return FirstRow + (qMax(1, rows) - 1) * RowStep; }
};

// Rows for a dragged height, snapped to the nearest whole row, from one to
// the most that fit in half the screen height.
int snapTaskbarRows(int height, int screenHeight);
int maxTaskbarRows(int screenHeight);

// A dragged vertical taskbar width: any width from minimum to half the
// screen width.
int clampTaskbarWidth(int width, int minimum, int screenWidth);

// The thickness a drag of the inner edge to a point on the screen asks for:
// the distance from the taskbar's outer screen edge.
int draggedThickness(TaskbarEdge edge, const QSize &screen, const QPoint &point);

} // namespace Daytona
