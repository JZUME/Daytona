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
#include <QScreen>
#include <QWindow>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcDesktop, "daytona.desktop", QtInfoMsg)

namespace Daytona {

QRect wallpaperRect(const QSize &desktopPixels, const QSize &imagePixels, WallpaperMode mode)
{
    QSize size = imagePixels;
    switch (mode) {
    case WallpaperMode::Stretch:
        return { QPoint(0, 0), desktopPixels };
    case WallpaperMode::Fit:
        size.scale(desktopPixels, Qt::KeepAspectRatio);
        break;
    case WallpaperMode::Fill:
        size.scale(desktopPixels, Qt::KeepAspectRatioByExpanding);
        break;
    case WallpaperMode::Centre:
    case WallpaperMode::Tile:
        // Images larger than the desktop are cropped evenly on both sides,
        // as NT did; nothing is scaled.
        break;
    }
    return { QPoint((desktopPixels.width() - size.width()) / 2,
                 (desktopPixels.height() - size.height()) / 2),
        size };
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
        m_scaled = QImage();
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

void Desktop::attachToLayerShell(QScreen *screen)
{
    winId();
    if (screen)
        windowHandle()->setScreen(screen);
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
    m_scaled = QImage();
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
    if (m_mode == WallpaperMode::Tile) {
        image.setDevicePixelRatio(ratio);
        painter.drawTiledPixmap(rect(), QPixmap::fromImage(image));
        return;
    }
    const QSize desktopPixels = (QSizeF(size()) * ratio).toSize();
    const QRect target = wallpaperRect(desktopPixels, image.size(), m_mode);
    if (target.size() != image.size()) {
        // Scale once in device pixels, then draw that pixel for pixel.
        if (m_scaled.size() != target.size())
            m_scaled = image.scaled(target.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        image = m_scaled;
    }
    image.setDevicePixelRatio(ratio);
    painter.drawImage(QPointF(target.topLeft()) / ratio, image);
}

void Desktop::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton)
        m_menu->popup(event->globalPosition().toPoint());
    event->accept();
}

DesktopManager::DesktopManager(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

DesktopManager::~DesktopManager()
{
    qDeleteAll(m_desktops);
}

void DesktopManager::start()
{
    for (QScreen *screen : QGuiApplication::screens())
        add(screen);
    connect(qApp, &QGuiApplication::screenAdded, this, &DesktopManager::add);
    connect(qApp, &QGuiApplication::screenRemoved, this, &DesktopManager::remove);
}

void DesktopManager::add(QScreen *screen)
{
    if (m_desktops.contains(screen))
        return;
    auto *desktop = new Desktop(m_settings);
    desktop->setObjectName(QStringLiteral("desktop-") + screen->name());
    connect(desktop, &Desktop::propertiesRequested, this, &DesktopManager::propertiesRequested);
    desktop->attachToLayerShell(screen);
    desktop->show();
    m_desktops.insert(screen, desktop);
}

void DesktopManager::remove(QScreen *screen)
{
    // The output is gone; so is its desktop.
    delete m_desktops.take(screen);
}

} // namespace Daytona
