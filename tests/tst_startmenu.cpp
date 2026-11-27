// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationindex.h"
#include "settings.h"
#include "startmenu.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

void writeApp(const QString &path, const char *name, const char *categories)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray("[Desktop Entry]\nType=Application\nExec=true\nName=") + name
        + "\nCategories=" + categories + "\n");
}

QStringList actionTexts(QMenu *menu)
{
    QStringList out;
    for (QAction *action : menu->actions())
        out << action->text();
    return out;
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
    void programsFollowTheMenuFile()
    {
        // The built-in Daytona menu over a small set of applications.
        QTemporaryDir root;
        const QString apps = root.filePath(QStringLiteral("share/applications"));
        writeApp(apps + QStringLiteral("/paint.desktop"), "Paint", "Graphics;");
        writeApp(apps + QStringLiteral("/notepad.desktop"), "Notepad", "Utility;TextEditor;");
        writeApp(apps + QStringLiteral("/calc.desktop"), "Calc", "Utility;");
        writeApp(apps + QStringLiteral("/odd.desktop"), "Odd", "GTK;");
        XdgMenuPaths paths;
        paths.configDirs = { root.filePath(QStringLiteral("config")) };
        paths.dataDirs = { root.filePath(QStringLiteral("share")) };
        qunsetenv("XDG_MENU_PREFIX");
        ApplicationIndex index;
        QSignalSpy changed(&index, &ApplicationIndex::changed);
        index.start({ apps }, paths);
        QVERIFY(changed.wait());

        Settings settings(m_dir.filePath(QStringLiteral("programs.ini")));
        StartMenu menu(&index, &settings);
        QCOMPARE(actionTexts(menu.programsMenu()),
            QStringList({ QStringLiteral("Accessories"), QStringLiteral("Graphics"),
                QStringLiteral("Odd") }));
        QMenu *accessories = menu.programsMenu()->actions().first()->menu();
        QVERIFY(accessories);
        QCOMPARE(actionTexts(accessories),
            QStringList({ QStringLiteral("Calc"), QStringLiteral("Notepad") }));

        // Every folder has the one program-group icon.
        const QString groupIcon = QIcon::fromTheme(QStringLiteral("applications-other")).name();
        for (QAction *action : menu.programsMenu()->actions()) {
            if (action->menu())
                QCOMPARE(action->icon().name(), groupIcon);
        }

        // The INI file's flat list: every program, no folders.
        settings.setFlatProgramsMenu(true);
        QCOMPARE(actionTexts(menu.programsMenu()),
            QStringList({ QStringLiteral("Calc"), QStringLiteral("Notepad"), QStringLiteral("Odd"),
                QStringLiteral("Paint") }));
        settings.setFlatProgramsMenu(false);
        QCOMPARE(actionTexts(menu.programsMenu()).size(), 3);
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

    void settingsMenuOpensDialogs()
    {
        StartMenu menu(nullptr, nullptr);
        QMenu *settingsMenu = menu.items()[2].submenu;
        QStringList enabled;
        for (QAction *action : settingsMenu->actions()) {
            if (action->isEnabled())
                enabled << action->text();
        }
        QCOMPARE(enabled,
            QStringList({ QStringLiteral("&Control Panel"), QStringLiteral("&Taskbar..."),
                QStringLiteral("&Display..."), QStringLiteral("&Keyboard..."),
                QStringLiteral("&Mouse...") }));
        QSignalSpy controlPanel(&menu, &StartMenu::controlPanelRequested);
        settingsMenu->actions().first()->trigger();
        QCOMPARE(controlPanel.count(), 1);
        QSignalSpy mouse(&menu, &StartMenu::mousePropertiesRequested);
        settingsMenu->actions().last()->trigger();
        QCOMPARE(mouse.count(), 1);
        QSignalSpy keyboard(&menu, &StartMenu::keyboardPropertiesRequested);
        settingsMenu->actions().at(3)->trigger();
        QCOMPARE(keyboard.count(), 1);
    }

    void commandItemsFollowSettings()
    {
        // No file manager on the PATH: Find needs a configured command.
        const QByteArray path = qgetenv("PATH");
        QTemporaryDir bin;
        qputenv("PATH", bin.path().toLocal8Bit());
        Settings settings(m_dir.filePath(QStringLiteral("daytona.ini")));
        {
            StartMenu menu(nullptr, &settings);
            QVERIFY(!menu.findAction()->isEnabled());
            QCOMPARE(StartMenu::findCommand(&settings), QString());
        }
        settings.setFindCommand(QStringLiteral("catfish --path ~"));
        {
            StartMenu menu(nullptr, &settings);
            QVERIFY(menu.findAction()->isEnabled());
        }
        qputenv("PATH", path);
    }

    void findDefaultsToFileManager()
    {
        const QByteArray path = qgetenv("PATH");
        QTemporaryDir bin;
        QFile program(bin.filePath(QStringLiteral("pcmanfm-qt")));
        QVERIFY(program.open(QIODevice::WriteOnly));
        program.write("#!/bin/sh\n");
        program.close();
        program.setPermissions(program.permissions() | QFileDevice::ExeUser);
        qputenv("PATH", bin.path().toLocal8Bit());

        Settings settings(m_dir.filePath(QStringLiteral("find.ini")));
        const QString expected
            = QStringLiteral("pcmanfm-qt --find-files \"%1\"").arg(QDir::homePath());
        QCOMPARE(StartMenu::findCommand(&settings), expected);
        QCOMPARE(StartMenu::findCommand(nullptr), expected);
        {
            StartMenu menu(nullptr, &settings);
            QVERIFY(menu.findAction()->isEnabled());
        }
        // A configured command wins.
        settings.setFindCommand(QStringLiteral("catfish"));
        QCOMPARE(StartMenu::findCommand(&settings), QStringLiteral("catfish"));
        qputenv("PATH", path);
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
