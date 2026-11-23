// SPDX-License-Identifier: GPL-3.0-or-later

#include "shutdowndialog.h"

#include <QRadioButton>
#include <QTest>

using namespace Daytona;

namespace {

class FakeActions : public SessionActions
{
public:
    bool canShutDown = true;
    bool canRestart = true;
    QList<SessionAction> performed;
    QString failWith;

    void queryAvailability(Availability done) override { done(canShutDown, canRestart); }
    void perform(SessionAction action, Failure failed) override
    {
        performed << action;
        if (!failWith.isEmpty())
            failed(failWith);
    }
};

} // namespace

class TestShutDownDialog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsToShutDown()
    {
        FakeActions actions;
        ShutDownDialog dialog(&actions);
        dialog.prepare();
        QVERIFY(dialog.option(SessionAction::ShutDown)->isEnabled());
        QCOMPARE(dialog.chosen(), SessionAction::ShutDown);
        dialog.accept();
        QCOMPARE(actions.performed, QList<SessionAction> { SessionAction::ShutDown });
    }

    void unavailableOptionsAreDisabled()
    {
        FakeActions actions;
        actions.canShutDown = false;
        actions.canRestart = false;
        ShutDownDialog dialog(&actions);
        dialog.prepare();
        QVERIFY(!dialog.option(SessionAction::ShutDown)->isEnabled());
        QVERIFY(!dialog.option(SessionAction::Restart)->isEnabled());
        QVERIFY(dialog.option(SessionAction::LogOff)->isEnabled());
        QCOMPARE(dialog.chosen(), SessionAction::LogOff); // falls back to what is possible
    }

    void remembersLastChoice()
    {
        FakeActions actions;
        ShutDownDialog dialog(&actions);
        dialog.prepare();
        dialog.option(SessionAction::Restart)->setChecked(true);
        dialog.accept();
        dialog.prepare();
        QCOMPARE(dialog.chosen(), SessionAction::Restart);
        QCOMPARE(actions.performed, QList<SessionAction> { SessionAction::Restart });
    }

    void noDoesNothing()
    {
        FakeActions actions;
        ShutDownDialog dialog(&actions);
        dialog.prepare();
        dialog.reject();
        QVERIFY(actions.performed.isEmpty());
    }

    void failureIsShown()
    {
        FakeActions actions;
        actions.failWith = QStringLiteral("Interactive authentication required.");
        ShutDownDialog dialog(&actions);
        QString shown;
        dialog.showError = [&](const QString &, const QString &message) { shown = message; };
        dialog.prepare();
        dialog.accept();
        QCOMPARE(shown, actions.failWith);
    }
};

QTEST_MAIN(TestShutDownDialog)
#include "tst_shutdowndialog.moc"
