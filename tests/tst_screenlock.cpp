// SPDX-License-Identifier: GPL-3.0-or-later

#include "blankscreen.h"
#include "screenlock.h"
#include "screensaverservice.h"
#include "settings.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QDeadlineTimer>
#include <QFile>
#include <QMouseEvent>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

// A stand-in for swaylock or swayidle that records its arguments, one per
// line, and then sleeps for the given number of seconds (0: exits at once).
QString fakeProgram(const QTemporaryDir &dir, const QString &name, int sleepSeconds)
{
    const QString path = dir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" > '%1.args'\nexec sleep %2\n")
            .arg(path)
            .arg(sleepSeconds)
            .toUtf8());
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return path;
}

QStringList recordedArguments(const QString &program)
{
    QFile file(program + QStringLiteral(".args"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

QString value(const QStringList &arguments, const QString &option)
{
    const qsizetype i = arguments.indexOf(option);
    return i >= 0 && i + 1 < arguments.size() ? arguments.at(i + 1) : QString();
}

// Calls the service in this process without blocking it.
// (QDBusInterface would block introspecting it.)
template <typename T>
QDBusReply<T> callAndWait(const QDBusConnection &bus, const QString &path, const QString &method,
    const QList<QVariant> &arguments = {})
{
    QDBusMessage message
        = QDBusMessage::createMethodCall(QString::fromLatin1(ScreenSaverService::ServiceName), path,
            QStringLiteral("org.freedesktop.ScreenSaver"), method);
    message.setArguments(arguments);
    QDBusPendingCall call = bus.asyncCall(message);
    QDeadlineTimer deadline(5000);
    while (!call.isFinished() && !deadline.hasExpired())
        QTest::qWait(10);
    return QDBusReply<T>(call.reply());
}

} // namespace

class TestScreenLock : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void swaylockForLockWorkstation()
    {
        SchemeColours colours = standardColours();
        colours.insert(QStringLiteral("Background"), QColor(0x12, 0x34, 0x56));
        const QStringList arguments = swaylockArguments(LockKind::Workstation, colours);
        // Daemonised, so the shell knows when the screen is locked, and
        // never with the user's own configuration, which could undo that.
        QVERIFY(arguments.contains(QStringLiteral("--daemonize")));
        QCOMPARE(value(arguments, QStringLiteral("--config")), QStringLiteral("/dev/null"));
        QCOMPARE(value(arguments, QStringLiteral("--color")), QStringLiteral("123456"));
        QCOMPARE(value(arguments, QStringLiteral("--inside-color")), QStringLiteral("c0c0c0"));
        QCOMPARE(value(arguments, QStringLiteral("--key-hl-color")), QStringLiteral("000080"));
        QVERIFY(arguments.contains(QStringLiteral("--indicator-idle-visible")));
    }

    void swaylockForScreenSaver()
    {
        const QStringList arguments = swaylockArguments(LockKind::ScreenSaver, standardColours());
        QCOMPARE(value(arguments, QStringLiteral("--color")), QStringLiteral("000000"));
        QVERIFY(!arguments.contains(QStringLiteral("--indicator-idle-visible")));
    }

    void swayidleArgumentsTest()
    {
        const QString shell = QStringLiteral("/opt/it's here/daytona-shell");
        const QString quoted = QStringLiteral("'/opt/it'\\''s here/daytona-shell'");
        QCOMPARE(swayidleArguments(600, shell),
            (QStringList { QStringLiteral("-w"), QStringLiteral("timeout"), QStringLiteral("600"),
                quoted + QStringLiteral(" --screen-saver"), QStringLiteral("before-sleep"),
                quoted + QStringLiteral(" --lock"), QStringLiteral("lock"),
                quoted + QStringLiteral(" --lock") }));
        // No screen saver: still lock before sleep and when logind asks.
        QCOMPARE(swayidleArguments(0, shell),
            (QStringList { QStringLiteral("-w"), QStringLiteral("before-sleep"),
                quoted + QStringLiteral(" --lock"), QStringLiteral("lock"),
                quoted + QStringLiteral(" --lock") }));
    }

    void lockerReportsWhenLocked()
    {
        QTemporaryDir dir;
        ScreenLocker locker;
        locker.setProgram(fakeProgram(dir, QStringLiteral("swaylock"), 0));
        QVERIFY(locker.isAvailable());
        int done = 0;
        locker.lock(LockKind::Workstation, [&done] { ++done; });
        // A second request while the first is starting waits for it.
        locker.lock(LockKind::Workstation, [&done] { ++done; });
        QTRY_COMPARE(done, 2);
        QVERIFY(recordedArguments(dir.filePath(QStringLiteral("swaylock")))
                .contains(QStringLiteral("--daemonize")));
    }

    void lockerWithoutSwaylockStillAnswers()
    {
        ScreenLocker locker;
        locker.setProgram(QStringLiteral("/nonexistent/swaylock"));
        QVERIFY(!locker.isAvailable());
        bool done = false;
        locker.lock(LockKind::Workstation, [&done] { done = true; });
        QTRY_VERIFY(done); // a suspend waiting on it is not held up
    }

    void idleMonitorFollowsSettingsAndInhibits()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        settings.setScreenSaverWait(2);
        const QString program = fakeProgram(dir, QStringLiteral("swayidle"), 60);
        IdleMonitor idle(&settings, QStringLiteral("/usr/bin/daytona-shell"));
        idle.setProgram(program);
        QVERIFY(idle.start());
        QTRY_COMPARE(recordedArguments(program).mid(0, 3),
            (QStringList {
                QStringLiteral("-w"), QStringLiteral("timeout"), QStringLiteral("120") }));
        QProcess *first = idle.process();
        QVERIFY(first);

        // Applied live: swayidle is restarted with the new wait.
        settings.setScreenSaverWait(5);
        QTRY_COMPARE(recordedArguments(program).value(2), QStringLiteral("300"));
        QVERIFY(idle.process() != first);

        // Inhibited: no timeout, but still the lock before sleep.
        idle.setInhibited(true);
        QTRY_COMPARE(recordedArguments(program).value(1), QStringLiteral("before-sleep"));
        idle.setInhibited(false);
        QTRY_COMPARE(recordedArguments(program).value(1), QStringLiteral("timeout"));

        // (None): likewise no timeout.
        settings.setScreenSaver(ScreenSaver::None);
        QTRY_COMPARE(recordedArguments(program).value(1), QStringLiteral("before-sleep"));

        // Unchanged settings do not restart it.
        QProcess *current = idle.process();
        settings.setScreenSaverWait(6);
        QCOMPARE(idle.process(), current);
    }

    void idleMonitorWithoutSwayidle()
    {
        IdleMonitor idle(nullptr, QStringLiteral("daytona-shell"));
        idle.setProgram(QStringLiteral("/nonexistent/swayidle"));
        QVERIFY(!idle.start());
        QVERIFY(!idle.process());
    }

    void inhibitOverDBus()
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        ScreenSaverService service(bus);
        QVERIFY(service.registerOnBus());
        QSignalSpy changed(&service, &ScreenSaverService::inhibitedChanged);

        // A separate connection, as a video player would have.
        QDBusConnection client
            = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("player"));
        QVERIFY(client.isConnected());
        for (const QString &path :
            { QStringLiteral("/org/freedesktop/ScreenSaver"), QStringLiteral("/ScreenSaver") }) {
            QDBusReply<uint> cookie = callAndWait<uint>(client, path, QStringLiteral("Inhibit"),
                { QStringLiteral("vlc"), QStringLiteral("Video") });
            QVERIFY(cookie.isValid());
            QTRY_VERIFY(service.isInhibited());
            QCOMPARE(changed.count(), 1);
            QCOMPARE(changed.takeFirst().first().toBool(), true);

            // Only the program that took it can end it.
            callAndWait<void>(bus, path, QStringLiteral("UnInhibit"), { cookie.value() });
            QVERIFY(service.isInhibited());

            callAndWait<void>(client, path, QStringLiteral("UnInhibit"), { cookie.value() });
            QTRY_VERIFY(!service.isInhibited());
            QCOMPARE(changed.takeFirst().first().toBool(), false);
        }

        // A program that leaves the bus without UnInhibit loses its inhibit.
        {
            QDBusConnection leaving = QDBusConnection::connectToBus(
                QDBusConnection::SessionBus, QStringLiteral("leaving"));
            QDBusReply<uint> cookie = callAndWait<uint>(leaving, QStringLiteral("/ScreenSaver"),
                QStringLiteral("Inhibit"), { QStringLiteral("vlc"), QStringLiteral("Video") });
            QVERIFY(cookie.isValid());
            QTRY_VERIFY(service.isInhibited());
        }
        QDBusConnection::disconnectFromBus(QStringLiteral("leaving"));
        QTRY_VERIFY(!service.isInhibited());
    }

    void blankScreenEndsOnInput()
    {
        BlankScreen blank;
        const auto move = [](int x, int y) {
            return QMouseEvent(QEvent::MouseMove, QPointF(x, y), QPointF(x, y), Qt::NoButton,
                Qt::NoButton, Qt::NoModifier);
        };
        // Not started: no grace period. The first position is only noted,
        // and a small drift is ignored.
        QMouseEvent first = move(100, 100);
        QVERIFY(!blank.endsScreenSaver(&first));
        QMouseEvent drift = move(102, 101);
        QVERIFY(!blank.endsScreenSaver(&drift));
        QMouseEvent moved = move(140, 100);
        QVERIFY(blank.endsScreenSaver(&moved));
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier);
        QVERIFY(blank.endsScreenSaver(&key));
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
        QVERIFY(!blank.endsScreenSaver(&release));
    }
};

QTEST_MAIN(TestScreenLock)
#include "tst_screenlock.moc"
