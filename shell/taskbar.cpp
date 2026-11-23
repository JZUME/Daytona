// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskbar.h"

#include "clock.h"
#include "notificationarea.h"
#include "startbutton.h"
#include "taskbarplacement.h"
#include "tasklist.h"

#include <LayerShellQt/Window>
#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

namespace Daytona {

Taskbar::Taskbar(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Taskbar"));
    setFixedHeight(RowHeight);
    setAttribute(Qt::WA_OpaquePaintEvent);
    // The taskbar is never the active window, but NT shows its tooltips
    // (such as the date on the clock) anyway.
    setAttribute(Qt::WA_AlwaysShowToolTips);

    m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
    m_layout->setSpacing(4);
    m_startButton = new StartButton(this);
    m_layout->addWidget(m_startButton, 0, Qt::AlignLeft | Qt::AlignTop);
    m_taskList = new TaskList(this);
    m_layout->addWidget(m_taskList, 1);
    m_notificationArea = new NotificationArea(this);
    m_notificationArea->setFixedHeight(22);
    m_layout->addWidget(m_notificationArea);

    // NT's taskbar menu, reduced to what applies (no Cascade or Tile, which
    // need window placement the compositor owns).
    m_menu = new QMenu(this);
    m_lockAction = m_menu->addAction(tr("&Lock the Taskbar"));
    m_lockAction->setCheckable(true);
    m_lockAction->setChecked(m_locked);
    connect(m_lockAction, &QAction::toggled, this, &Taskbar::setLocked);
    m_menu->addSeparator();
    QAction *properties = m_menu->addAction(tr("P&roperties"));
    properties->setObjectName(QStringLiteral("properties"));
    connect(properties, &QAction::triggered, this, &Taskbar::propertiesRequested);

    // Auto hide: NT waited a moment after the pointer left before hiding.
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(500);
    connect(m_hideTimer, &QTimer::timeout, this, [this] {
        if (!m_autoHide || m_collapsed)
            return;
        const bool keep = QApplication::activePopupWidget() || (m_keepShown && m_keepShown())
            || underMouse() || m_dragging || m_resizing;
        if (keep) {
            m_hideTimer->start(); // check again shortly
            return;
        }
        m_collapsed = true;
        applyThickness();
    });

    // For the resize cursor over the inner edge.
    setMouseTracking(true);
    setEdge(TaskbarEdge::Bottom);
}

void Taskbar::setSettings(Settings *settings)
{
    m_settings = settings;
    if (!settings)
        return;
    setLocked(settings->taskbarLocked());
    m_rows = settings->taskbarRows();
    m_verticalWidth = settings->taskbarWidth();
    m_onTop = settings->taskbarAlwaysOnTop();
    m_autoHide = settings->taskbarAutoHide();
    m_collapsed = m_autoHide;
    setShowClock(settings->showClock());
    setEdge(settings->taskbarEdge());
    // Taskbar Properties (or daytona.ini edited by hand) take effect at once.
    connect(settings, &Settings::changed, this, [this](const QString &key) {
        if (!m_settings)
            return;
        if (key.isEmpty() || key == QLatin1String("Taskbar/AlwaysOnTop"))
            setAlwaysOnTop(m_settings->taskbarAlwaysOnTop());
        if (key.isEmpty() || key == QLatin1String("Taskbar/AutoHide"))
            setAutoHide(m_settings->taskbarAutoHide());
        if (key.isEmpty() || key == QLatin1String("Taskbar/ShowClock"))
            setShowClock(m_settings->showClock());
        if (key.isEmpty() || key == QLatin1String("Taskbar/Locked"))
            setLocked(m_settings->taskbarLocked());
        if ((key.isEmpty() || key == QLatin1String("Taskbar/Edge"))
            && m_settings->taskbarEdge() != m_edge && !m_dragging)
            setEdge(m_settings->taskbarEdge());
    });
}

void Taskbar::setLocked(bool locked)
{
    m_locked = locked;
    m_dragging = m_dragging && !locked;
    m_resizing = m_resizing && !locked;
    if (locked)
        unsetCursor();
    if (m_lockAction->isChecked() != locked)
        m_lockAction->setChecked(locked);
    if (m_settings)
        m_settings->setTaskbarLocked(locked);
}

int Taskbar::minimumVerticalWidth() const
{
    // NT's narrowest vertical taskbar: room for the Start button.
    const int content
        = qMax(m_startButton->sizeHint().width(), m_notificationArea->sizeHint().width());
    return content + 2 + 4; // the raised edge, padding
}

void Taskbar::setAlwaysOnTop(bool onTop)
{
    if (onTop == m_onTop)
        return;
    m_onTop = onTop;
    if (windowHandle())
        applyLayerShell();
}

void Taskbar::setAutoHide(bool autoHide)
{
    if (autoHide == m_autoHide)
        return;
    m_autoHide = autoHide;
    m_collapsed = autoHide; // shown again when the pointer reaches the strip
    applyThickness();
}

void Taskbar::setShowClock(bool show)
{
    m_notificationArea->clock()->setVisible(show);
}

void Taskbar::expand()
{
    m_hideTimer->stop();
    if (m_autoHide && m_collapsed) {
        m_collapsed = false;
        applyThickness();
    }
}

void Taskbar::collapseLater()
{
    if (m_autoHide && !m_collapsed)
        m_hideTimer->start();
}

void Taskbar::enterEvent(QEnterEvent *event)
{
    // The pointer reached the strip. (A pending hide needs no cancelling:
    // it does not hide while the pointer is over the taskbar.)
    if (isCollapsed())
        expand();
    QWidget::enterEvent(event);
}

void Taskbar::leaveEvent(QEvent *event)
{
    collapseLater();
    QWidget::leaveEvent(event);
}

int Taskbar::thickness() const
{
    // Auto-hidden: a 2 pixel strip along the edge, as NT left.
    if (isCollapsed())
        return 2;
    if (isVertical(m_edge))
        return qMax(minimumVerticalWidth(), m_verticalWidth);
    return TaskbarThickness::forRows(m_rows);
}

void Taskbar::setRows(int rows)
{
    m_rows = qMax(1, rows);
    if (!isVertical(m_edge))
        applyThickness();
}

void Taskbar::setVerticalWidth(int width)
{
    m_verticalWidth = qMax(0, width);
    if (isVertical(m_edge))
        applyThickness();
}

void Taskbar::applyThickness()
{
    const bool vertical = isVertical(m_edge);
    for (QWidget *child : { static_cast<QWidget *>(m_startButton),
             static_cast<QWidget *>(m_taskList), static_cast<QWidget *>(m_notificationArea) })
        child->setVisible(!isCollapsed());
    m_taskList->setRows(vertical ? 1 : m_rows);
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if (vertical)
        setFixedWidth(thickness());
    else
        setFixedHeight(thickness());
    if (windowHandle())
        applyLayerShell();
    update();
}

bool Taskbar::isResizeHandle(const QPoint &pos) const
{
    // The inner edge: the raised edge and padding facing the screen.
    constexpr int Strip = 4;
    switch (m_edge) {
    case TaskbarEdge::Bottom:
        return pos.y() >= 0 && pos.y() < Strip;
    case TaskbarEdge::Top:
        return pos.y() >= height() - Strip && pos.y() < height();
    case TaskbarEdge::Left:
        return pos.x() >= width() - Strip && pos.x() < width();
    case TaskbarEdge::Right:
        return pos.x() >= 0 && pos.x() < Strip;
    }
    return false;
}

void Taskbar::updateCursor(const QPoint &pos)
{
    if (!m_locked && isResizeHandle(pos))
        setCursor(isVertical(m_edge) ? Qt::SizeHorCursor : Qt::SizeVerCursor);
    else
        unsetCursor();
}

void Taskbar::setEdge(TaskbarEdge edge)
{
    m_edge = edge;
    const bool vertical = isVertical(edge);
    m_layout->setDirection(vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    // The 2 pixel raised edge faces the screen; 2 pixels of padding elsewhere.
    switch (edge) {
    case TaskbarEdge::Bottom:
        m_layout->setContentsMargins(2, 4, 2, 2);
        break;
    case TaskbarEdge::Top:
        m_layout->setContentsMargins(2, 2, 2, 4);
        break;
    case TaskbarEdge::Left:
        m_layout->setContentsMargins(2, 2, 4, 2);
        break;
    case TaskbarEdge::Right:
        m_layout->setContentsMargins(4, 2, 2, 2);
        break;
    }
    m_taskList->setVertical(vertical);
    // Several rows tall, the notification area stays in the first row.
    m_layout->setAlignment(m_notificationArea, vertical ? Qt::Alignment() : Qt::AlignTop);
    applyThickness();
    Q_EMIT edgeChanged(edge);
}

void Taskbar::attachToLayerShell()
{
    winId(); // creates the QWindow the layer surface is attached to
    auto *layer = LayerShellQt::Window::get(windowHandle());
    layer->setScope(QStringLiteral("daytona-taskbar"));
    // Keyboard focus only when the user interacts with the taskbar.
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    applyLayerShell();
}

void Taskbar::applyLayerShell()
{
    using A = LayerShellQt::Window;
    auto *layer = A::get(windowHandle());
    switch (m_edge) {
    case TaskbarEdge::Bottom:
        layer->setAnchors(A::Anchors(A::AnchorBottom | A::AnchorLeft | A::AnchorRight));
        break;
    case TaskbarEdge::Top:
        layer->setAnchors(A::Anchors(A::AnchorTop | A::AnchorLeft | A::AnchorRight));
        break;
    case TaskbarEdge::Left:
        layer->setAnchors(A::Anchors(A::AnchorLeft | A::AnchorTop | A::AnchorBottom));
        break;
    case TaskbarEdge::Right:
        layer->setAnchors(A::Anchors(A::AnchorRight | A::AnchorTop | A::AnchorBottom));
        break;
    }
    // Always on top: NT's taskbar above windows, or below them when off.
    layer->setLayer(m_onTop ? A::LayerTop : A::LayerBottom);
    // Auto hide reserves no space: maximised windows use the whole screen.
    layer->setExclusiveZone(m_autoHide ? 0 : thickness());
    // Layer properties take effect on the next commit; when the size stays
    // the same (bottom to top), only a repaint makes one.
    // See docs/taskbar-placement.md.
    update();
}

void Taskbar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QPalette &pal = palette();
    painter.fillRect(rect(), pal.button());
    // NT's raised edge on the side facing the screen: light lines when that
    // is the top or left of the bar, shadow lines when it is the bottom or
    // right, as a raised bevel is lit from the top left.
    const QColor light = pal.color(QPalette::Light);
    const QColor midlight = pal.color(QPalette::Midlight);
    const QColor mid = pal.color(QPalette::Mid);
    const QColor shadow = pal.color(QPalette::Shadow);
    switch (m_edge) {
    case TaskbarEdge::Bottom:
        painter.fillRect(QRect(0, 0, width(), 1), midlight);
        painter.fillRect(QRect(0, 1, width(), 1), light);
        break;
    case TaskbarEdge::Top:
        painter.fillRect(QRect(0, height() - 2, width(), 1), mid);
        painter.fillRect(QRect(0, height() - 1, width(), 1), shadow);
        break;
    case TaskbarEdge::Left:
        painter.fillRect(QRect(width() - 2, 0, 1, height()), mid);
        painter.fillRect(QRect(width() - 1, 0, 1, height()), shadow);
        break;
    case TaskbarEdge::Right:
        painter.fillRect(QRect(0, 0, 1, height()), midlight);
        painter.fillRect(QRect(1, 0, 1, height()), light);
        break;
    }
}

void Taskbar::mousePressEvent(QMouseEvent *event)
{
    // A press on the inner edge resizes; elsewhere on empty taskbar space it
    // starts a move. Both only when unlocked.
    if (event->button() == Qt::LeftButton && !m_locked
        && isResizeHandle(event->position().toPoint())) {
        m_resizing = true;
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && !m_locked) {
        m_dragging = true;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void Taskbar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging && !m_resizing) {
        updateCursor(event->position().toPoint());
        return;
    }
    if (!screen())
        return;
    if (m_resizing) {
        const QSize screenSize = screen()->size();
        const QPoint onScreen
            = taskbarToScreen(m_edge, screenSize, thickness(), event->position().toPoint());
        const int wanted = draggedThickness(m_edge, screenSize, onScreen);
        if (isVertical(m_edge)) {
            const int width = clampTaskbarWidth(wanted, minimumVerticalWidth(), screenSize.width());
            if (width != thickness())
                setVerticalWidth(width);
        } else {
            const int rows = snapTaskbarRows(wanted, screenSize.height());
            if (rows != m_rows)
                setRows(rows);
        }
        return;
    }
    // The compositor keeps sending motion to the taskbar while the button is
    // held (implicit grab), with positions relative to the taskbar surface.
    const QSize screenSize = screen()->size();
    const QPoint onScreen
        = taskbarToScreen(m_edge, screenSize, thickness(), event->position().toPoint());
    const TaskbarEdge target = nearestEdge(onScreen, screenSize);
    if (target != m_edge)
        setEdge(target); // snaps live, as NT does
}

void Taskbar::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_resizing && event->button() == Qt::LeftButton) {
        m_resizing = false;
        if (m_settings) {
            m_settings->setTaskbarRows(m_rows);
            m_settings->setTaskbarWidth(m_verticalWidth);
        }
        return;
    }
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        if (m_settings)
            m_settings->setTaskbarEdge(m_edge);
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void Taskbar::contextMenuEvent(QContextMenuEvent *event)
{
    m_menu->popup(event->globalPos());
}

} // namespace Daytona
