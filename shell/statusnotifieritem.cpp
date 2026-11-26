// SPDX-License-Identifier: GPL-3.0-or-later

#include "statusnotifieritem.h"

#include "dbusmenu.h"
#include "volumepopup.h"

#include <QDBusArgument>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QLoggingCategory>
#include <QPixmap>
#include <QPointer>
#include <QtEndian>

Q_LOGGING_CATEGORY(lcSniItem, "daytona.sni.item", QtInfoMsg)

namespace Daytona {

namespace {

    const QString ItemInterface = QStringLiteral("org.kde.StatusNotifierItem");

    // Limits on what an application can make the shell decode.
    constexpr int MaxPixmapSide = 256;
    constexpr int MaxPixmaps = 16;
    constexpr int MaxTextLength = 1024;
    constexpr qint64 MaxIconFileSize = 1024 * 1024;

    QString text(const QVariantMap &properties, const char *key)
    {
        const QVariant value = properties.value(QLatin1String(key));
        return value.typeId() == QMetaType::QString ? value.toString().left(MaxTextLength)
                                                    : QString();
    }

    QList<QImage> pixmaps(const QVariantMap &properties, const char *key)
    {
        const QVariant value = properties.value(QLatin1String(key));
        if (value.userType() != qMetaTypeId<QDBusArgument>())
            return {};
        return parseSniPixmaps(qvariant_cast<QDBusArgument>(value));
    }

    QImage loadIconFile(const QString &path, int size)
    {
        const QFileInfo info(path);
        if (!info.isFile() || info.size() > MaxIconFileSize)
            return {};
        QImageReader reader(path);
        if (reader.supportsOption(QImageIOHandler::ScaledSize))
            reader.setScaledSize(QSize(size, size));
        return reader.read();
    }

    // An icon name looked up in an application's own icon directory, as
    // IconThemePath asks. Only image files named after the icon are read.
    QImage iconFromThemePath(const QString &themePath, const QString &name, int size)
    {
        if (themePath.isEmpty() || name.contains(QLatin1Char('/'))
            || !QDir::isAbsolutePath(themePath))
            return {};
        QImage best;
        int seen = 0;
        QDirIterator it(themePath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext() && ++seen < 2000) {
            const QFileInfo info = it.nextFileInfo();
            if (info.completeBaseName() != name)
                continue;
            const QImage image = loadIconFile(info.filePath(), size);
            if (image.isNull())
                continue;
            // Prefer the smallest image that is still big enough.
            const bool fits = image.width() >= size;
            if (best.isNull() || (fits && (best.width() < size || image.width() < best.width()))
                || (!fits && best.width() < size && image.width() > best.width())) {
                best = image;
            }
        }
        return best;
    }

    QImage iconFromName(const QString &name, const QString &themePath, int size)
    {
        if (name.isEmpty())
            return {};
        // Some applications send a file path instead of a name.
        if (QDir::isAbsolutePath(name))
            return loadIconFile(name, size);
        const QImage fromPath = iconFromThemePath(themePath, name, size);
        if (!fromPath.isNull())
            return fromPath;
        const QIcon icon = QIcon::fromTheme(name);
        return icon.isNull() ? QImage() : icon.pixmap(QSize(size, size), 1.0).toImage();
    }

} // namespace

QList<QImage> parseSniPixmaps(const QDBusArgument &argument)
{
    QList<QImage> result;
    if (argument.currentSignature() != QLatin1String("a(iiay)"))
        return result;
    argument.beginArray();
    while (!argument.atEnd()) {
        int width = 0;
        int height = 0;
        QByteArray data;
        argument.beginStructure();
        argument >> width >> height >> data;
        argument.endStructure();
        if (result.size() >= MaxPixmaps || width <= 0 || height <= 0 || width > MaxPixmapSide
            || height > MaxPixmapSide || data.size() != qsizetype(width) * height * 4) {
            continue;
        }
        QImage image(width, height, QImage::Format_ARGB32);
        const auto *source = reinterpret_cast<const uchar *>(data.constData());
        for (int y = 0; y < height; ++y) {
            auto *line = reinterpret_cast<quint32 *>(image.scanLine(y));
            for (int x = 0; x < width; ++x)
                line[x] = qFromBigEndian<quint32>(source + (qsizetype(y) * width + x) * 4);
        }
        result << image;
    }
    argument.endArray();
    return result;
}

QImage bestSniPixmap(const QList<QImage> &pixmaps, int size)
{
    QImage best;
    for (const QImage &image : pixmaps) {
        const int side = qMax(image.width(), image.height());
        const int bestSide = best.isNull() ? 0 : qMax(best.width(), best.height());
        if (best.isNull() || (side >= size && (bestSide < size || side < bestSide))
            || (bestSide < size && side > bestSide)) {
            best = image;
        }
    }
    return best;
}

QString parseSniToolTipTitle(const QDBusArgument &argument)
{
    if (argument.currentSignature() != QLatin1String("(sa(iiay)ss)"))
        return {};
    QString iconName;
    QString title;
    QString description;
    argument.beginStructure();
    argument >> iconName;
    parseSniPixmaps(argument);
    argument >> title >> description;
    argument.endStructure();
    return title.left(MaxTextLength);
}

StatusNotifierItem::StatusNotifierItem(
    const QString &id, QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_service(id.section(QLatin1Char('/'), 0, 0))
    , m_path(QLatin1Char('/') + id.section(QLatin1Char('/'), 1))
    , m_connection(connection)
{
    // Applications often send several "New..." signals at once.
    m_refresh.setSingleShot(true);
    m_refresh.setInterval(50);
    connect(&m_refresh, &QTimer::timeout, this, &StatusNotifierItem::refresh);
    for (const char *signal :
        { "NewIcon", "NewAttentionIcon", "NewOverlayIcon", "NewToolTip", "NewTitle" }) {
        m_connection.connect(
            m_service, m_path, ItemInterface, QLatin1String(signal), &m_refresh, SLOT(start()));
    }
    // NewStatus carries the status; reading everything again is simpler.
    m_connection.connect(
        m_service, m_path, ItemInterface, QStringLiteral("NewStatus"), &m_refresh, SLOT(start()));
}

void StatusNotifierItem::refresh()
{
    QDBusMessage message = QDBusMessage::createMethodCall(m_service, m_path,
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
    message << ItemInterface;
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isError()) {
            qCDebug(lcSniItem) << m_id << "cannot be read:" << reply.error().message();
            return;
        }
        applyProperties(reply.value());
    });
}

void StatusNotifierItem::applyProperties(const QVariantMap &properties)
{
    const QString status = text(properties, "Status");
    if (status == QLatin1String("Passive"))
        m_status = Status::Passive;
    else if (status == QLatin1String("NeedsAttention"))
        m_status = Status::NeedsAttention;
    else
        m_status = Status::Active;
    m_title = text(properties, "Title");
    m_iconName = text(properties, "IconName");
    m_attentionIconName = text(properties, "AttentionIconName");
    m_iconThemePath = text(properties, "IconThemePath");
    const QVariant menu = properties.value(QStringLiteral("Menu"));
    QString menuPath = menu.userType() == qMetaTypeId<QDBusObjectPath>()
        ? menu.value<QDBusObjectPath>().path()
        : QString();
    if (menuPath == QLatin1String("/") || menuPath.size() > 255)
        menuPath.clear(); // "/" means none
    if (menuPath != m_menuPath) {
        delete m_menuClient;
        m_menuClient = nullptr;
        m_menuPath = menuPath;
    }
    const QVariant isMenu = properties.value(QStringLiteral("ItemIsMenu"));
    m_itemIsMenu = isMenu.typeId() == QMetaType::Bool && isMenu.toBool();
    m_pixmaps = pixmaps(properties, "IconPixmap");
    m_attentionPixmaps = pixmaps(properties, "AttentionIconPixmap");
    m_toolTipTitle.clear();
    const QVariant toolTip = properties.value(QStringLiteral("ToolTip"));
    if (toolTip.userType() == qMetaTypeId<QDBusArgument>())
        m_toolTipTitle = parseSniToolTipTitle(qvariant_cast<QDBusArgument>(toolTip));
    m_ready = true;
    Q_EMIT changed();
}

QPixmap StatusNotifierItem::pixmap(int size) const
{
    QImage image;
    if (m_status == Status::NeedsAttention) {
        image = iconFromName(m_attentionIconName, m_iconThemePath, size);
        if (image.isNull())
            image = bestSniPixmap(m_attentionPixmaps, size);
    }
    if (image.isNull())
        image = iconFromName(m_iconName, m_iconThemePath, size);
    if (image.isNull())
        image = bestSniPixmap(m_pixmaps, size);
    if (image.isNull())
        return {};
    if (image.width() != size || image.height() != size)
        image = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QPixmap::fromImage(image);
}

void StatusNotifierItem::call(const char *method, const QPoint &globalPos)
{
    QDBusMessage message
        = QDBusMessage::createMethodCall(m_service, m_path, ItemInterface, QLatin1String(method));
    message << globalPos.x() << globalPos.y();
    // Fire and forget; a slow or broken application never blocks the shell.
    m_connection.asyncCall(message, 5000);
}

void StatusNotifierItem::showMenu(const QPoint &globalPos, QWidget *anchor)
{
    if (m_menuPath.isEmpty()) {
        contextMenu(globalPos);
        return;
    }
    if (!m_menuClient)
        m_menuClient = new DBusMenuClient(m_service, m_menuPath, m_connection, this);
    QPointer<QWidget> icon = anchor;
    m_menuClient->popup(globalPos, anchor, [icon](QMenu *menu) {
        if (icon)
            popUpBesideTrayIcon(menu, icon);
    });
}

void StatusNotifierItem::activate(const QPoint &globalPos, QWidget *anchor)
{
    if (m_noActivate) {
        showMenu(globalPos, anchor);
        return;
    }
    QDBusMessage message = QDBusMessage::createMethodCall(
        m_service, m_path, ItemInterface, QStringLiteral("Activate"));
    message << globalPos.x() << globalPos.y();
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message, 5000), this);
    QPointer<QWidget> parent = anchor;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, globalPos, parent](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            if (!w->isError())
                return;
            const QDBusError::ErrorType error = w->error().type();
            if (error != QDBusError::UnknownMethod && error != QDBusError::UnknownInterface)
                return;
            m_noActivate = true;
            if (parent && !m_menuPath.isEmpty())
                showMenu(globalPos, parent);
        });
}

void StatusNotifierItem::secondaryActivate(const QPoint &globalPos)
{
    call("SecondaryActivate", globalPos);
}

void StatusNotifierItem::contextMenu(const QPoint &globalPos)
{
    call("ContextMenu", globalPos);
}

} // namespace Daytona
