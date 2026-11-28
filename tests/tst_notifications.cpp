// SPDX-License-Identifier: GPL-3.0-or-later

// Runs under dbus-run-session for the D-Bus part.

#include "balloon.h"
#include "notificationqueue.h"
#include "notificationservice.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QSignalSpy>
#include <QTest>

using namespace Daytona;
using Reason = NotificationQueue::CloseReason;

namespace {

Notification note(const QString &summary, int timeoutMs = -1, QStringList actions = {})
{
    Notification n;
    n.summary = summary;
    n.timeoutMs = timeoutMs;
    n.actions = actions;
    return n;
}

} // namespace

class TestNotifications : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { qRegisterMetaType<NotificationQueue::CloseReason>(); }

    void oneAtATimeInOrder()
    {
        NotificationQueue queue;
        QSignalSpy closed(&queue, &NotificationQueue::closed);
        const uint a = queue.notify(note(QStringLiteral("A")));
        const uint b = queue.notify(note(QStringLiteral("B")));
        QVERIFY(a != 0 && b != 0 && a != b);
        QCOMPARE(queue.current()->summary, QStringLiteral("A"));
        QCOMPARE(queue.queued().size(), 1);
        QVERIFY(queue.close(a, Reason::Dismissed));
        QCOMPARE(closed.takeFirst(), QVariantList({ a, QVariant::fromValue(Reason::Dismissed) }));
        QCOMPARE(queue.current()->summary, QStringLiteral("B"));
        QVERIFY(queue.close(b, Reason::ClosedByCall));
        QVERIFY(!queue.current());
        QVERIFY(!queue.close(b, Reason::ClosedByCall)); // already gone
    }

    void replacement()
    {
        NotificationQueue queue;
        QSignalSpy changed(&queue, &NotificationQueue::currentChanged);
        const uint a = queue.notify(note(QStringLiteral("A")));
        const uint b = queue.notify(note(QStringLiteral("B")));
        changed.clear();
        // Replacing the shown one updates it in place, keeping its id.
        QCOMPARE(queue.notify(note(QStringLiteral("A2")), a), a);
        QCOMPARE(queue.current()->summary, QStringLiteral("A2"));
        QCOMPARE(changed.count(), 1);
        // Replacing a queued one keeps its place.
        QCOMPARE(queue.notify(note(QStringLiteral("B2")), b), b);
        QCOMPARE(queue.queued().first().summary, QStringLiteral("B2"));
        // An unknown id is a new notification.
        const uint c = queue.notify(note(QStringLiteral("C")), 9999);
        QVERIFY(c != 9999 && c != a && c != b);
        QCOMPARE(queue.queued().size(), 2);
    }

    void timeouts()
    {
        NotificationQueue queue;
        QCOMPARE(queue.effectiveTimeoutMs(note({}, -1)), NotificationQueue::DefaultTimeoutMs);
        QCOMPARE(queue.effectiveTimeoutMs(note({}, 0)), 0); // never
        QCOMPARE(queue.effectiveTimeoutMs(note({}, 2500)), 2500);
        QCOMPARE(queue.effectiveTimeoutMs(note({}, 999999)), NotificationQueue::MaxTimeoutMs);
        Notification critical = note({}, 2500);
        critical.critical = true;
        QCOMPARE(queue.effectiveTimeoutMs(critical), 0); // critical never expires

        QSignalSpy closed(&queue, &NotificationQueue::closed);
        const uint a = queue.notify(note(QStringLiteral("A"), 100));
        queue.notify(note(QStringLiteral("B"), 100));
        QVERIFY(closed.wait(2000));
        QCOMPARE(closed.takeFirst(), QVariantList({ a, QVariant::fromValue(Reason::Expired) }));
        QCOMPARE(queue.current()->summary, QStringLiteral("B")); // the next one shows
    }

    void stayingBalloonStaysUntilClosed()
    {
        NotificationQueue queue;
        queue.setTurnMs(50);
        QSignalSpy closed(&queue, &NotificationQueue::closed);
        const uint a = queue.notify(note(QStringLiteral("A"), 0));
        QTest::qWait(200); // well past its turn
        QVERIFY(closed.isEmpty());
        QCOMPARE(queue.current()->id, a);
        QVERIFY(queue.close(a, Reason::Dismissed));
        QVERIFY(!queue.current());

        Notification critical = note(QStringLiteral("C"), 50);
        critical.critical = true;
        const uint c = queue.notify(critical);
        QTest::qWait(200);
        QCOMPARE(queue.current()->id, c);
        QCOMPARE(closed.count(), 1); // only A's
    }

    void stayingBalloonGivesWay()
    {
        NotificationQueue queue;
        queue.setTurnMs(100);
        QSignalSpy closed(&queue, &NotificationQueue::closed);
        const uint a = queue.notify(note(QStringLiteral("A"), 0));
        // Within its turn, a new one waits.
        const uint b = queue.notify(note(QStringLiteral("B"), 60000));
        QCOMPARE(queue.current()->id, a);
        // Once the turn is over, the waiting one shows and A goes back in
        // the queue, still open.
        QTRY_COMPARE(queue.current()->id, b);
        QCOMPARE(queue.queued().size(), 1);
        QCOMPARE(queue.queued().first().id, a);
        QVERIFY(closed.isEmpty());
        // A comes back when B closes.
        QVERIFY(queue.close(b, Reason::Dismissed));
        QCOMPARE(queue.current()->id, a);

        // Past its turn with nothing waiting, A stays, and the next
        // notification to arrive shows at once.
        QTest::qWait(200);
        QCOMPARE(queue.current()->id, a);
        const uint c = queue.notify(note(QStringLiteral("C"), 60000));
        QCOMPARE(queue.current()->id, c);
        QCOMPARE(queue.queued().first().id, a);
        // An application can still close it while it waits.
        QVERIFY(queue.close(a, Reason::ClosedByCall));
        QVERIFY(queue.queued().isEmpty());
    }

    void pausedTimeDoesNotRun()
    {
        NotificationQueue queue;
        QSignalSpy closed(&queue, &NotificationQueue::closed);
        const uint a = queue.notify(note(QStringLiteral("A"), 150));
        queue.setPaused(true);
        QTest::qWait(300);
        QVERIFY(closed.isEmpty());
        // One shown while paused waits too.
        queue.close(a, Reason::Dismissed);
        const uint b = queue.notify(note(QStringLiteral("B"), 100));
        QTest::qWait(200);
        QCOMPARE(closed.count(), 1);
        queue.setPaused(false);
        QVERIFY(closed.wait(2000));
        QCOMPARE(closed.last(), QVariantList({ b, QVariant::fromValue(Reason::Expired) }));
    }

    void defaultAction()
    {
        NotificationQueue queue;
        QSignalSpy invoked(&queue, &NotificationQueue::actionInvoked);
        QSignalSpy closed(&queue, &NotificationQueue::closed);
        const uint a = queue.notify(note(QStringLiteral("A"), -1,
            { QStringLiteral("default"), QStringLiteral("Open"), QStringLiteral("x"),
                QStringLiteral("X") }));
        queue.activateCurrent();
        QCOMPARE(invoked.takeFirst(), QVariantList({ a, QStringLiteral("default") }));
        QCOMPARE(closed.takeFirst(), QVariantList({ a, QVariant::fromValue(Reason::Dismissed) }));
        // Without a default action, a click only dismisses.
        const uint b = queue.notify(
            note(QStringLiteral("B"), -1, { QStringLiteral("other"), QStringLiteral("Other") }));
        queue.activateCurrent();
        QVERIFY(invoked.isEmpty());
        QCOMPARE(closed.takeFirst(), QVariantList({ b, QVariant::fromValue(Reason::Dismissed) }));
    }

    void queueIsBounded()
    {
        NotificationQueue queue;
        QSignalSpy closed(&queue, &NotificationQueue::closed);
        for (int i = 0; i < NotificationQueue::MaxQueued + 2; ++i)
            queue.notify(note(QString::number(i)));
        QCOMPARE(queue.queued().size(), NotificationQueue::MaxQueued);
        QCOMPARE(closed.count(), 1);
        QCOMPARE(closed.first().at(1).value<Reason>(), Reason::Undefined);
    }

    void balloonGeometryPointsAtTarget()
    {
        // Bottom taskbar, target 60 px from the right edge.
        const BalloonGeometry g = balloonGeometry(TaskbarEdge::Bottom, QSize(200, 40), 60);
        QCOMPARE(g.surface, QSize(216, 56 + 16));
        QCOMPARE(g.margin, 36);
        // The tip is at the bottom, target - margin from the surface's right.
        const QPoint tip = g.tail.last();
        QCOMPARE(tip.y(), g.surface.height() - 1);
        QCOMPARE(g.surface.width() - 1 - tip.x(), 60 - 36);
        // Close to the screen edge the balloon stays on screen.
        QCOMPARE(balloonGeometry(TaskbarEdge::Bottom, QSize(200, 40), 10).margin, 4);
        // Other edges: the tail is on the taskbar's side.
        QCOMPARE(balloonGeometry(TaskbarEdge::Top, QSize(200, 40), 60).tail.last().y(), 0);
        QCOMPARE(balloonGeometry(TaskbarEdge::Left, QSize(200, 40), 30).tail.last().x(), 0);
        const BalloonGeometry right = balloonGeometry(TaskbarEdge::Right, QSize(200, 40), 30);
        QCOMPARE(right.tail.last().x(), right.surface.width() - 1);
    }

    void balloonClicks()
    {
        Balloon balloon;
        Notification n = note(QStringLiteral("Title"));
        n.id = 7;
        n.body = QStringLiteral("<b>not bold</b>");
        balloon.prepare(n, TaskbarEdge::Bottom, 60);
        balloon.show();
        QSignalSpy closeClicked(&balloon, &Balloon::closeClicked);
        QSignalSpy bodyClicked(&balloon, &Balloon::bodyClicked);
        QTest::mouseClick(&balloon, Qt::LeftButton, {}, balloon.closeButtonRect().center());
        QCOMPARE(closeClicked.takeFirst(), QVariantList({ 7u }));
        QTest::mouseClick(&balloon, Qt::LeftButton, {}, QPoint(20, 30));
        QCOMPARE(bodyClicked.takeFirst(), QVariantList({ 7u }));
        QVERIFY(balloon.focusPolicy() == Qt::NoFocus);
        QVERIFY(balloon.testAttribute(Qt::WA_ShowWithoutActivating));
    }

    void overDBus()
    {
        QVERIFY2(QDBusConnection::sessionBus().isConnected(), "run under dbus-run-session");
        NotificationQueue queue;
        NotificationService service(&queue, QDBusConnection::sessionBus());
        QVERIFY(service.registerOnBus());

        // A client on its own connection, as notify-send would be.
        QDBusConnection client
            = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("client"));
        // Plain messages: QDBusInterface would introspect synchronously, and
        // the server runs in this same thread.
        const auto call = [&client](const QString &method, const QVariantList &args) {
            QDBusMessage message = QDBusMessage::createMethodCall(
                QString::fromLatin1(NotificationService::ServiceName),
                QString::fromLatin1(NotificationService::ObjectPath),
                QStringLiteral("org.freedesktop.Notifications"), method);
            message.setArguments(args);
            return client.asyncCall(message);
        };
        QVERIFY(client.connect(QString(), QString::fromLatin1(NotificationService::ObjectPath),
            QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("NotificationClosed"),
            this, SLOT(recordClosed(uint, uint))));

        QDBusPendingCall caps = call(QStringLiteral("GetCapabilities"), {});
        QTRY_VERIFY(caps.isFinished());
        QCOMPARE(QDBusReply<QStringList>(caps).value(),
            QStringList({ QStringLiteral("body"), QStringLiteral("actions"),
                QStringLiteral("icon-static"), QStringLiteral("sound") }));

        QDBusPendingCall notify = call(QStringLiteral("Notify"),
            { QStringLiteral("app"), 0u, QString(), QStringLiteral("Hello"),
                QStringLiteral("World"), QStringList(), QVariantMap(), -1 });
        QTRY_VERIFY(notify.isFinished());
        const uint id = QDBusReply<uint>(notify).value();
        QVERIFY(id != 0);
        QCOMPARE(queue.current()->summary, QStringLiteral("Hello"));
        QCOMPARE(queue.current()->body, QStringLiteral("World"));
        QVERIFY(!queue.current()->critical);

        // notify-send -u critical: the urgency hint is a byte.
        QDBusPendingCall urgent = call(QStringLiteral("Notify"),
            { QStringLiteral("app"), id, QString(), QStringLiteral("Hello"), QString(),
                QStringList(),
                QVariantMap({ { QStringLiteral("urgency"), QVariant::fromValue(uchar(2)) } }),
                -1 });
        QTRY_VERIFY(urgent.isFinished());
        QVERIFY(queue.current()->critical);

        call(QStringLiteral("CloseNotification"), { id });
        QTRY_COMPARE(m_closed.size(), 1);
        QCOMPARE(m_closed.first(), QVariantList({ id, 3u }));
        client = QDBusConnection(QStringLiteral("released"));
        QDBusConnection::disconnectFromBus(QStringLiteral("client"));

        // A second server cannot take the name.
        NotificationQueue other;
        NotificationService second(&other,
            QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("second")));
        QVERIFY(!second.registerOnBus());
    }

    void soundHints()
    {
        NotificationQueue queue;
        NotificationService service(&queue, QDBusConnection(QStringLiteral("none")));
        QSignalSpy sounds(&service, &NotificationService::soundRequested);
        const auto notify = [&service](const QVariantMap &hints, uint replaces = 0) {
            return service.Notify(QStringLiteral("app"), replaces, QString(),
                QStringLiteral("Hello"), QString(), {}, hints, -1);
        };

        // Without a hint, a new notification plays message-new-instant...
        const uint id = notify({});
        QCOMPARE(
            sounds.takeFirst(), QVariantList({ QStringLiteral("message-new-instant"), QString() }));
        // ...but an update to it stays quiet.
        notify({}, id);
        QCOMPARE(sounds.count(), 0);
        notify({ { QStringLiteral("sound-name"), QStringLiteral("dialog-warning") } });
        QCOMPARE(sounds.takeFirst(), QVariantList({ QStringLiteral("dialog-warning"), QString() }));
        // A file wins over a name; only absolute paths are taken.
        notify({ { QStringLiteral("sound-name"), QStringLiteral("bell") },
            { QStringLiteral("sound-file"), QStringLiteral("/tmp/ring.oga") } });
        QCOMPARE(sounds.takeFirst(), QVariantList({ QString(), QStringLiteral("/tmp/ring.oga") }));
        notify({ { QStringLiteral("sound-file"), QStringLiteral("ring.oga") } });
        QCOMPARE(
            sounds.takeFirst(), QVariantList({ QStringLiteral("message-new-instant"), QString() }));
        notify({ { QStringLiteral("sound-name"), QStringLiteral("dialog-warning") },
            { QStringLiteral("suppress-sound"), true } });
        QCOMPARE(sounds.count(), 0);
    }

public Q_SLOTS:
    void recordClosed(uint id, uint reason) { m_closed << QVariantList({ id, reason }); }

private:
    QList<QVariantList> m_closed;
};

QTEST_MAIN(TestNotifications)
#include "tst_notifications.moc"
