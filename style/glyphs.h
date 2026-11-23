// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QPoint>
#include <QSize>

class QPainter;

// Small NT glyphs (menu check mark, radio bullet, submenu arrow), drawn as
// pixel art in logical pixels so they scale crisply.
namespace Daytona::Glyph {

enum class Kind
{
    Check,
    Bullet,
    ArrowRight
};

QSize size(Kind kind);

// Draws the glyph with its top-left corner at topLeft.
void draw(QPainter *painter, Kind kind, const QPoint &topLeft, const QColor &colour);

// NT radio button, 12x12 logical pixels: two-tone sunken rings around a
// round field, with a dot when checked.
constexpr int RadioSize = 12;
void drawRadio(QPainter *painter, const QPoint &topLeft, const QColor &outerTopLeft,
    const QColor &outerBottomRight, const QColor &innerTopLeft, const QColor &innerBottomRight,
    const QColor &field, const QColor *dot);

} // namespace Daytona::Glyph
