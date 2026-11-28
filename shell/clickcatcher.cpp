// SPDX-License-Identifier: GPL-3.0-or-later

#include "clickcatcher.h"

#include <LayerShellQt/Window>
#include <QMouseEvent>
#include <QScreen>
#include <QWindow>

namespace Daytona {

ClickCatcher::ClickCatcher(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
}

void ClickCatcher::cover(const QWidget *onScreenOf)
{
    QScreen *screen = onScreenOf ? onScreenOf->screen() : nullptr;
    // A layer surface stays on the output it was made for: start afresh when
    // the taskbar has moved to another output.
    if (m_attached && screen && windowHandle() && windowHandle()->screen() != screen) {
        hide();
        destroy(true, false);
        m_attached = false;
    }
    if (!m_attached) {
        winId();
        if (screen)
            windowHandle()->setScreen(screen);
        auto *layer = LayerShellQt::Window::get(windowHandle());
        layer->setScope(QStringLiteral("daytona-click-catcher"));
        layer->setLayer(LayerShellQt::Window::LayerTop);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop
            | LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
            | LayerShellQt::Window::AnchorRight));
        // Cover the whole screen, taskbar included.
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        m_attached = true;
    }
    show();
}

void ClickCatcher::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    Q_EMIT clicked();
}

} // namespace Daytona

namespace Daytona {

void ClickCatcher::paintEvent(QPaintEvent *)
{
    // A surface only receives input once it has a buffer. A translucent
    // window's buffer is already cleared to transparent before painting, so
    // there is nothing to draw: painting the whole screen again each time
    // the Start menu opens would only add to the wait.
}

} // namespace Daytona
