// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings.h"

#include <QWidget>

class QBoxLayout;
class QMenu;

namespace Daytona {

class NotificationArea;
class StartButton;
class TaskList;

// The NT taskbar: a strip along an edge of the primary screen (the bottom by
// default) that reserves its space, so maximised windows clear it. Unlocked,
// it can be dragged to any edge.
class Taskbar : public QWidget
{
    Q_OBJECT

public:
    // One taskbar row, in logical pixels: a 2 pixel raised top edge, 2
    // pixels of padding, 22 pixel buttons and 2 pixels of padding.
    static constexpr int RowHeight = 28;

    explicit Taskbar(QWidget *parent = nullptr);

    // Makes the window a layer-shell surface. Call before show().
    void attachToLayerShell();

    // Reads and saves edge and lock state. Call before show().
    void setSettings(Settings *settings);

    TaskbarEdge edge() const { return m_edge; }
    // Docks to edge: layout direction, size, anchors and exclusive zone.
    void setEdge(TaskbarEdge edge);
    // Height when horizontal, width when vertical.
    int thickness() const;

    // Rows of a horizontal taskbar, and the width of a vertical one (0 for
    // the narrowest).
    int rows() const { return m_rows; }
    void setRows(int rows);
    void setVerticalWidth(int width);
    int minimumVerticalWidth() const;
    // True within the inner edge's resize strip.
    bool isResizeHandle(const QPoint &pos) const;

    bool isLocked() const { return m_locked; }
    void setLocked(bool locked);

    // The right-click menu on empty taskbar space.
    QMenu *contextMenu() const { return m_menu; }

    StartButton *startButton() const { return m_startButton; }
    NotificationArea *notificationArea() const { return m_notificationArea; }
    TaskList *taskList() const { return m_taskList; }

Q_SIGNALS:
    void edgeChanged(Daytona::TaskbarEdge edge);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void applyLayerShell();
    void applyThickness();
    void updateCursor(const QPoint &pos);

    Settings *m_settings = nullptr;
    TaskbarEdge m_edge = TaskbarEdge::Bottom;
    bool m_locked = true;
    bool m_dragging = false;
    bool m_resizing = false;
    int m_rows = 1;
    int m_verticalWidth = 0;
    QBoxLayout *m_layout;
    QMenu *m_menu;
    QAction *m_lockAction;
    StartButton *m_startButton;
    NotificationArea *m_notificationArea;
    TaskList *m_taskList;
};

} // namespace Daytona
