// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// A small com.canonical.dbusmenu menu, as tray applications export it:
//   Open (id 1), separator, Options > [Check (2, checkmark), A (3, radio,
//   on), B (4, radio)], Disabled (5), Quit (6).

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

struct SniTestMenuItem
{
    int id = 0;
    QVariantMap properties;
    QList<QDBusVariant> children;
};
inline bool operator==(const SniTestMenuItem &a, const SniTestMenuItem &b)
{
    return a.id == b.id && a.properties == b.properties;
}
Q_DECLARE_METATYPE(SniTestMenuItem)

inline QDBusArgument &operator<<(QDBusArgument &argument, const SniTestMenuItem &item)
{
    argument.beginStructure();
    argument << item.id << item.properties;
    argument.beginArray(qMetaTypeId<QDBusVariant>());
    for (const QDBusVariant &child : item.children)
        argument << child;
    argument.endArray();
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SniTestMenuItem &item)
{
    argument.beginStructure();
    argument >> item.id >> item.properties;
    argument.beginArray();
    while (!argument.atEnd()) {
        QDBusVariant child;
        argument >> child;
        item.children << child;
    }
    argument.endArray();
    argument.endStructure();
    return argument;
}

class SniTestMenu : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.dbusmenu")

public:
    static constexpr const char *Path = "/MenuBar";

    static void registerTypes() { qDBusRegisterMetaType<SniTestMenuItem>(); }

    QStringList events; // "clicked 1", ...
    bool optionsChecked = false;

    bool publish(QDBusConnection connection)
    {
        return connection.registerObject(QString::fromLatin1(Path), this,
            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
    }

public Q_SLOTS:
    Q_SCRIPTABLE uint GetLayout(
        int parentId, int recursionDepth, const QStringList &properties, SniTestMenuItem &layout)
    {
        Q_UNUSED(recursionDepth)
        Q_UNUSED(properties)
        layout = parentId == 10 ? options() : root();
        return 1;
    }

    Q_SCRIPTABLE void Event(
        int id, const QString &eventId, const QDBusVariant &data, uint timestamp)
    {
        Q_UNUSED(data)
        Q_UNUSED(timestamp)
        const QString event = QStringLiteral("%1 %2").arg(eventId).arg(id);
        events << event;
        Q_EMIT eventReceived(event);
    }

    Q_SCRIPTABLE bool AboutToShow(int id) { return id == 10; }

Q_SIGNALS:
    Q_SCRIPTABLE void LayoutUpdated(uint revision, int parent);
    void eventReceived(const QString &event);

private:
    static QDBusVariant item(int id, const QVariantMap &properties)
    {
        return QDBusVariant(QVariant::fromValue(SniTestMenuItem { id, properties, {} }));
    }

    SniTestMenuItem options() const
    {
        SniTestMenuItem menu { 10,
            { { QStringLiteral("label"), QStringLiteral("_Options") },
                { QStringLiteral("children-display"), QStringLiteral("submenu") } },
            {} };
        menu.children << item(2,
            { { QStringLiteral("label"), QStringLiteral("_Check & go") },
                { QStringLiteral("toggle-type"), QStringLiteral("checkmark") },
                { QStringLiteral("toggle-state"), optionsChecked ? 1 : 0 } })
                      << item(3,
                             { { QStringLiteral("label"), QStringLiteral("A") },
                                 { QStringLiteral("toggle-type"), QStringLiteral("radio") },
                                 { QStringLiteral("toggle-state"), 1 } })
                      << item(4,
                             { { QStringLiteral("label"), QStringLiteral("B") },
                                 { QStringLiteral("toggle-type"), QStringLiteral("radio") },
                                 { QStringLiteral("toggle-state"), 0 } });
        return menu;
    }

    SniTestMenuItem root() const
    {
        SniTestMenuItem menu { 0, {}, {} };
        menu.children << item(1,
            { { QStringLiteral("label"), QStringLiteral("_Open") },
                { QStringLiteral("icon-name"), QStringLiteral("document-open") } })
                      << item(7, { { QStringLiteral("type"), QStringLiteral("separator") } })
                      << QDBusVariant(QVariant::fromValue(options()))
                      << item(5,
                             { { QStringLiteral("label"), QStringLiteral("Disabled") },
                                 { QStringLiteral("enabled"), false } })
                      << item(8,
                             { { QStringLiteral("label"), QStringLiteral("Hidden") },
                                 { QStringLiteral("visible"), false } })
                      << item(6, { { QStringLiteral("label"), QStringLiteral("_Quit") } });
        return menu;
    }
};
