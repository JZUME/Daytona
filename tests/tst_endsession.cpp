// SPDX-License-Identifier: GPL-3.0-or-later

#include "endsession.h"
#include "shutdowndialog.h"
#include "toplevellist.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>

using namespace Daytona;

namespace {

void open(ToplevelList &list, quint32 id, const QString &title, quint32 parent = 0)
{
    list.create(id);
    list.setTitle(id, title);
    if (parent)
        list.setParentToplevel(id, parent);
    list.done(id);
}

// A ProgramCloser over a list, recording close requests and prompts.
struct Fixture
{
    ToplevelList list;
    QList<quint32> closeRequests;
    ProgramCloser closer { &list, [this](quint32 id) { closeRequests << id; } };
    QStringList prompts;
    int hidden = 0;
    int finished = 0;

    Fixture()
    {
        closer.setTimeout(50);
        closer.showPrompt = [this](const QString &program) { prompts << program; };
        closer.hidePrompt = [this] { ++hidden; };
    }

    void closeAll()
    {
        closer.closeAll([this] { ++finished; });
    }
};

} // namespace

class TestEndSession : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void noWindowsEndsAtOnce()
    {
        Fixture f;
        f.closeAll();
        QCOMPARE(f.finished, 1);
        QVERIFY(!f.closer.isClosing());
    }

    void asksOwnerWindowsAndWaitsForThem()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        open(f.list, 2, QStringLiteral("Save As"), 1);
        open(f.list, 3, QStringLiteral("Terminal"));
        f.closeAll();
        // Dialogs go with their program; closing one would only cancel it.
        QCOMPARE(f.closeRequests, (QList<quint32> { 1, 3 }));
        QCOMPARE(f.finished, 0);
        f.list.close(3);
        QCOMPARE(f.finished, 0);
        f.list.close(2);
        f.list.close(1);
        QCOMPARE(f.finished, 1);
        QVERIFY(f.prompts.isEmpty());
    }

    void windowsOpenedMeanwhileAreNotWaitedFor()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        f.closeAll();
        open(f.list, 2, QStringLiteral("Something new"));
        f.list.close(1);
        QCOMPARE(f.finished, 1);
    }

    void promptsForAProgramThatHasNotClosed()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        f.closeAll();
        QTRY_COMPARE(f.prompts, QStringList { QStringLiteral("Notes") });
        QCOMPARE(f.closer.asking(), 1u);
        f.closer.answer(EndTaskDialog::EndTask);
        QCOMPARE(f.finished, 1);
    }

    void promptGoesAwayWhenTheProgramCloses()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        f.closeAll();
        QTRY_COMPARE(f.prompts.size(), 1);
        // The user answered the program's own "save changes?" question.
        f.list.close(1);
        QCOMPARE(f.hidden, 1);
        QCOMPARE(f.finished, 1);
    }

    void endTaskMovesOnToTheNextProgram()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        open(f.list, 2, QString());
        f.list.setAppId(2, QStringLiteral("org.example.Paint"));
        f.list.done(2);
        f.closeAll();
        QTRY_COMPARE(f.prompts.size(), 1);
        f.closer.answer(EndTaskDialog::EndTask);
        // The others have had the same time, so no second wait.
        QCOMPARE(f.prompts,
            (QStringList { QStringLiteral("Notes"), QStringLiteral("org.example.Paint") }));
        QCOMPARE(f.finished, 0);
        f.list.close(2);
        QCOMPARE(f.finished, 1);
    }

    void waitGivesMoreTime()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        f.closeAll();
        QTRY_COMPARE(f.prompts.size(), 1);
        f.closer.answer(EndTaskDialog::Wait);
        QCOMPARE(f.closer.asking(), 0u);
        QTRY_COMPARE(f.prompts.size(), 2);
        f.list.close(1);
        QCOMPARE(f.finished, 1);
    }

    void cancelKeepsTheSession()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        f.closeAll();
        QTRY_COMPARE(f.prompts.size(), 1);
        f.closer.answer(EndTaskDialog::Cancel);
        QVERIFY(!f.closer.isClosing());
        f.list.close(1);
        QTest::qWait(100);
        QCOMPARE(f.finished, 0);
        QCOMPARE(f.prompts.size(), 1);
    }

    void endTaskDialogAnswers()
    {
        ToplevelList list;
        open(list, 1, QStringLiteral("Notes"));
        QList<quint32> requests;
        ProgramCloser closer(&list, [&](quint32 id) { requests << id; });
        closer.setTimeout(10);
        int finished = 0;
        closer.closeAll([&] { ++finished; });
        auto shown = [] {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (auto *dialog = qobject_cast<EndTaskDialog *>(w); dialog && dialog->isVisible())
                    return dialog;
            }
            return static_cast<EndTaskDialog *>(nullptr);
        };
        QTRY_VERIFY(shown());
        EndTaskDialog *dialog = shown();
        QCOMPARE(requests, QList<quint32> { 1 });
        QCOMPARE(dialog->windowTitle(), QStringLiteral("End Task - Notes"));
        dialog->done(EndTaskDialog::EndTask);
        QCOMPARE(finished, 1);
    }

    void sessionActionsCloseProgramsFirst()
    {
        qunsetenv("LABWC_PID");
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        SessionActions actions;
        actions.setProgramCloser(&f.closer);
        QStringList failures;
        actions.perform(SessionAction::LogOff, [&](const QString &m) { failures << m; });
        QCOMPARE(f.closeRequests, QList<quint32> { 1 });
        QVERIFY(failures.isEmpty()); // labwc not signalled yet
        f.list.close(1);
        // Now it tries, and without LABWC_PID cannot.
        QCOMPARE(failures.size(), 1);
    }

    void lockKeepsPrograms()
    {
        Fixture f;
        open(f.list, 1, QStringLiteral("Notes"));
        SessionActions actions;
        actions.setProgramCloser(&f.closer);
        QSignalSpy locked(&actions, &SessionActions::lockRequested);
        actions.perform(SessionAction::Lock, [](const QString &) { QFAIL("failed"); });
        QCOMPARE(locked.count(), 1);
        QVERIFY(f.closeRequests.isEmpty());
    }
};

QTEST_MAIN(TestEndSession)
#include "tst_endsession.moc"
