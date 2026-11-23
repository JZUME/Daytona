// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "notificationqueue.h"
#include "settings.h"

#include <QWidget>

namespace Daytona {

// Where a balloon's box and tail go, in logical pixels. edge is the screen
// edge the taskbar is on; the tail points at target, a distance along that
// edge measured from the end the balloon is anchored to (the right end of a
// horizontal taskbar, the bottom end of a vertical one).
struct BalloonGeometry
{
    QSize surface; // the whole surface: box and tail
    QRect box; // the rounded box, inside surface
    QPolygon tail; // three points; the last is the tip
    int margin = 0; // the surface's distance from its anchored corner along the edge
};
BalloonGeometry balloonGeometry(TaskbarEdge edge, const QSize &content, int target);

// A Windows 2000 notification balloon: tooltip-coloured box with a 1 pixel
// black border and a tail pointing at the notification area, a bold title,
// plain-text body, optional 32 pixel icon and a close button. An overlay
// layer surface that never takes keyboard focus.
class Balloon : public QWidget
{
    Q_OBJECT

public:
    explicit Balloon(QWidget *parent = nullptr);

    // Shows the notification with its tail pointing at target (see
    // BalloonGeometry) on the given taskbar edge.
    void showNotification(const Notification &notification, TaskbarEdge edge, int target);

    // For tests: the layout without a layer surface.
    void prepare(const Notification &notification, TaskbarEdge edge, int target);
    QRect closeButtonRect() const;
    uint notificationId() const { return m_id; }

Q_SIGNALS:
    void closeClicked(uint id);
    void bodyClicked(uint id);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QSize contentSize() const;

    uint m_id = 0;
    QString m_title;
    QString m_body;
    QPixmap m_icon;
    TaskbarEdge m_edge = TaskbarEdge::Bottom;
    BalloonGeometry m_geometry;
    bool m_attached = false;
};

} // namespace Daytona
