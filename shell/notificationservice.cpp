// SPDX-License-Identifier: GPL-3.0-or-later

#include "notificationservice.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcNotifications, "daytona.notifications", QtInfoMsg)

namespace Daytona {

namespace {

    constexpr int MaxSummary = 256;
    constexpr int MaxBody = 2048;
    constexpr int MaxActions = 20; // key/label pairs
    constexpr int MaxImageSide = 512;

} // namespace

QImage parseNotificationImage(const QDBusArgument &argument)
{
    if (argument.currentSignature() != QLatin1String("(iiibiiay)"))
        return {};
    int width = 0;
    int height = 0;
    int rowStride = 0;
    bool hasAlpha = false;
    int bitsPerSample = 0;
    int channels = 0;
    QByteArray data;
    argument.beginStructure();
    argument >> width >> height >> rowStride >> hasAlpha >> bitsPerSample >> channels >> data;
    argument.endStructure();
    if (width <= 0 || height <= 0 || width > MaxImageSide || height > MaxImageSide
        || bitsPerSample != 8 || channels != (hasAlpha ? 4 : 3) || rowStride < width * channels
        || data.size() < qsizetype(rowStride) * (height - 1) + qsizetype(width) * channels) {
        return {};
    }
    // RGB(A) bytes, as GdkPixbuf lays them out.
    QImage image(width, height, hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        memcpy(image.scanLine(y), data.constData() + qsizetype(y) * rowStride,
            size_t(width) * channels);
    }
    return image;
}

NotificationService::NotificationService(
    NotificationQueue *queue, QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_queue(queue)
    , m_connection(connection)
{
    connect(queue, &NotificationQueue::closed, this,
        [this](uint id, NotificationQueue::CloseReason reason) {
            Q_EMIT NotificationClosed(id, uint(reason));
        });
    connect(queue, &NotificationQueue::actionInvoked, this,
        [this](uint id, const QString &key) { Q_EMIT ActionInvoked(id, key); });
}

bool NotificationService::registerOnBus()
{
    if (!m_connection.isConnected())
        return false;
    if (!m_connection.registerObject(QString::fromLatin1(ObjectPath), this,
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        return false;
    }
    if (!m_connection.registerService(QString::fromLatin1(ServiceName))) {
        m_connection.unregisterObject(QString::fromLatin1(ObjectPath));
        return false;
    }
    return true;
}

uint NotificationService::Notify(const QString &appName, uint replacesId, const QString &appIcon,
    const QString &summary, const QString &body, const QStringList &actions,
    const QVariantMap &hints, int expireTimeout)
{
    Notification notification;
    notification.appName = appName.left(MaxSummary);
    notification.appIcon = appIcon.left(1024);
    notification.summary = summary.left(MaxSummary);
    notification.body = body.left(MaxBody);
    notification.actions = actions.mid(0, MaxActions * 2);
    for (QString &action : notification.actions)
        action = action.left(MaxSummary);
    notification.timeoutMs = expireTimeout;
    for (const char *key : { "image-data", "image_data", "icon_data" }) {
        const QVariant value = hints.value(QLatin1String(key));
        if (value.userType() == qMetaTypeId<QDBusArgument>()) {
            notification.image = parseNotificationImage(qvariant_cast<QDBusArgument>(value));
            if (!notification.image.isNull())
                break;
        }
    }
    if (notification.appIcon.isEmpty()) {
        for (const char *key : { "image-path", "image_path" }) {
            const QVariant value = hints.value(QLatin1String(key));
            if (value.typeId() == QMetaType::QString) {
                notification.appIcon = value.toString().left(1024);
                break;
            }
        }
    }
    const uint id = m_queue->notify(notification, replacesId);

    // Sounds (the specification's "sound" capability).
    const auto hint = [&hints](const char *key) { return hints.value(QLatin1String(key)); };
    const QVariant suppress = hint("suppress-sound");
    if (suppress.typeId() == QMetaType::Bool && suppress.toBool())
        return id;
    const QVariant file = hint("sound-file");
    const QVariant name = hint("sound-name");
    if (file.typeId() == QMetaType::QString && QDir::isAbsolutePath(file.toString())) {
        Q_EMIT soundRequested({}, file.toString().left(4096));
    } else if (name.typeId() == QMetaType::QString && !name.toString().isEmpty()
        && !name.toString().contains(QLatin1Char('/'))) {
        Q_EMIT soundRequested(name.toString().left(MaxSummary), {});
    } else if (replacesId == 0) {
        // Updates to a shown notification (progress, say) stay quiet.
        Q_EMIT soundRequested(QStringLiteral("message-new-instant"), {});
    }
    return id;
}

void NotificationService::CloseNotification(uint id)
{
    m_queue->close(id, NotificationQueue::CloseReason::ClosedByCall);
}

QStringList NotificationService::GetCapabilities()
{
    // Markup is not supported: bodies are shown as plain text.
    return { QStringLiteral("body"), QStringLiteral("actions"), QStringLiteral("icon-static"),
        QStringLiteral("sound") };
}

QString NotificationService::GetServerInformation(
    QString &vendor, QString &version, QString &specVersion)
{
    vendor = QStringLiteral("Daytona");
    version = QStringLiteral(DAYTONA_VERSION);
    specVersion = QStringLiteral("1.2");
    return QStringLiteral("Daytona");
}

} // namespace Daytona
