// SPDX-License-Identifier: GPL-3.0-or-later

#include "blankscreen.h"

#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QScreen>
#include <QWindow>

namespace Daytona {

namespace {

    // How far the pointer may drift, in logical pixels, without ending it.
    constexpr int PointerSlack = 4;
    // How long input is ignored after the screen saver starts.
    constexpr qint64 GraceMs = 500;

} // namespace

BlankScreen::BlankScreen(QObject *parent)
    : QObject(parent)
{
}

BlankScreen::~BlankScreen()
{
    for (const QPointer<QWidget> &window : std::as_const(m_windows))
        delete window;
}

QList<QWidget *> BlankScreen::windows() const
{
    QList<QWidget *> list;
    for (const QPointer<QWidget> &window : m_windows) {
        if (window)
            list.append(window);
    }
    return list;
}

void BlankScreen::start()
{
    if (isActive())
        return;
    m_shown.start();
    m_seenPointer = false;
    for (QScreen *screen : QGuiApplication::screens()) {
        auto *window = new QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint);
        window->setObjectName(QStringLiteral("blank-screen-") + screen->name());
        window->setAttribute(Qt::WA_DeleteOnClose);
        // Black whatever the colour scheme: this is the screen switched off.
        QPalette black = window->palette();
        black.setColor(QPalette::Window, Qt::black);
        window->setPalette(black);
        window->setAutoFillBackground(true);
        window->setCursor(Qt::BlankCursor);
        window->setMouseTracking(true);
        window->installEventFilter(this);
        window->winId();
        window->windowHandle()->setScreen(screen);
        auto *layer = LayerShellQt::Window::get(window->windowHandle());
        layer->setScope(QStringLiteral("daytona-screensaver"));
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop
            | LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
            | LayerShellQt::Window::AnchorRight));
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
        window->show();
        m_windows.append(window);
    }
    connect(qApp, &QGuiApplication::screenAdded, this, &BlankScreen::stop);
    connect(qApp, &QGuiApplication::screenRemoved, this, &BlankScreen::stop);
}

void BlankScreen::stop()
{
    disconnect(qApp, &QGuiApplication::screenAdded, this, &BlankScreen::stop);
    disconnect(qApp, &QGuiApplication::screenRemoved, this, &BlankScreen::stop);
    if (m_windows.isEmpty())
        return;
    const QList<QPointer<QWidget>> windows = std::exchange(m_windows, {});
    for (const QPointer<QWidget> &window : windows) {
        if (window)
            window->deleteLater();
    }
    Q_EMIT stopped();
}

bool BlankScreen::endsScreenSaver(QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
        return !m_shown.isValid() || m_shown.elapsed() >= GraceMs;
    case QEvent::MouseMove: {
        const QPoint position = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
        if (!m_seenPointer || (m_shown.isValid() && m_shown.elapsed() < GraceMs)) {
            m_seenPointer = true;
            m_firstPointer = position;
            return false;
        }
        return (position - m_firstPointer).manhattanLength() > PointerSlack;
    }
    default:
        return false;
    }
}

bool BlankScreen::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        // Where the pointer is when the screen goes black.
        const QPoint position = static_cast<QEnterEvent *>(event)->globalPosition().toPoint();
        if (!m_seenPointer) {
            m_seenPointer = true;
            m_firstPointer = position;
        }
        return false;
    }
    if (endsScreenSaver(event)) {
        stop();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace Daytona
