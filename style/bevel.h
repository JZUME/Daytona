// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPalette>
#include <QRect>

class QPainter;

// Windows NT 3D edges, drawn from QPalette roles so a colour scheme change
// restyles them. Each edge is a 1 logical pixel filled rectangle, which stays
// crisp at integer scale factors.
//
// Palette roles follow Qt's mapping of the Windows system colours:
//   Light    = ButtonHilight (white)     Midlight = ButtonLight
//   Dark     = ButtonShadow (grey)       Shadow   = ButtonDkShadow (black)
namespace Daytona::Bevel {

// Raised button edge: outer light/shadow, inner midlight/dark.
void raised(QPainter *painter, const QRect &rect, const QPalette &palette);

// Pressed button edge: the raised edge inverted.
void pressed(QPainter *painter, const QRect &rect, const QPalette &palette);

// Sunken field edge (text boxes, list boxes): outer dark/light, inner
// shadow/midlight.
void sunken(QPainter *painter, const QRect &rect, const QPalette &palette);

// 1 pixel raised and sunken edges (panels, status areas).
void raisedThin(QPainter *painter, const QRect &rect, const QPalette &palette);
void sunkenThin(QPainter *painter, const QRect &rect, const QPalette &palette);

// Two-colour edge: topLeft along the top and left, bottomRight along the
// bottom and right. The bottom-left and top-right corner pixels belong to
// bottomRight, as in Windows.
void edge(QPainter *painter, const QRect &rect, const QColor &topLeft, const QColor &bottomRight);

// Dotted focus rectangle: every other pixel, as NT draws it.
void focusRect(QPainter *painter, const QRect &rect, const QColor &colour);

} // namespace Daytona::Bevel
