// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop.h"

#include "daytonastyle.h"

#include <LayerShellQt/Window>
#include <QApplication>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWindow>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcDesktop, "daytona.desktop", QtInfoMsg)

namespace Daytona {

QPoint centredWallpaperPosition(const QSize &desktopPixels, const QSize &imagePixels)
{
    // Images larger than the desktop are cropped evenly on both sides, as NT
    // did; nothing is scaled.
    return { (desktopPixels.width() - imagePixels.width()) / 2,
        (desktopPixels.height() - imagePixels.height()) / 2 };
}

Desktop::Desktop(Settings *settings, QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_settings(settings)
    , m_menu(new QMenu(this))
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    // NT's desktop menu without its icon commands (no desktop icons yet).
    QAction *properties = m_menu->addAction(tr("P&roperties"));
    properties->setObjectName(QStringLiteral("properties"));
    connect(properties, &QAction::triggered, this, &Desktop::propertiesRequested);

    connect(&m_loader, &QFutureWatcher<QImage>::finished, this, [this] {
        m_wallpaper = m_loader.result();
        update();
        Q_EMIT wallpaperLoaded();
    });
    if (auto *style = qobject_cast<Style *>(QApplication::style()))
        connect(style, &Style::coloursChanged, this, qOverload<>(&QWidget::update));
    if (m_settings) {
        connect(m_settings, &Settings::changed, this, [this](const QString &key) {
            if (key.isEmpty() || key.startsWith(QLatin1String("Appearance/")))
                loadWallpaper();
        });
    }
    loadWallpaper();
}

void Desktop::attachToLayerShell()
{
    winId();
    auto *layer = LayerShellQt::Window::get(windowHandle());
    layer->setScope(QStringLiteral("daytona-desktop"));
    layer->setLayer(LayerShellQt::Window::LayerBackground);
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop
        | LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
        | LayerShellQt::Window::AnchorRight));
    // The whole screen, including under the taskbar, as in NT.
    layer->setExclusiveZone(-1);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
}

void Desktop::loadWallpaper()
{
    const QString path = m_settings ? m_settings->wallpaperPath() : QString();
    const WallpaperMode mode = m_settings ? m_settings->wallpaperMode() : WallpaperMode::Centre;
    if (mode != m_mode) {
        m_mode = mode;
        update();
    }
    if (path == m_wallpaperPath)
        return;
    m_wallpaperPath = path;
    m_wallpaper = QImage();
    update();
    if (path.isEmpty()) {
        Q_EMIT wallpaperLoaded();
        return;
    }
    // Decode off the UI thread; wallpapers can be large.
    m_loader.setFuture(QtConcurrent::run([path] {
        QImageReader reader(path);
        QImage image = reader.read();
        if (image.isNull())
            qCWarning(lcDesktop) << "Cannot load wallpaper" << path << reader.errorString();
        return image;
    }));
}

void Desktop::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Style::schemeColour(QStringLiteral("Background")));
    if (m_wallpaper.isNull())
        return;

    // NT drew wallpapers pixel for pixel. Give the image the screen's device
    // pixel ratio so one image pixel covers one device pixel.
    QImage image = m_wallpaper;
    const qreal ratio = devicePixelRatioF();
    image.setDevicePixelRatio(ratio);
    if (m_mode == WallpaperMode::Tile) {
        painter.drawTiledPixmap(rect(), QPixmap::fromImage(image));
    } else {
        const QSize desktopPixels = (QSizeF(size()) * ratio).toSize();
        const QPoint at = centredWallpaperPosition(desktopPixels, image.size());
        painter.drawImage(QPointF(at) / ratio, image);
    }
}

void Desktop::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        m_menu->popup(event->globalPosition().toPoint());
    event->accept();
}

} // namespace Daytona
