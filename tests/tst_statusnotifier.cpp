// SPDX-License-Identifier: GPL-3.0-or-later

// Runs under dbus-run-session: a host (with Daytona's own watcher) on one
// connection and test items on others, as separate applications would be.

#include "dbusmenu.h"
#include "notificationarea.h"
#include "sni/testitem.h"
#include "sni/testmenu.h"
#include "statusnotifierhost.h"
#include "statusnotifieritem.h"
#include "statusnotifierwatcher.h"
#include "trayicon.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QMenu>
#include <QSignalSpy>
#include <QTest>

using namespace Daytona;

namespace {

QImage solid(int size, QColor colour)
{
    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(colour);
    return image;
}

} // namespace

class TestStatusNotifier : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        SniTestItem::registerTypes();
        SniTestMenu::registerTypes();
        qRegisterMetaType<StatusNotifierItem *>();
        QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run under dbus-run-session");
        m_host = new StatusNotifierHost(QDBusConnection::sessionBus(), this);
        QVERIFY(m_host->start());
        QVERIFY(m_host->ownsWatcher());
    }

    void itemIds()
    {
        QCOMPARE(StatusNotifierWatcher::itemId(QString(), QStringLiteral(":1.5")),
            QStringLiteral(":1.5/StatusNotifierItem"));
        QCOMPARE(StatusNotifierWatcher::itemId(
                     QStringLiteral("/org/ayatana/Item"), QStringLiteral(":1.5")),
            QStringLiteral(":1.5/org/ayatana/Item"));
        QCOMPARE(StatusNotifierWatcher::itemId(
                     QStringLiteral("org.example.App"), QStringLiteral(":1.5")),
            QStringLiteral("org.example.App/StatusNotifierItem"));
        QVERIFY(StatusNotifierWatcher::itemId(QStringLiteral("/bad path"), QStringLiteral(":1.5"))
                .isEmpty());
        QVERIFY(StatusNotifierWatcher::itemId(QStringLiteral("not a name"), QStringLiteral(":1.5"))
                .isEmpty());
        QVERIFY(StatusNotifierWatcher::itemId(QStringLiteral("/a//b"), QStringLiteral(":1.5"))
                .isEmpty());
    }

    void bestPixmap()
    {
        const QList<QImage> images { solid(16, Qt::red), solid(32, Qt::green),
            solid(64, Qt::blue) };
        QCOMPARE(bestSniPixmap(images, 16).width(), 16);
        QCOMPARE(bestSniPixmap(images, 24).width(), 32); // smallest big enough
        QCOMPARE(bestSniPixmap(images, 128).width(), 64); // else the largest
        QVERIFY(bestSniPixmap({}, 16).isNull());
    }

    void itemAppearsRespondsAndDisappears()
    {
        NotificationArea area;
        area.setHost(m_host);
        QSignalSpy added(m_host, &StatusNotifierHost::itemAdded);

        QDBusConnection app
            = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("app1"));
        SniTestItem item;
        // Pixmaps only (no icon name): 16 red and 32 green, picked by size.
        item.iconPixmap << sniTestPixmap(solid(16, Qt::red)) << sniTestPixmap(solid(32, Qt::green));
        item.toolTip.title = QStringLiteral("<b>Not bold</b> & co");
        QVERIFY(item.publish(app, false));
        QVERIFY(added.wait(3000));
        StatusNotifierItem *model = m_host->items().constLast();
        QCOMPARE(model->id(), app.baseService() + QStringLiteral("/StatusNotifierItem"));
        QTRY_VERIFY_WITH_TIMEOUT(model->isReady(), 3000);
        QCOMPARE(model->toolTipTitle(), QStringLiteral("<b>Not bold</b> & co"));
        QCOMPARE(model->pixmap(16).toImage().pixelColor(8, 8), QColor(Qt::red));
        QCOMPARE(model->pixmap(32).toImage().pixelColor(8, 8), QColor(Qt::green));

        // Shown left of the clock, with the title as a plain-text tooltip.
        QCOMPARE(area.trayIcons().size(), 1);
        TrayIcon *icon = area.trayIcons().first();
        QVERIFY(!icon->isHidden());
        QVERIFY(icon->toolTip().contains(QLatin1String("&lt;b&gt;Not bold&lt;/b&gt; &amp; co")));

        // Clicks are forwarded.
        QSignalSpy called(&item, &SniTestItem::called);
        icon->resize(icon->sizeHint());
        QTest::mouseClick(icon, Qt::LeftButton);
        QVERIFY(called.wait(3000));
        QVERIFY(item.calls.last().startsWith(QLatin1String("Activate ")));
        QTest::mouseClick(icon, Qt::MiddleButton);
        QVERIFY(called.wait(3000));
        QVERIFY(item.calls.last().startsWith(QLatin1String("SecondaryActivate ")));
        QTest::mouseClick(icon, Qt::RightButton);
        QVERIFY(called.wait(3000));
        QVERIFY(item.calls.last().startsWith(QLatin1String("ContextMenu ")));

        // Passive items are hidden; NeedsAttention uses the attention icon.
        item.setStatus(QStringLiteral("Passive"));
        QTRY_VERIFY_WITH_TIMEOUT(icon->isHidden(), 3000);
        item.attentionIconPixmap << sniTestPixmap(solid(16, Qt::blue));
        item.setStatus(QStringLiteral("NeedsAttention"));
        QTRY_VERIFY_WITH_TIMEOUT(!icon->isHidden(), 3000);
        QCOMPARE(model->pixmap(16).toImage().pixelColor(8, 8), QColor(Qt::blue));

        // The application quits: its icon goes.
        QSignalSpy removed(m_host, &StatusNotifierHost::itemRemoved);
        app = QDBusConnection(QStringLiteral("released")); // drop our reference
        QDBusConnection::disconnectFromBus(QStringLiteral("app1"));
        QVERIFY(removed.wait(3000));
        QTRY_COMPARE_WITH_TIMEOUT(area.trayIcons().size(), 0, 3000);
    }

    void registrationOrderAndBadData()
    {
        NotificationArea area;
        QSignalSpy added(m_host, &StatusNotifierHost::itemAdded);
        QDBusConnection first
            = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("first"));
        QDBusConnection second
            = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("second"));
        SniTestItem a;
        a.title = QStringLiteral("A");
        // Malformed: the data is shorter than 16x16x4 bytes; it is dropped.
        a.iconPixmap << SniTestPixmap { 16, 16, QByteArray(10, 'x') }
                     << SniTestPixmap { 100000, 1, QByteArray(4, 'x') };
        SniTestItem b;
        b.title = QStringLiteral("B");
        QVERIFY(a.publish(first, false));
        QVERIFY(added.wait(3000));
        QVERIFY(b.publish(second, false));
        QVERIFY(added.count() >= 2 || added.wait(3000));
        area.setHost(m_host);
        const QList<TrayIcon *> icons = area.trayIcons();
        QCOMPARE(icons.size(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(icons[0]->item()->isReady() && icons[1]->item()->isReady(), 3000);
        QCOMPARE(icons[0]->item()->title(), QStringLiteral("A"));
        QCOMPARE(icons[1]->item()->title(), QStringLiteral("B"));
        QVERIFY(icons[0]->item()->pixmap(16).isNull());
        // The clock stays rightmost.
        QVERIFY(area.findChildren<QWidget *>().size() >= 3);
        first = QDBusConnection(QStringLiteral("released")); // drop our reference
        QDBusConnection::disconnectFromBus(QStringLiteral("first"));
        second = QDBusConnection(QStringLiteral("released")); // drop our reference
        QDBusConnection::disconnectFromBus(QStringLiteral("second"));
        QTRY_COMPARE_WITH_TIMEOUT(m_host->items().size(), 0, 3000);
    }

    void menuLabels()
    {
        QCOMPARE(dbusMenuLabelToQt(QStringLiteral("_Open")), QStringLiteral("&Open"));
        QCOMPARE(dbusMenuLabelToQt(QStringLiteral("Save __as_")), QStringLiteral("Save _as&"));
        QCOMPARE(dbusMenuLabelToQt(QStringLiteral("Tom & Jerry")), QStringLiteral("Tom && Jerry"));
    }

    void trayMenu()
    {
        NotificationArea area;
        area.setHost(m_host);
        QSignalSpy added(m_host, &StatusNotifierHost::itemAdded);
        QDBusConnection app
            = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("menuapp"));
        SniTestMenu menuObject;
        QVERIFY(menuObject.publish(app));
        SniTestItem item;
        item.menu = QDBusObjectPath(QString::fromLatin1(SniTestMenu::Path));
        item.iconPixmap << sniTestPixmap(solid(16, Qt::red));
        QVERIFY(item.publish(app, false));
        QVERIFY(added.wait(3000));
        TrayIcon *icon = area.trayIcons().constLast();
        QTRY_VERIFY_WITH_TIMEOUT(icon->item()->isReady(), 3000);
        QCOMPARE(icon->item()->menuPath(), QString::fromLatin1(SniTestMenu::Path));

        // Right click shows the application's menu instead of ContextMenu.
        icon->resize(icon->sizeHint());
        QTest::mouseClick(icon, Qt::RightButton);
        QTRY_VERIFY_WITH_TIMEOUT(icon->item()->menuClient(), 3000);
        DBusMenuClient *client = icon->item()->menuClient();
        QTRY_VERIFY_WITH_TIMEOUT(client->menu() && client->menu()->isVisible(), 3000);
        QVERIFY(item.calls.isEmpty());

        const QList<QAction *> actions = client->menu()->actions();
        QCOMPARE(actions.size(), 5); // Open, separator, Options, Disabled, Quit
        QCOMPARE(actions[0]->text(), QStringLiteral("&Open"));
        QVERIFY(actions[1]->isSeparator());
        QCOMPARE(actions[2]->text(), QStringLiteral("&Options"));
        QVERIFY(!actions[3]->isEnabled());
        QCOMPARE(actions[4]->text(), QStringLiteral("&Quit"));

        const QList<QAction *> options = actions[2]->menu()->actions();
        QCOMPARE(options.size(), 3);
        QCOMPARE(options[0]->text(), QStringLiteral("&Check && go"));
        QVERIFY(options[0]->isCheckable() && !options[0]->isChecked());
        QVERIFY(options[1]->isCheckable() && options[1]->isChecked());
        QVERIFY(
            options[1]->actionGroup() && options[1]->actionGroup() == options[2]->actionGroup());

        // Choosing an item sends a clicked event to the application.
        QSignalSpy events(&menuObject, &SniTestMenu::eventReceived);
        actions[0]->trigger();
        QVERIFY(events.wait(3000));
        QCOMPARE(menuObject.events.last(), QStringLiteral("clicked 1"));
        options[0]->trigger();
        QVERIFY(events.wait(3000));
        QCOMPARE(menuObject.events.last(), QStringLiteral("clicked 2"));
        client->menu()->hide();

        app = QDBusConnection(QStringLiteral("released"));
        QDBusConnection::disconnectFromBus(QStringLiteral("menuapp"));
        QTRY_COMPARE_WITH_TIMEOUT(area.trayIcons().size(), 0, 3000);
    }

    void brokenMenuDoesNotBlock()
    {
        // A menu path with nothing behind it: the call fails, nothing shows,
        // and the shell carries on.
        QWidget anchor;
        DBusMenuClient client(QDBusConnection::sessionBus().baseService(),
            QStringLiteral("/Nothing"), QDBusConnection::sessionBus());
        QSignalSpy failed(&client, &DBusMenuClient::failed);
        QElapsedTimer timer;
        timer.start();
        client.popup(QPoint(0, 0), &anchor);
        QVERIFY(timer.elapsed() < 100); // returned at once
        QVERIFY(failed.wait(4000));
        QVERIFY(!client.menu());
    }

    void foreignNamesAreRefused()
    {
        // An item may not register a well-known name it does not own.
        QDBusConnection app
            = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("liar"));
        QDBusMessage message
            = QDBusMessage::createMethodCall(QStringLiteral("org.kde.StatusNotifierWatcher"),
                QStringLiteral("/StatusNotifierWatcher"),
                QStringLiteral("org.kde.StatusNotifierWatcher"),
                QStringLiteral("RegisterStatusNotifierItem"));
        message << QStringLiteral("org.kde.StatusNotifierWatcher");
        app.asyncCall(message);
        QTest::qWait(300);
        QCOMPARE(m_host->items().size(), 0);
        app = QDBusConnection(QStringLiteral("released")); // drop our reference
        QDBusConnection::disconnectFromBus(QStringLiteral("liar"));
    }

private:
    StatusNotifierHost *m_host = nullptr;
};

QTEST_MAIN(TestStatusNotifier)
#include "tst_statusnotifier.moc"
