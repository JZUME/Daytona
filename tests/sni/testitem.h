// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A minimal StatusNotifierItem, as an application would export it, for
// tests and for trying the notification area by hand.

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QImage>
#include <QObject>
#include <QStringList>
#include <QtEndian>

struct SniTestPixmap
{
    int width = 0;
    int height = 0;
    QByteArray data; // ARGB32, network byte order
};
inline bool operator==(const SniTestPixmap &a, const SniTestPixmap &b)
{
    return a.width == b.width && a.height == b.height && a.data == b.data;
}
inline bool operator!=(const SniTestPixmap &a, const SniTestPixmap &b)
{
    return !(a == b);
}
Q_DECLARE_METATYPE(SniTestPixmap)

struct SniTestToolTip
{
    QString iconName;
    QList<SniTestPixmap> pixmaps;
    QString title;
    QString text;
};
inline bool operator==(const SniTestToolTip &a, const SniTestToolTip &b)
{
    return a.iconName == b.iconName && a.pixmaps == b.pixmaps && a.title == b.title
        && a.text == b.text;
}
inline bool operator!=(const SniTestToolTip &a, const SniTestToolTip &b)
{
    return !(a == b);
}
Q_DECLARE_METATYPE(SniTestToolTip)

inline QDBusArgument &operator<<(QDBusArgument &argument, const SniTestPixmap &pixmap)
{
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.data;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SniTestPixmap &pixmap)
{
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.data;
    argument.endStructure();
    return argument;
}

inline QDBusArgument &operator<<(QDBusArgument &argument, const SniTestToolTip &tip)
{
    argument.beginStructure();
    argument << tip.iconName << tip.pixmaps << tip.title << tip.text;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SniTestToolTip &tip)
{
    argument.beginStructure();
    argument >> tip.iconName >> tip.pixmaps >> tip.title >> tip.text;
    argument.endStructure();
    return argument;
}

inline SniTestPixmap sniTestPixmap(const QImage &source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    SniTestPixmap pixmap { image.width(), image.height(), {} };
    pixmap.data.resize(qsizetype(image.width()) * image.height() * 4);
    auto *out = reinterpret_cast<uchar *>(pixmap.data.data());
    for (int y = 0; y < image.height(); ++y) {
        const auto *line = reinterpret_cast<const quint32 *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x)
            qToBigEndian<quint32>(line[x], out + (qsizetype(y) * image.width() + x) * 4);
    }
    return pixmap;
}

class SniTestItem : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierItem")
    Q_PROPERTY(QString Category MEMBER category)
    Q_PROPERTY(QString Id MEMBER itemId)
    Q_PROPERTY(QString Title MEMBER title)
    Q_PROPERTY(QString Status MEMBER status)
    Q_PROPERTY(QString IconName MEMBER iconName)
    Q_PROPERTY(QString IconThemePath MEMBER iconThemePath)
    Q_PROPERTY(QList<SniTestPixmap> IconPixmap MEMBER iconPixmap)
    Q_PROPERTY(QString AttentionIconName MEMBER attentionIconName)
    Q_PROPERTY(QList<SniTestPixmap> AttentionIconPixmap MEMBER attentionIconPixmap)
    Q_PROPERTY(SniTestToolTip ToolTip MEMBER toolTip)
    Q_PROPERTY(QDBusObjectPath Menu MEMBER menu)
    Q_PROPERTY(bool ItemIsMenu MEMBER itemIsMenu)

public:
    static void registerTypes()
    {
        qDBusRegisterMetaType<SniTestPixmap>();
        qDBusRegisterMetaType<QList<SniTestPixmap>>();
        qDBusRegisterMetaType<SniTestToolTip>();
    }

    QString category = QStringLiteral("ApplicationStatus");
    QString itemId = QStringLiteral("daytona-sni-test");
    QString title = QStringLiteral("Daytona test item");
    QString status = QStringLiteral("Active");
    QString iconName;
    QString iconThemePath;
    QList<SniTestPixmap> iconPixmap;
    QString attentionIconName;
    QList<SniTestPixmap> attentionIconPixmap;
    SniTestToolTip toolTip;
    QDBusObjectPath menu { QStringLiteral("/") }; // "/" means no menu
    bool itemIsMenu = false;
    QStringList calls; // "Activate 1 2", ...

    // Exports the item on connection and registers it with the watcher.
    // Without waiting (for a watcher in the same thread) only the export is
    // checked. Without methods, the item is like libayatana-appindicator's,
    // which has no Activate (or other click) methods, only a menu.
    bool publish(QDBusConnection connection, bool waitForWatcher = true, bool withMethods = true)
    {
        QDBusConnection::RegisterOptions options
            = QDBusConnection::ExportAllProperties | QDBusConnection::ExportScriptableSignals;
        if (withMethods)
            options |= QDBusConnection::ExportScriptableSlots;
        if (!connection.registerObject(QStringLiteral("/StatusNotifierItem"), this, options)) {
            return false;
        }
        QDBusMessage message
            = QDBusMessage::createMethodCall(QStringLiteral("org.kde.StatusNotifierWatcher"),
                QStringLiteral("/StatusNotifierWatcher"),
                QStringLiteral("org.kde.StatusNotifierWatcher"),
                QStringLiteral("RegisterStatusNotifierItem"));
        message << connection.baseService();
        if (!waitForWatcher) {
            connection.asyncCall(message);
            return true;
        }
        return connection.call(message).type() == QDBusMessage::ReplyMessage;
    }

    void setStatus(const QString &newStatus)
    {
        status = newStatus;
        Q_EMIT NewStatus(newStatus);
    }

public Q_SLOTS:
    Q_SCRIPTABLE void Activate(int x, int y) { record(QStringLiteral("Activate"), x, y); }
    Q_SCRIPTABLE void SecondaryActivate(int x, int y)
    {
        record(QStringLiteral("SecondaryActivate"), x, y);
    }
    Q_SCRIPTABLE void ContextMenu(int x, int y) { record(QStringLiteral("ContextMenu"), x, y); }

Q_SIGNALS:
    Q_SCRIPTABLE void NewStatus(const QString &status);
    Q_SCRIPTABLE void NewIcon();
    Q_SCRIPTABLE void NewToolTip();
    void called(const QString &call);

private:
    void record(const QString &method, int x, int y)
    {
        const QString call = QStringLiteral("%1 %2 %3").arg(method).arg(x).arg(y);
        calls << call;
        Q_EMIT called(call);
    }
};
