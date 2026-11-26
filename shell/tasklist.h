// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "tasklayout.h"

#include <QHash>
#include <QList>
#include <QWidget>

class QToolButton;

namespace Daytona {

class TaskButton;
class ToplevelManager;

// The row of window buttons between the Start button and the notification
// area.
class TaskList : public QWidget
{
    Q_OBJECT

public:
    explicit TaskList(QWidget *parent = nullptr);

    void setManager(ToplevelManager *manager);

    // On a vertical taskbar the buttons are stacked, full width, one per
    // row, as NT does.
    void setVertical(bool vertical);
    bool isVertical() const { return m_vertical; }
    // On a horizontal taskbar several rows tall, buttons wrap across rows.
    void setRows(int rows);
    int rows() const { return m_rows; }

    QList<TaskButton *> buttons() const { return m_buttons; }
    const TaskLayout &layout() const { return m_layout; }
    int page() const { return m_page; }

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void add(quint32 id);
    void change(quint32 id);
    void remove(quint32 id);
    void sync();
    void buttonClicked(TaskButton *button);
    void showWindowMenu(TaskButton *button, const QPoint &globalPos);
    void showActivePage();
    void relayout();
    void relayoutVertical();

    ToplevelManager *m_manager = nullptr;
    QList<TaskButton *> m_buttons; // launch order
    TaskLayout m_layout;
    int m_page = 0;
    bool m_vertical = false;
    int m_rows = 1;
    QToolButton *m_pageUp;
    QToolButton *m_pageDown;
};

} // namespace Daytona
