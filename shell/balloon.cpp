// SPDX-License-Identifier: GPL-3.0-or-later

#include "balloon.h"

#include <LayerShellQt/Window>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>
#include <QWindow>

namespace Daytona {

namespace {

    constexpr int Tail = 16; // length of the tail
    constexpr int TailInset = 24; // tail tip from the box's anchored end
    constexpr int Radius = 7;
    constexpr int Padding = 8;
    constexpr int IconSize = 32;
    constexpr int CloseSize = 14;
    constexpr int TextWidth = 260;
    constexpr int ScreenGap = 4; // keep the box off the screen edge

    QPixmap loadIcon(const Notification &notification, qreal ratio)
    {
        const int size = qRound(IconSize * ratio);
        QImage image = notification.image;
        if (image.isNull() && !notification.appIcon.isEmpty()) {
            QString path = notification.appIcon;
            if (path.startsWith(QLatin1String("file://")))
                path = QUrl(path).toLocalFile();
            if (QFileInfo(path).isAbsolute()) {
                const QFileInfo info(path);
                if (info.isFile() && info.size() <= 1024 * 1024) {
                    QImageReader reader(path);
                    image = reader.read();
                }
            } else {
                image = QIcon::fromTheme(path).pixmap(QSize(size, size), 1.0).toImage();
            }
        }
        if (image.isNull())
            return {};
        QPixmap pixmap = QPixmap::fromImage(
            image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        pixmap.setDevicePixelRatio(ratio);
        return pixmap;
    }

} // namespace

BalloonGeometry balloonGeometry(TaskbarEdge edge, const QSize &content, int target)
{
    BalloonGeometry g;
    const QSize box = content + QSize(2 * Padding, 2 * Padding);
    const bool horizontal = edge == TaskbarEdge::Bottom || edge == TaskbarEdge::Top;
    const int along = horizontal ? box.width() : box.height();
    // Put the tail TailInset in from the anchored end, unless that would push
    // the balloon off the screen.
    g.margin = qMax(ScreenGap, target - TailInset);
    const int tip = qBound(Radius + 1, target - g.margin, along - Radius - Tail - 1);
    g.surface = horizontal ? QSize(box.width(), box.height() + Tail)
                           : QSize(box.width() + Tail, box.height());
    switch (edge) {
    case TaskbarEdge::Bottom: {
        // Box on top, tail down; measured from the right.
        g.box = QRect(QPoint(0, 0), box);
        const int x = box.width() - 1 - tip;
        g.tail = QPolygon({ QPoint(x - Tail, box.height() - 1), QPoint(x, box.height() - 1),
            QPoint(x, box.height() - 1 + Tail) });
        break;
    }
    case TaskbarEdge::Top: {
        g.box = QRect(QPoint(0, Tail), box);
        const int x = box.width() - 1 - tip;
        g.tail = QPolygon({ QPoint(x - Tail, Tail), QPoint(x, Tail), QPoint(x, 0) });
        break;
    }
    case TaskbarEdge::Left: {
        // Box to the right, tail left; measured from the bottom.
        g.box = QRect(QPoint(Tail, 0), box);
        const int y = box.height() - 1 - tip;
        g.tail = QPolygon({ QPoint(Tail, y - Tail), QPoint(Tail, y), QPoint(0, y) });
        break;
    }
    case TaskbarEdge::Right: {
        g.box = QRect(QPoint(0, 0), box);
        const int y = box.height() - 1 - tip;
        g.tail = QPolygon({ QPoint(box.width() - 1, y - Tail), QPoint(box.width() - 1, y),
            QPoint(box.width() - 1 + Tail, y) });
        break;
    }
    }
    return g;
}

Balloon::Balloon(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
}

QSize Balloon::contentSize() const
{
    QFont bold = font();
    bold.setBold(true);
    const QFontMetrics titleMetrics(bold);
    const QFontMetrics bodyMetrics(font());
    const int textLeft = m_icon.isNull() ? 0 : IconSize + Padding;
    const QRect title = titleMetrics.boundingRect(
        QRect(0, 0, TextWidth - CloseSize - Padding, 1000), Qt::TextWordWrap, m_title);
    const QRect body = m_body.isEmpty()
        ? QRect()
        : bodyMetrics.boundingRect(QRect(0, 0, TextWidth, 1000), Qt::TextWordWrap, m_body);
    const int width = textLeft + qMax(title.width() + Padding + CloseSize, body.width());
    const int textHeight
        = qMax(title.height(), CloseSize) + (m_body.isEmpty() ? 0 : 4 + body.height());
    return { qMax(width, 120), qMax(textHeight, m_icon.isNull() ? 0 : IconSize) };
}

void Balloon::prepare(const Notification &notification, TaskbarEdge edge, int target)
{
    m_id = notification.id;
    m_title = notification.summary.isEmpty() ? notification.appName : notification.summary;
    m_body = notification.body;
    m_icon = loadIcon(notification, devicePixelRatioF());
    m_edge = edge;
    m_geometry = balloonGeometry(edge, contentSize(), target);
    setFixedSize(m_geometry.surface);
    update();
}

void Balloon::showNotification(
    const Notification &notification, TaskbarEdge edge, int target, QScreen *screen)
{
    // LayerShellQt (6.3) loses the layer role when a surface is hidden and
    // shown again, so each balloon starts from a fresh native window.
    if (m_attached) {
        destroy(true, false);
        m_attached = false;
    }
    prepare(notification, edge, target);
    create();
    if (screen)
        windowHandle()->setScreen(screen);
    auto *layer = LayerShellQt::Window::get(windowHandle());
    layer->setScope(QStringLiteral("daytona-balloon"));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    // Exclusive zone 0: placed clear of the taskbar's exclusive zone.
    layer->setExclusiveZone(0);
    using A = LayerShellQt::Window;
    switch (edge) {
    case TaskbarEdge::Bottom:
        layer->setAnchors(A::Anchors(A::AnchorBottom | A::AnchorRight));
        layer->setMargins(QMargins(0, 0, m_geometry.margin, 0));
        break;
    case TaskbarEdge::Top:
        layer->setAnchors(A::Anchors(A::AnchorTop | A::AnchorRight));
        layer->setMargins(QMargins(0, 0, m_geometry.margin, 0));
        break;
    case TaskbarEdge::Left:
        layer->setAnchors(A::Anchors(A::AnchorLeft | A::AnchorBottom));
        layer->setMargins(QMargins(0, 0, 0, m_geometry.margin));
        break;
    case TaskbarEdge::Right:
        layer->setAnchors(A::Anchors(A::AnchorRight | A::AnchorBottom));
        layer->setMargins(QMargins(0, 0, 0, m_geometry.margin));
        break;
    }
    m_attached = true;
    show();
}

QRect Balloon::closeButtonRect() const
{
    const QRect box = m_geometry.box;
    return { box.right() - Padding - CloseSize + 2, box.top() + Padding - 2, CloseSize, CloseSize };
}

void Balloon::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // GDI drew balloons as aliased regions.
    painter.setRenderHint(QPainter::Antialiasing, false);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(m_geometry.box.adjusted(0, 0, -1, -1)), Radius, Radius);
    QPainterPath tail;
    tail.addPolygon(QPolygonF(m_geometry.tail));
    tail.closeSubpath();
    shape = shape.united(tail);
    painter.setPen(QPen(Qt::black, 0));
    painter.setBrush(palette().color(QPalette::ToolTipBase));
    painter.drawPath(shape);

    const QRect content = m_geometry.box.adjusted(Padding, Padding, -Padding, -Padding);
    int textLeft = content.left();
    if (!m_icon.isNull()) {
        painter.drawPixmap(content.left(), content.top(), m_icon);
        textLeft += IconSize + Padding;
    }
    painter.setPen(palette().color(QPalette::ToolTipText));
    QFont bold = font();
    bold.setBold(true);
    painter.setFont(bold);
    const QRect titleRect(textLeft, content.top(),
        content.right() - textLeft - CloseSize - Padding + 1, content.height());
    const QRect drawnTitle = painter.boundingRect(titleRect, Qt::TextWordWrap, m_title);
    painter.drawText(titleRect, Qt::TextWordWrap, m_title);
    painter.setFont(font());
    const int bodyTop = qMax(drawnTitle.bottom(), content.top() + CloseSize) + 4;
    painter.drawText(
        QRect(textLeft, bodyTop, content.right() - textLeft + 1, content.bottom() - bodyTop + 1),
        Qt::TextWordWrap, m_body);

    // The close button: an "x" in the corner.
    const QRect close = closeButtonRect();
    painter.setPen(QPen(palette().color(QPalette::ToolTipText), 2));
    painter.drawLine(close.topLeft() + QPoint(3, 3), close.bottomRight() - QPoint(3, 3));
    painter.drawLine(close.topRight() + QPoint(-3, 3), close.bottomLeft() + QPoint(3, -3));
}

void Balloon::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    if (closeButtonRect().contains(event->position().toPoint()))
        Q_EMIT closeClicked(m_id);
    else
        Q_EMIT bodyClicked(m_id);
}

} // namespace Daytona
