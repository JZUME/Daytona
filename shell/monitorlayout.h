// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

namespace Daytona {

// Where a monitor dragged to moving.topLeft() settles: edge to edge with one
// of others (sharing part of that edge, overlapping none), as near as
// possible to where it was dropped. Within snap of another's edge, its own
// edge lines up with it. With nothing to touch, it stays where it was
// dropped. Positions and sizes are in the desktop's logical pixels.
QPoint snapMonitor(const QRect &moving, const QList<QRect> &others, int snap);

// Moves the monitors together so the leftmost starts at x 0 and the topmost
// at y 0, as compositors expect.
QList<QRect> normaliseMonitors(const QList<QRect> &monitors);

// One monitor's size changed from before to after: monitors that touched its
// right or bottom edge move with that edge, so the desktop stays in one
// piece.
QList<QRect> resizeMonitor(const QList<QRect> &monitors, int index, const QSize &size);

// The monitor diagram of Windows 2000's Settings tab: each monitor drawn to
// scale where it sits on the desktop, numbered, in the desktop colour. A
// monitor that is switched off is drawn greyed, in a row below. Clicking a
// monitor selects it; dragging a switched-on one moves it, and it snaps
// edge to edge with the others when dropped.
class MonitorArrangement : public QWidget
{
    Q_OBJECT

public:
    struct Monitor
    {
        QString number; // what is drawn on it, "1", "2"...
        QRect geometry; // on the desktop, in logical pixels
        bool enabled = true;
    };

    explicit MonitorArrangement(QWidget *parent = nullptr);

    void setMonitors(const QList<Monitor> &monitors);
    QList<Monitor> monitors() const { return m_monitors; }
    void setSelected(int index);
    int selected() const { return m_selected; }
    void setDesktopColour(const QColor &colour);

    // Where a monitor is drawn, in widget coordinates (for tests).
    QRect monitorRect(int index) const;

    QSize sizeHint() const override;

Q_SIGNALS:
    void selectedChanged(int index);
    // A monitor was dragged to a new place (already snapped).
    void monitorMoved(int index, const QPoint &position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    // Desktop geometry of each monitor as drawn: switched-off monitors are
    // given a place below the desktop.
    QList<QRect> drawnGeometry() const;
    void updateScale();
    int monitorAt(const QPoint &point) const;

    QList<Monitor> m_monitors;
    int m_selected = -1;
    QColor m_desktop { 0, 128, 128 };
    // Desktop to widget: widget = origin + desktop * scale.
    qreal m_scale = 0.1;
    QPointF m_origin;
    QRect m_bounds; // the drawn monitors' desktop bounds
    int m_dragging = -1;
    QPoint m_pressPoint;
    QPoint m_dragOffset; // in widget pixels, while dragging
    bool m_moved = false;
};

} // namespace Daytona
