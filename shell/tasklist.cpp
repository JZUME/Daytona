// SPDX-License-Identifier: GPL-3.0-or-later

#include "tasklist.h"

#include "taskbutton.h"
#include "toplevelmanager.h"
#include "windowmenu.h"

#include <QMenu>
#include <QToolButton>

namespace Daytona {

namespace {
    constexpr int ButtonHeight = 22;
    constexpr int ArrowRowHeight = 16; // page arrows on a vertical taskbar
    const TaskLayoutMetrics Metrics;
} // namespace

TaskList::TaskList(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(ButtonHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // NT's single-row taskbar pages through its buttons with small arrows.
    m_pageUp = new QToolButton(this);
    m_pageUp->setArrowType(Qt::UpArrow);
    m_pageUp->setAccessibleName(tr("Previous windows"));
    m_pageDown = new QToolButton(this);
    m_pageDown->setArrowType(Qt::DownArrow);
    m_pageDown->setAccessibleName(tr("More windows"));
    for (QToolButton *arrow : { m_pageUp, m_pageDown }) {
        arrow->setFocusPolicy(Qt::NoFocus);
        arrow->hide();
    }
    connect(m_pageUp, &QToolButton::clicked, this, [this] {
        m_page = std::max(0, m_page - 1);
        relayout();
    });
    connect(m_pageDown, &QToolButton::clicked, this, [this] {
        ++m_page;
        relayout();
    });
}

void TaskList::setManager(ToplevelManager *manager)
{
    m_manager = manager;
    ToplevelList &list = manager->list();
    connect(&list, &ToplevelList::added, this, &TaskList::add);
    connect(&list, &ToplevelList::changed, this, &TaskList::change);
    connect(&list, &ToplevelList::removed, this, &TaskList::remove);
    for (const Toplevel &t : list.toplevels())
        add(t.id);
}

void TaskList::add(quint32 id)
{
    const Toplevel *toplevel = m_manager->list().find(id);
    if (!toplevel)
        return;
    auto *button = new TaskButton(id, this);
    button->update(*toplevel);
    connect(button, &TaskButton::clicked, this, [this, button] { buttonClicked(button); });
    connect(button, &TaskButton::windowMenuRequested, this,
        [this, button](const QPoint &pos) { showWindowMenu(button, pos); });
    m_buttons.append(button);
    showActivePage();
    relayout();
}

void TaskList::change(quint32 id)
{
    const Toplevel *toplevel = m_manager->list().find(id);
    for (TaskButton *button : std::as_const(m_buttons)) {
        if (button->id() == id && toplevel) {
            button->update(*toplevel);
            if (toplevel->activated)
                showActivePage();
        }
    }
    relayout();
}

void TaskList::remove(quint32 id)
{
    for (int i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i]->id() == id) {
            delete m_buttons.takeAt(i);
            break;
        }
    }
    relayout();
}

void TaskList::buttonClicked(TaskButton *button)
{
    const Toplevel *toplevel = m_manager->list().find(button->id());
    if (!toplevel)
        return;
    // NT: clicking the active window's button minimises it; any other
    // click brings the window forward, restoring it if minimised.
    if (toplevel->activated && !toplevel->minimized)
        m_manager->setMinimized(toplevel->id, true);
    else
        m_manager->activate(toplevel->id);
}

void TaskList::showWindowMenu(TaskButton *button, const QPoint &globalPos)
{
    const Toplevel *toplevel = m_manager->list().find(button->id());
    if (!toplevel)
        return;
    const quint32 id = toplevel->id;
    const bool minimized = toplevel->minimized;
    const bool maximized = toplevel->maximized;

    // NT brings the window forward when its button is right-clicked, unless
    // it is minimised, which leaves it minimised behind the menu.
    if (!minimized && !toplevel->activated)
        m_manager->activate(id);

    const WindowMenuState state = windowMenuState(*toplevel);
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *restore = menu->addAction(tr("&Restore"), this, [this, id, minimized, maximized] {
        if (minimized)
            m_manager->activate(id); // unminimises, keeping any maximised state
        else if (maximized)
            m_manager->setMaximized(id, false);
    });
    QAction *move = menu->addAction(tr("&Move"));
    QAction *size = menu->addAction(tr("&Size"));
    QAction *minimize
        = menu->addAction(tr("Mi&nimize"), this, [this, id] { m_manager->setMinimized(id, true); });
    QAction *maximize = menu->addAction(tr("Ma&ximize"), this, [this, id, minimized] {
        m_manager->setMaximized(id, true);
        if (minimized)
            m_manager->activate(id);
    });
    menu->addSeparator();
    QAction *close = menu->addAction(tr("&Close"), this, [this, id] { m_manager->close(id); });
    close->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));

    restore->setEnabled(state.restore);
    move->setEnabled(state.move);
    size->setEnabled(state.size);
    minimize->setEnabled(state.minimize);
    maximize->setEnabled(state.maximize);
    close->setEnabled(state.close);
    switch (state.defaultItem) {
    case WindowMenuState::Item::Restore:
        menu->setDefaultAction(restore);
        break;
    default:
        menu->setDefaultAction(close);
        break;
    }
    menu->popup(globalPos);
}

void TaskList::showActivePage()
{
    // Keep the active window's button visible when paging.
    if (m_layout.perPage <= 0)
        return;
    for (int i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i]->isChecked())
            m_page = i / m_layout.perPage;
    }
}

void TaskList::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayout();
}

void TaskList::setVertical(bool vertical)
{
    if (vertical == m_vertical)
        return;
    m_vertical = vertical;
    if (vertical) {
        // Take the taskbar's height, as the width when horizontal.
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(m_rows * ButtonHeight + (m_rows - 1) * Metrics.spacing);
    }
    relayout();
}

void TaskList::setRows(int rows)
{
    m_rows = std::max(1, rows);
    if (!m_vertical)
        setFixedHeight(m_rows * ButtonHeight + (m_rows - 1) * Metrics.spacing);
    relayout();
}

void TaskList::relayout()
{
    if (m_vertical) {
        relayoutVertical();
        return;
    }
    // Buttons fill the rows evenly, as NT wraps them on a taller taskbar.
    const int count = int(m_buttons.size());
    const int wanted = std::max(1, (count + m_rows - 1) / m_rows);
    const TaskLayout row = computeTaskLayout(wanted, width(), Metrics);
    const int perRow = row.paged ? std::max(1, row.perPage) : wanted;
    const int perPage = perRow * m_rows;
    m_layout = { row.buttonWidth, perPage, count > perPage };
    const int pages = count == 0 ? 1 : (count + perPage - 1) / perPage;
    m_page = std::clamp(m_page, 0, pages - 1);

    for (int i = 0; i < count; ++i) {
        const bool visible = !m_layout.paged || i / perPage == m_page;
        m_buttons[i]->setVisible(visible);
        if (visible) {
            const int j = i % perPage;
            m_buttons[i]->setGeometry((j % perRow) * (m_layout.buttonWidth + Metrics.spacing),
                (j / perRow) * (ButtonHeight + Metrics.spacing), m_layout.buttonWidth,
                ButtonHeight);
        }
    }

    m_pageUp->setVisible(m_layout.paged);
    m_pageDown->setVisible(m_layout.paged);
    if (m_layout.paged) {
        const int arrowX = width() - Metrics.arrowsWidth;
        const int half = ButtonHeight / 2;
        m_pageUp->setGeometry(arrowX, 0, Metrics.arrowsWidth, half);
        m_pageDown->setGeometry(arrowX, half, Metrics.arrowsWidth, ButtonHeight - half);
        m_pageUp->setEnabled(m_page > 0);
        m_pageDown->setEnabled(m_page < pages - 1);
    }
}

void TaskList::relayoutVertical()
{
    // One full-width button per row; when they do not all fit, a page at a
    // time with the arrows side by side in the last row.
    const int row = ButtonHeight + Metrics.spacing;
    const int count = int(m_buttons.size());
    int perPage = std::max(1, (height() + Metrics.spacing) / row);
    const bool paged = count > perPage;
    if (paged)
        perPage = std::max(1, (height() - ArrowRowHeight) / row);
    m_layout = { width(), perPage, paged };
    const int pages = count == 0 ? 1 : (count + perPage - 1) / perPage;
    m_page = std::clamp(m_page, 0, pages - 1);

    int y = 0;
    for (int i = 0; i < count; ++i) {
        const bool visible = !paged || i / perPage == m_page;
        m_buttons[i]->setVisible(visible);
        if (visible) {
            m_buttons[i]->setGeometry(0, y, width(), ButtonHeight);
            y += row;
        }
    }
    m_pageUp->setVisible(paged);
    m_pageDown->setVisible(paged);
    if (paged) {
        const int arrowY = height() - ArrowRowHeight;
        const int half = width() / 2;
        m_pageUp->setGeometry(0, arrowY, half, ArrowRowHeight);
        m_pageDown->setGeometry(half, arrowY, width() - half, ArrowRowHeight);
        m_pageUp->setEnabled(m_page > 0);
        m_pageDown->setEnabled(m_page < pages - 1);
    }
}

} // namespace Daytona
