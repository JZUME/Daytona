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
};

QTEST_GUILESS_MAIN(TestToplevelList)
#include "tst_toplevellist.moc"
