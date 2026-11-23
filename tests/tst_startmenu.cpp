// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"
#include "startmenu.h"

#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

DesktopEntry entry(const QString &name, const QStringList &categories)
{
    DesktopEntry e;
    e.name = name;
    e.categories = categories;
    e.type = QStringLiteral("Application");
    e.exec = QStringLiteral("true");
    return e;
}

QStringList texts(const QList<StartMenu::Item> &items)
{
    QStringList out;
    for (const auto &item : items)
        out << (item.separator ? QStringLiteral("-") : item.text);
    return out;
}

} // namespace

class TestStartMenu : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private Q_SLOTS:
    void groupsFollowMainCategoryOrder()
    {
        const QList<ProgramGroup> groups = groupPrograms({
            entry(QStringLiteral("Paint"), { QStringLiteral("Graphics") }),
            entry(QStringLiteral("Notepad"),
                { QStringLiteral("Utility"), QStringLiteral("TextEditor") }),
            entry(QStringLiteral("Odd"), { QStringLiteral("GTK") }),
            entry(QStringLiteral("Calc"), { QStringLiteral("Utility") }),
        });
        QCOMPARE(groups.size(), 3);
        QCOMPARE(groups[0].key, QStringLiteral("Utility"));
        QCOMPARE(groups[0].displayName, QStringLiteral("Accessories"));
        QCOMPARE(groups[0].entries.size(), 2);
        QCOMPARE(groups[1].key, QStringLiteral("Graphics"));
        QCOMPARE(groups[2].key, QString()); // uncategorised last
        QCOMPARE(groups[2].entries.first().name, QStringLiteral("Odd"));
    }

    void itemsInNtOrder()
    {
        StartMenu menu(nullptr, nullptr);
        QCOMPARE(texts(menu.items()),
            (QStringList { QStringLiteral("&Programs"), QStringLiteral("&Documents"),
                QStringLiteral("&Settings"), QStringLiteral("&Find"), QStringLiteral("&Run..."),
                QStringLiteral("-"), QStringLiteral("Sh&ut Down...") }));
        QVERIFY(menu.items()[4].enabled); // Run...
        QVERIFY(menu.items()[6].enabled); // Shut Down...
        QVERIFY(menu.items()[0].submenu);
    }

    void emptyProgramsSaysSo()
    {
        StartMenu menu(nullptr, nullptr);
        const QList<QAction *> actions = menu.programsMenu()->actions();
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions.first()->text(), QStringLiteral("(Empty)"));
        QVERIFY(!actions.first()->isEnabled());
    }

    void commandItemsFollowSettings()
    {
        Settings settings(m_dir.filePath(QStringLiteral("daytona.ini")));
        {
            StartMenu menu(nullptr, &settings);
            QVERIFY(!menu.controlPanelAction()->isEnabled());
            QVERIFY(!menu.findAction()->isEnabled());
        }
        settings.setControlPanelCommand(QStringLiteral("control-panel"));
        settings.setFindCommand(QStringLiteral("catfish --path ~"));
        StartMenu menu(nullptr, &settings);
        QVERIFY(menu.controlPanelAction()->isEnabled());
        QVERIFY(menu.findAction()->isEnabled());
    }

    void keyboardSkipsSeparator()
    {
        StartMenu menu(nullptr, nullptr);
        QCOMPARE(menu.currentIndex(), -1);
        QTest::keyClick(&menu, Qt::Key_Down);
        QCOMPARE(menu.currentIndex(), 0);
        QTest::keyClick(&menu, Qt::Key_Up); // wraps to the bottom item
        QCOMPARE(menu.currentIndex(), 6);
        QTest::keyClick(&menu, Qt::Key_Up); // skips the separator
        QCOMPARE(menu.currentIndex(), 4);
    }

    void mnemonicActivates()
    {
        StartMenu menu(nullptr, nullptr);
        QSignalSpy shutDown(&menu, &StartMenu::shutDownRequested);
        QSignalSpy run(&menu, &StartMenu::runRequested);
        QTest::keyClick(&menu, Qt::Key_U);
        QCOMPARE(shutDown.count(), 1);
        QTest::keyClick(&menu, Qt::Key_R);
        QCOMPARE(run.count(), 1);
    }
};

QTEST_MAIN(TestStartMenu)
#include "tst_startmenu.moc"
