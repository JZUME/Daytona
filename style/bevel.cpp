// SPDX-License-Identifier: GPL-3.0-or-later

#include "bevel.h"

#include <QPainter>

namespace Daytona::Bevel {

void edge(QPainter *painter, const QRect &rect, const QColor &topLeft, const QColor &bottomRight)
{
    if (rect.width() < 1 || rect.height() < 1)
        return;
    const int x = rect.x();
    const int y = rect.y();
    const int w = rect.width();
    const int h = rect.height();
    painter->fillRect(QRect(x, y, w - 1, 1), topLeft);
    painter->fillRect(QRect(x, y, 1, h - 1), topLeft);
    painter->fillRect(QRect(x, y + h - 1, w, 1), bottomRight);
    painter->fillRect(QRect(x + w - 1, y, 1, h), bottomRight);
}

void raised(QPainter *painter, const QRect &rect, const QPalette &palette)
{
    edge(painter, rect, palette.color(QPalette::Light), palette.color(QPalette::Shadow));
    edge(painter, rect.adjusted(1, 1, -1, -1), palette.color(QPalette::Midlight),
        palette.color(QPalette::Dark));
}

void pressed(QPainter *painter, const QRect &rect, const QPalette &palette)
{
    edge(painter, rect, palette.color(QPalette::Shadow), palette.color(QPalette::Light));
    edge(painter, rect.adjusted(1, 1, -1, -1), palette.color(QPalette::Dark),
        palette.color(QPalette::Midlight));
}

void sunken(QPainter *painter, const QRect &rect, const QPalette &palette)
{
    edge(painter, rect, palette.color(QPalette::Dark), palette.color(QPalette::Light));
    edge(painter, rect.adjusted(1, 1, -1, -1), palette.color(QPalette::Shadow),
        palette.color(QPalette::Midlight));
}

void raisedThin(QPainter *painter, const QRect &rect, const QPalette &palette)
{
    edge(painter, rect, palette.color(QPalette::Light), palette.color(QPalette::Dark));
}

void sunkenThin(QPainter *painter, const QRect &rect, const QPalette &palette)
{
    edge(painter, rect, palette.color(QPalette::Dark), palette.color(QPalette::Light));
}

void focusRect(QPainter *painter, const QRect &rect, const QColor &colour)
{
    if (rect.width() < 1 || rect.height() < 1)
        return;
    const int left = rect.left();
    const int top = rect.top();
    const int right = rect.right();
    const int bottom = rect.bottom();
    // Pixels whose (x + y) is even are drawn, so the dots continue around
    // corners without doubling up.
    for (int x = left; x <= right; ++x) {
        if ((x + top) % 2 == 0)
            painter->fillRect(QRect(x, top, 1, 1), colour);
        if ((x + bottom) % 2 == 0 && bottom != top)
            painter->fillRect(QRect(x, bottom, 1, 1), colour);
    }
    for (int y = top + 1; y < bottom; ++y) {
        if ((left + y) % 2 == 0)
            painter->fillRect(QRect(left, y, 1, 1), colour);
        if ((right + y) % 2 == 0 && right != left)
            painter->fillRect(QRect(right, y, 1, 1), colour);
    }
}

} // namespace Daytona::Bevel
