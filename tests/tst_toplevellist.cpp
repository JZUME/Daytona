// SPDX-License-Identifier: GPL-3.0-or-later

#include "toplevellist.h"

#include <QSignalSpy>
#include <QTest>

using Daytona::ToplevelList;

namespace {

QByteArray states(std::initializer_list<quint32> values)
{
    QByteArray bytes;
    for (quint32 v : values)
        bytes.append(reinterpret_cast<const char *>(&v), sizeof(v));
    return bytes;
}

// An owner window (1) with a dialog (2), both announced.
void openOwnerAndDialog(ToplevelList &list)
{
    list.create(1);
    list.done(1);
    list.create(2);
    list.setParentToplevel(2, 1);
    list.done(2);
}

} // namespace

class TestToplevelList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void windowAppearsOnlyAfterDone()
    {
        ToplevelList list;
        QSignalSpy added(&list, &ToplevelList::added);
        list.create(1);
        list.setTitle(1, QStringLiteral("Notepad"));
        list.setAppId(1, QStringLiteral("org.example.notepad"));
        QVERIFY(list.toplevels().isEmpty());
        QVERIFY(!list.find(1));
        list.done(1);
        QCOMPARE(added.count(), 1);
        QCOMPARE(list.toplevels().size(), 1);
        QCOMPARE(list.find(1)->title, QStringLiteral("Notepad"));
        QCOMPARE(list.find(1)->appId, QStringLiteral("org.example.notepad"));
    }

    void changesApplyTogetherOnDone()
    {
        ToplevelList list;
        list.create(1);
        list.setTitle(1, QStringLiteral("Old"));
        list.done(1);
        QSignalSpy changed(&list, &ToplevelList::changed);
        list.setTitle(1, QStringLiteral("New"));
        list.setState(1, states({ ToplevelList::Activated, ToplevelList::Maximized }));
        QCOMPARE(list.find(1)->title, QStringLiteral("Old")); // not yet
        QVERIFY(!list.find(1)->activated);
        list.done(1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(list.find(1)->title, QStringLiteral("New"));
        QVERIFY(list.find(1)->activated);
        QVERIFY(list.find(1)->maximized);
        QVERIFY(!list.find(1)->minimized);
    }

    void doneWithoutChangesIsSilent()
    {
        ToplevelList list;
        list.create(1);
        list.done(1);
        QSignalSpy changed(&list, &ToplevelList::changed);
        list.setTitle(1, QString());
        list.done(1);
        QCOMPARE(changed.count(), 0);
    }

    void stateReplacesPreviousState()
    {
        ToplevelList list;
        list.create(1);
        list.setState(1, states({ ToplevelList::Minimized, ToplevelList::Fullscreen }));
        list.done(1);
        QVERIFY(list.find(1)->minimized);
        QVERIFY(list.find(1)->fullscreen);
        list.setState(1, states({}));
        list.done(1);
        QVERIFY(!list.find(1)->minimized);
        QVERIFY(!list.find(1)->fullscreen);
    }

    void unknownStatesAreIgnored()
    {
        ToplevelList list;
        list.create(1);
        list.setState(1, states({ 99, ToplevelList::Activated }) + QByteArray("x", 1));
        list.done(1);
        QVERIFY(list.find(1)->activated);
    }

    void launchOrderIsFirstAppearance()
    {
        ToplevelList list;
        for (quint32 id : { 3u, 1u, 2u })
            list.create(id);
        // Announced in the order of their first "done".
        list.done(2);
        list.done(3);
        list.done(1);
        list.setTitle(2, QStringLiteral("changed"));
        list.done(2); // a change does not move it
        QList<quint32> ids;
        for (const auto &t : list.toplevels())
            ids.append(t.id);
        QCOMPARE(ids, (QList<quint32> { 2, 3, 1 }));
    }

    void closeRemoves()
    {
        ToplevelList list;
        list.create(1);
        list.create(2);
        list.done(1);
        list.done(2);
        QSignalSpy removed(&list, &ToplevelList::removed);
        list.close(1);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0).at(0).toUInt(), 1u);
        QCOMPARE(list.toplevels().size(), 1);
        QVERIFY(!list.find(1));
        list.done(1); // events after close are ignored
        QCOMPARE(list.toplevels().size(), 1);
    }

    void closeBeforeDoneIsSilent()
    {
        ToplevelList list;
        QSignalSpy removed(&list, &ToplevelList::removed);
        list.create(1);
        list.close(1);
        QCOMPARE(removed.count(), 0);
    }

    void untrustedTextIsCapped()
    {
        ToplevelList list;
        list.create(1);
        list.setTitle(1, QString(100000, QLatin1Char('x')));
        list.setAppId(1, QString(100000, QLatin1Char('y')));
        list.done(1);
        QCOMPARE(list.find(1)->title.size(), ToplevelList::MaxTextLength);
        QCOMPARE(list.find(1)->appId.size(), ToplevelList::MaxTextLength);
    }

    void eventsForUnknownIdsAreIgnored()
    {
        ToplevelList list;
        list.setTitle(7, QStringLiteral("ghost"));
        list.setState(7, states({ ToplevelList::Activated }));
        list.done(7);
        list.close(7);
        QVERIFY(list.toplevels().isEmpty());
    }

    void childIsHidden()
    {
        ToplevelList list;
        openOwnerAndDialog(list);
        QCOMPARE(list.toplevels().size(), 2);
        QCOMPARE(list.find(2)->parent, 1u);
        QCOMPARE(list.find(1)->parent, 0u);
        QCOMPARE(list.owners(), (QList<quint32> { 1 }));
        QCOMPARE(list.rootOwner(2), 1u);
        QCOMPARE(list.rootOwner(1), 1u);
    }

    void parentSetAfterChildAppeared()
    {
        ToplevelList list;
        list.create(1);
        list.done(1);
        list.create(2);
        list.done(2);
        QSignalSpy changed(&list, &ToplevelList::changed);
        list.setParentToplevel(2, 1);
        QCOMPARE(list.find(2)->parent, 0u); // not yet
        list.done(2);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toUInt(), 2u);
        QCOMPARE(list.owners(), (QList<quint32> { 1 }));
    }

    void childAnnouncedBeforeParent()
    {
        // The child's handle can be announced before its parent's.
        ToplevelList list;
        list.create(1);
        list.done(1);
        QCOMPARE(list.owners(), (QList<quint32> { 1 }));
        list.create(2);
        list.done(2);
        list.setParentToplevel(1, 2);
        list.done(1);
        QCOMPARE(list.owners(), (QList<quint32> { 2 }));
        QCOMPARE(list.rootOwner(1), 2u);
    }

    void parentCleared()
    {
        ToplevelList list;
        openOwnerAndDialog(list);
        QSignalSpy changed(&list, &ToplevelList::changed);
        list.setParentToplevel(2, 0);
        list.done(2);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(list.find(2)->parent, 0u);
        // Back in launch order.
        QCOMPARE(list.owners(), (QList<quint32> { 1, 2 }));
    }

    void childOfClosedParentBecomesOwner()
    {
        ToplevelList list;
        openOwnerAndDialog(list);
        QSignalSpy changed(&list, &ToplevelList::changed);
        list.close(1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).toUInt(), 2u);
        QCOMPARE(list.find(2)->parent, 0u);
        QCOMPARE(list.owners(), (QList<quint32> { 2 }));
        // A later done must not bring back the closed parent.
        list.setTitle(2, QStringLiteral("Save As"));
        list.done(2);
        QCOMPARE(list.find(2)->parent, 0u);
    }

    void pendingChildOfClosedParentLosesParent()
    {
        ToplevelList list;
        list.create(1);
        list.done(1);
        list.create(2);
        list.setParentToplevel(2, 1);
        list.close(1);
        list.done(2);
        QCOMPARE(list.find(2)->parent, 0u);
        QCOMPARE(list.owners(), (QList<quint32> { 2 }));
    }

    void ownerIsActiveWhileItsDialogIs()
    {
        ToplevelList list;
        openOwnerAndDialog(list);
        list.create(3);
        list.setParentToplevel(3, 2); // a message box from the dialog
        list.done(3);
        QCOMPARE(list.activeOwner(), 0u);
        list.setState(3, states({ ToplevelList::Activated }));
        list.done(3);
        QCOMPARE(list.rootOwner(3), 1u);
        QCOMPARE(list.activeOwner(), 1u);
        list.setState(3, states({}));
        list.done(3);
        list.setState(1, states({ ToplevelList::Activated }));
        list.done(1);
        QCOMPARE(list.activeOwner(), 1u);
    }

    void parentLoopsDoNotHang()
    {
        ToplevelList list;
        list.create(1);
        list.create(2);
        list.setParentToplevel(1, 2);
        list.setParentToplevel(2, 1);
        list.done(1);
        list.done(2);
        list.create(3);
        list.setParentToplevel(3, 3); // its own parent: ignored
        list.done(3);
        QCOMPARE(list.find(3)->parent, 0u);
        QCOMPARE(list.owners(), (QList<quint32> { 3 }));
        const quint32 root = list.rootOwner(1);
        QVERIFY(root == 1 || root == 2);
    }

    void unknownParentHidesUntilCleared()
    {
        // A parent that has not been announced yet still makes the window
        // a child.
        ToplevelList list;
        list.create(1);
        list.create(2);
        list.setParentToplevel(2, 1);
        list.done(2);
        QVERIFY(list.owners().isEmpty());
        QCOMPARE(list.rootOwner(2), 2u);
        list.done(1);
        QCOMPARE(list.owners(), (QList<quint32> { 1 }));
        QCOMPARE(list.rootOwner(2), 1u);
    }

    void outputsFollowEnterAndLeave()
    {
        ToplevelList list;
        list.create(1);
        list.outputEnter(1, QStringLiteral("DP-1"));
        list.outputEnter(1, QStringLiteral("HDMI-A-1"));
        list.outputEnter(1, QStringLiteral("DP-1")); // already on it
        list.outputEnter(1, QString()); // a screen Qt does not know
        list.done(1);
        QCOMPARE(list.find(1)->outputs, QStringList({ "DP-1", "HDMI-A-1" }));
        QSignalSpy changed(&list, &ToplevelList::changed);
        list.outputLeave(1, QStringLiteral("DP-1"));
        list.done(1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(list.find(1)->outputs, QStringList({ "HDMI-A-1" }));
    }

    void activeOutputIsTheActiveWindowsFirstOutput()
    {
        ToplevelList list;
        QCOMPARE(list.activeOutput(), QString());
        list.create(1);
        list.outputEnter(1, QStringLiteral("DP-1"));
        list.done(1);
        list.create(2);
        list.outputEnter(2, QStringLiteral("HDMI-A-1"));
        list.outputEnter(2, QStringLiteral("DP-1"));
        list.done(2);
        QCOMPARE(list.activeOutput(), QString()); // nothing active
        list.setState(2, states({ ToplevelList::Activated }));
        list.done(2);
        QCOMPARE(list.activeOutput(), QStringLiteral("HDMI-A-1"));
        list.create(3); // active, but not yet on any output
        list.setState(2, states({}));
        list.done(2);
        list.setState(3, states({ ToplevelList::Activated }));
        list.done(3);
        QCOMPARE(list.activeOutput(), QString());
    }
};

QTEST_GUILESS_MAIN(TestToplevelList)
#include "tst_toplevellist.moc"
