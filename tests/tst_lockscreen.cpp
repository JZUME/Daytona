// SPDX-License-Identifier: GPL-3.0-or-later

#include "daytonastyle.h"
#include "lockui.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>

using namespace Daytona;

namespace {

const LockUser User { QStringLiteral("casey"), QStringLiteral("Casey Jones") };

// Accepts "secret", answering on the next turn of the event loop as PAM
// does from its thread.
LockController::Authenticate fakePam(QStringList *tried)
{
    return [tried](const QString &password, std::function<void(bool)> done) {
        tried->append(password);
        QMetaObject::invokeMethod(
            qApp, [password, done] { done(password == QLatin1String("secret")); },
            Qt::QueuedConnection);
    };
}

void pressCtrlAltDelete(QWidget *screen)
{
    QTest::keyClick(screen->windowHandle(), Qt::Key_Delete, Qt::ControlModifier | Qt::AltModifier);
}

void type(QWidget *screen, const QString &text)
{
    for (const QChar c : text)
        QTest::keyClick(screen->windowHandle(), c.toLatin1());
}

} // namespace

class TestLockScreen : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QApplication::setStyle(new Style); }

    void workstationLockedNeedsCtrlAltDelete()
    {
        QStringList tried;
        LockController controller(User, false, fakePam(&tried));
        QWidget *screen = controller.addScreen(QGuiApplication::primaryScreen());
        screen->show();
        QVERIFY(QTest::qWaitForWindowExposed(screen));
        LockPrompt *prompt = controller.prompt();
        QTRY_VERIFY(prompt->isVisible());
        QCOMPARE(prompt->parentWidget(), screen);
        QCOMPARE(prompt->page(), LockPrompt::Page::Locked);
        QCOMPARE(prompt->title(), QStringLiteral("Workstation Locked"));
        QCOMPARE(screen->palette().color(QPalette::Window), LockController::backgroundColour());
        // Centred on the screen.
        QVERIFY(qAbs(prompt->geometry().center().x() - screen->rect().center().x()) <= 1);

        // Typing does nothing until Ctrl+Alt+Delete.
        type(screen, QStringLiteral("abc"));
        QCOMPARE(prompt->page(), LockPrompt::Page::Locked);
        pressCtrlAltDelete(screen);
        QCOMPARE(prompt->page(), LockPrompt::Page::Unlock);
        QCOMPARE(prompt->title(), QStringLiteral("Unlock Workstation"));
        QTRY_VERIFY(prompt->passwordField()->hasFocus());

        // Escape (or Cancel) goes back.
        QTest::keyClick(screen->windowHandle(), Qt::Key_Escape);
        QCOMPARE(prompt->page(), LockPrompt::Page::Locked);
        pressCtrlAltDelete(screen);
        prompt->cancelButton()->click();
        QCOMPARE(prompt->page(), LockPrompt::Page::Locked);
        QVERIFY(tried.isEmpty());
    }

    void wrongThenRightPassword()
    {
        QStringList tried;
        LockController controller(User, false, fakePam(&tried));
        QWidget *screen = controller.addScreen(QGuiApplication::primaryScreen());
        screen->show();
        QVERIFY(QTest::qWaitForWindowExposed(screen));
        LockPrompt *prompt = controller.prompt();
        QSignalSpy unlocked(&controller, &LockController::unlocked);

        pressCtrlAltDelete(screen);
        QTRY_VERIFY(prompt->passwordField()->hasFocus());
        type(screen, QStringLiteral("wrong"));
        QTest::keyClick(screen->windowHandle(), Qt::Key_Return);
        QTRY_COMPARE(prompt->page(), LockPrompt::Page::Failed);
        QCOMPARE(tried, QStringList { QStringLiteral("wrong") });
        QCOMPARE(unlocked.count(), 0);

        // OK (Enter) returns to an empty password box.
        QTest::keyClick(screen->windowHandle(), Qt::Key_Return);
        QCOMPARE(prompt->page(), LockPrompt::Page::Unlock);
        QVERIFY(prompt->passwordField()->text().isEmpty());
        QTRY_VERIFY(prompt->passwordField()->hasFocus());

        type(screen, QStringLiteral("secret"));
        prompt->okButton()->click();
        QTRY_COMPARE(unlocked.count(), 1);
    }

    void screenSaverStartsBlank()
    {
        QStringList tried;
        LockController controller(User, true, fakePam(&tried));
        QWidget *screen = controller.addScreen(QGuiApplication::primaryScreen());
        screen->show();
        QVERIFY(QTest::qWaitForWindowExposed(screen));
        QVERIFY(controller.isBlank());
        QVERIFY(!controller.prompt()->isVisible());
        QCOMPARE(screen->palette().color(QPalette::Window), QColor(Qt::black));

        // The key that wakes it is not typed anywhere.
        QTest::keyClick(screen->windowHandle(), Qt::Key_A);
        QVERIFY(!controller.isBlank());
        QTRY_VERIFY(controller.prompt()->isVisible());
        QCOMPARE(controller.prompt()->page(), LockPrompt::Page::Locked);
        QCOMPARE(screen->palette().color(QPalette::Window), LockController::backgroundColour());

        // Left alone, it goes back to black.
        controller.setIdleTimeout(50);
        pressCtrlAltDelete(screen);
        QTRY_VERIFY(controller.isBlank());
        QCOMPARE(controller.prompt()->page(), LockPrompt::Page::Locked);
        QVERIFY(!controller.prompt()->isVisible());
    }

    void promptFollowsRemovedScreen()
    {
        QStringList tried;
        LockController controller(User, false, fakePam(&tried));
        QWidget *first = controller.addScreen(QGuiApplication::primaryScreen());
        QCOMPARE(controller.prompt()->parentWidget(), first);
        QWidget *second = controller.addScreen(nullptr);
        QCOMPARE(controller.screens().size(), 2);
        controller.removeScreen(QGuiApplication::primaryScreen());
        QCOMPARE(controller.screens(), QList<QWidget *> { second });
        QCOMPARE(controller.prompt()->parentWidget(), second);
    }

    void userNamedInTheDialog()
    {
        LockPrompt prompt(User);
        QStringList texts;
        for (const QLabel *label : prompt.findChildren<QLabel *>())
            texts << label->text();
        QVERIFY(texts.contains(
            QStringLiteral("Only casey (Casey Jones) can unlock this workstation.")));
        LockPrompt plain(LockUser { QStringLiteral("casey"), QString() });
        texts.clear();
        for (const QLabel *label : plain.findChildren<QLabel *>())
            texts << label->text();
        QVERIFY(texts.contains(QStringLiteral("Only casey can unlock this workstation.")));
    }
};

QTEST_MAIN(TestLockScreen)
#include "tst_lockscreen.moc"
