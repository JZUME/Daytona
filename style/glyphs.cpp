// SPDX-License-Identifier: GPL-3.0-or-later

#include "glyphs.h"

#include <QPainter>
#include <QRect>
#include <cmath>

namespace Daytona::Glyph {

namespace {

    // '#' marks a set pixel. Original pixel art in the NT manner.
    const char *const CheckRows[] = {
        "......#",
        ".....##",
        "#...###",
        "##.###.",
        "#####..",
        ".###...",
        "..#....",
    };

    const char *const BulletRows[] = {
        ".####.",
        "######",
        "######",
        "######",
        "######",
        ".####.",
    };

    const char *const ArrowRows[] = {
        "#...",
        "##..",
        "###.",
        "####",
        "###.",
        "##..",
        "#...",
    };

    struct Bitmap
    {
        const char *const *rows;
        int height;
    };

    Bitmap bitmap(Kind kind)
    {
        switch (kind) {
        case Kind::Check:
            return { CheckRows, int(std::size(CheckRows)) };
        case Kind::Bullet:
            return { BulletRows, int(std::size(BulletRows)) };
        case Kind::ArrowRight:
            return { ArrowRows, int(std::size(ArrowRows)) };
        }
        return { CheckRows, int(std::size(CheckRows)) };
    }

} // namespace

QSize size(Kind kind)
{
    const Bitmap b = bitmap(kind);
    return { int(qstrlen(b.rows[0])), b.height };
}

void draw(QPainter *painter, Kind kind, const QPoint &topLeft, const QColor &colour)
{
    const Bitmap b = bitmap(kind);
    for (int y = 0; y < b.height; ++y) {
        for (int x = 0; b.rows[y][x] != '\0'; ++x) {
            if (b.rows[y][x] == '#')
                painter->fillRect(QRect(topLeft.x() + x, topLeft.y() + y, 1, 1), colour);
        }
    }
}

void drawRadio(QPainter *painter, const QPoint &topLeft, const QColor &outerTopLeft,
    const QColor &outerBottomRight, const QColor &innerTopLeft, const QColor &innerBottomRight,
    const QColor &field, const QColor *dot)
{
    // Each pixel is classified by its distance from the centre, which gives
    // hard-edged circles that scale without blurring.
    const double centre = (RadioSize - 1) / 2.0;
    for (int y = 0; y < RadioSize; ++y) {
        for (int x = 0; x < RadioSize; ++x) {
            const double dx = x - centre;
            const double dy = y - centre;
            const double r = std::sqrt(dx * dx + dy * dy);
            // Pixels above the anti-diagonal take the top-left colour.
            const bool upperLeft = x + y < RadioSize - 1;
            QColor colour;
            if (r > 6.0) {
                continue;
            } else if (r > 5.0) {
                colour = upperLeft ? outerTopLeft : outerBottomRight;
            } else if (r > 4.0) {
                colour = upperLeft ? innerTopLeft : innerBottomRight;
            } else if (dot && r < 2.0) {
                colour = *dot;
            } else {
                colour = field;
            }
            painter->fillRect(QRect(topLeft.x() + x, topLeft.y() + y, 1, 1), colour);
        }
    }
}

} // namespace Daytona::Glyph
