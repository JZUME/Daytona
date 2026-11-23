// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbusmenu.h"

#include <QActionGroup>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDateTime>
#include <QIcon>
#include <QLoggingCategory>
#include <QMenu>
#include <QWidget>

Q_LOGGING_CATEGORY(lcDBusMenu, "daytona.dbusmenu", QtInfoMsg)

namespace Daytona {

namespace {

    const QString MenuInterface = QStringLiteral("com.canonical.dbusmenu");
    constexpr int CallTimeout = 3000; // ms
    constexpr int MaxDepth = 8;
    constexpr int MaxItems = 500;
    constexpr int MaxLabel = 256;

    bool parseNode(const QDBusArgument &argument, DBusMenuNode *node, int depth, int *count)
    {
        if (argument.currentSignature() != QLatin1String("(ia{sv}av)"))
            return false;
        argument.beginStructure();
        argument >> node->id;
        argument >> node->properties;
        argument.beginArray();
        while (!argument.atEnd()) {
            QDBusVariant child;
            argument >> child;
            const QVariant value = child.variant();
            if (depth >= MaxDepth || *count >= MaxItems
                || value.userType() != qMetaTypeId<QDBusArgument>()) {
                continue;
            }
            DBusMenuNode childNode;
            ++*count;
            if (parseNode(qvariant_cast<QDBusArgument>(value), &childNode, depth + 1, count))
                node->children << childNode;
        }
        argument.endArray();
        argument.endStructure();
        return true;
    }

    QString text(const QVariantMap &properties, const char *key)
    {
        const QVariant value = properties.value(QLatin1String(key));
        return value.typeId() == QMetaType::QString ? value.toString().left(MaxLabel) : QString();
    }

    bool flag(const QVariantMap &properties, const char *key, bool fallback)
    {
        const QVariant value = properties.value(QLatin1String(key));
        return value.typeId() == QMetaType::Bool ? value.toBool() : fallback;
    }

} // namespace

bool parseDBusMenuLayout(const QDBusArgument &argument, DBusMenuNode *root)
{
    int count = 0;
    *root = {};
    return parseNode(argument, root, 0, &count);
}

QString dbusMenuLabelToQt(const QString &label)
{
    QString result;
    result.reserve(label.size());
    for (int i = 0; i < label.size(); ++i) {
        const QChar c = label[i];
        if (c == QLatin1Char('&')) {
            result += QLatin1String("&&");
        } else if (c == QLatin1Char('_')) {
            if (i + 1 < label.size() && label[i + 1] == QLatin1Char('_')) {
                result += QLatin1Char('_');
                ++i;
            } else {
                result += QLatin1Char('&');
            }
        } else {
            result += c;
        }
    }
    return result;
}

void populateDBusMenu(QMenu *menu, const DBusMenuNode &node,
    const std::function<void(int id)> &clicked,
    const std::function<void(QMenu *submenu, int id)> &aboutToShow)
{
    QActionGroup *radioGroup = nullptr;
    for (const DBusMenuNode &child : node.children) {
        const QVariantMap &p = child.properties;
        if (!flag(p, "visible", true))
            continue;
        if (text(p, "type") == QLatin1String("separator")) {
            menu->addSeparator();
            radioGroup = nullptr;
            continue;
        }
        QAction *action = nullptr;
        const QString label = dbusMenuLabelToQt(text(p, "label"));
        if (text(p, "children-display") == QLatin1String("submenu") || !child.children.isEmpty()) {
            QMenu *submenu = menu->addMenu(label);
            action = submenu->menuAction();
            populateDBusMenu(submenu, child, clicked, aboutToShow);
            if (aboutToShow) {
                const int id = child.id;
                QObject::connect(submenu, &QMenu::aboutToShow, submenu,
                    [submenu, id, aboutToShow] { aboutToShow(submenu, id); });
            }
        } else {
            action = menu->addAction(label);
            const int id = child.id;
            QObject::connect(action, &QAction::triggered, menu, [clicked, id] { clicked(id); });
        }
        action->setEnabled(flag(p, "enabled", true));
        const QString iconName = text(p, "icon-name");
        if (!iconName.isEmpty() && !iconName.contains(QLatin1Char('/')))
            action->setIcon(QIcon::fromTheme(iconName));

        const QString toggle = text(p, "toggle-type");
        if (toggle == QLatin1String("checkmark") || toggle == QLatin1String("radio")) {
            action->setCheckable(true);
            const QVariant state = p.value(QStringLiteral("toggle-state"));
            action->setChecked(state.canConvert<int>() && state.toInt() == 1);
            if (toggle == QLatin1String("radio")) {
                // Consecutive radio items form a group, as dbusmenu clients do.
                if (!radioGroup) {
                    radioGroup = new QActionGroup(menu);
                    radioGroup->setExclusionPolicy(
                        QActionGroup::ExclusionPolicy::ExclusiveOptional);
                }
                radioGroup->addAction(action);
            } else {
                radioGroup = nullptr;
            }
        } else {
            radioGroup = nullptr;
        }
    }
}

DBusMenuClient::DBusMenuClient(
    const QString &service, const QString &path, QDBusConnection connection, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_path(path)
    , m_connection(connection)
{
    m_relayout.setSingleShot(true);
    m_relayout.setInterval(50);
    connect(&m_relayout, &QTimer::timeout, this, [this] {
        // Only an open menu is updated; a closed one is fetched afresh.
        if (!m_menu || !m_menu->isVisible())
            return;
        QPointer<QMenu> menu = m_menu;
        fetch(0, [this, menu](const DBusMenuNode &root) {
            if (menu && menu->isVisible())
                fill(menu, root);
        });
    });
    m_connection.connect(m_service, m_path, MenuInterface, QStringLiteral("LayoutUpdated"), this,
        SLOT(layoutChanged()));
    m_connection.connect(m_service, m_path, MenuInterface, QStringLiteral("ItemsPropertiesUpdated"),
        this, SLOT(layoutChanged()));
}

void DBusMenuClient::layoutChanged()
{
    m_relayout.start();
}

void DBusMenuClient::fetch(int parentId, const std::function<void(const DBusMenuNode &)> &done)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        m_service, m_path, MenuInterface, QStringLiteral("GetLayout"));
    message << parentId << -1 << QStringList();
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message, CallTimeout), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, done](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            const QDBusMessage reply = w->reply();
            DBusMenuNode root;
            if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2
                || reply.arguments().at(1).userType() != qMetaTypeId<QDBusArgument>()
                || !parseDBusMenuLayout(
                    qvariant_cast<QDBusArgument>(reply.arguments().at(1)), &root)) {
                qCDebug(lcDBusMenu) << m_service << "sent no usable menu:" << reply.errorMessage();
                Q_EMIT failed();
                return;
            }
            done(root);
        });
}

void DBusMenuClient::sendEvent(int id)
{
    QDBusMessage message
        = QDBusMessage::createMethodCall(m_service, m_path, MenuInterface, QStringLiteral("Event"));
    message << id << QStringLiteral("clicked") << QVariant::fromValue(QDBusVariant(0))
            << uint(QDateTime::currentSecsSinceEpoch());
    m_connection.asyncCall(message, CallTimeout);
}

void DBusMenuClient::aboutToShow(QMenu *submenu, int id)
{
    QDBusMessage message = QDBusMessage::createMethodCall(
        m_service, m_path, MenuInterface, QStringLiteral("AboutToShow"));
    message << id;
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(message, CallTimeout), this);
    QPointer<QMenu> menu = submenu;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, menu, id](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            const QDBusPendingReply<bool> reply = *w;
            // The application wants the submenu read again.
            if (!reply.isError() && reply.value() && menu) {
                fetch(id, [this, menu](const DBusMenuNode &node) {
                    if (menu)
                        fill(menu, node);
                });
            }
        });
}

void DBusMenuClient::fill(QMenu *menu, const DBusMenuNode &node)
{
    menu->clear();
    populateDBusMenu(
        menu, node, [this](int id) { sendEvent(id); },
        [this](QMenu *submenu, int id) { aboutToShow(submenu, id); });
}

void DBusMenuClient::popup(const QPoint &globalPos, QWidget *anchor)
{
    // Tell the application first, as dbusmenu asks; it may update the menu
    // before we read it. Only the layout read waits; this call does not.
    QDBusMessage notice = QDBusMessage::createMethodCall(
        m_service, m_path, MenuInterface, QStringLiteral("AboutToShow"));
    notice << 0;
    m_connection.asyncCall(notice, CallTimeout);

    QPointer<QWidget> parent = anchor;
    fetch(0, [this, parent, globalPos](const DBusMenuNode &root) {
        if (!parent)
            return;
        if (m_menu)
            m_menu->deleteLater();
        m_menu = new QMenu(parent);
        fill(m_menu, root);
        if (m_menu->isEmpty()) {
            m_menu->deleteLater();
            Q_EMIT failed();
            return;
        }
        m_menu->popup(globalPos);
        Q_EMIT shown();
    });
}

} // namespace Daytona
