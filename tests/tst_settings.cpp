// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using Daytona::Settings;
using Daytona::TaskbarEdge;
using Daytona::WallpaperMode;

class TestSettings : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString path(const QString &name) const { return m_dir.filePath(name); }

    void writeFile(const QString &file, const QByteArray &contents)
    {
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(contents);
    }

private Q_SLOTS:
    void defaults()
    {
        Settings s(path(QStringLiteral("defaults.ini")));
        QCOMPARE(s.taskbarEdge(), TaskbarEdge::Bottom);
        QCOMPARE(s.taskbarRows(), 1);
        QCOMPARE(s.taskbarLocked(), true);
        QCOMPARE(s.taskbarAlwaysOnTop(), true);
        QCOMPARE(s.taskbarAutoHide(), false);
        QCOMPARE(s.showClock(), true);
        QCOMPARE(s.smallStartMenuIcons(), false);
        QCOMPARE(s.controlPanelCommand(), QString());
        QCOMPARE(s.findCommand(), QString());
        QCOMPARE(s.runHistory(), QStringList());
        QCOMPARE(s.wallpaperPath(), QString());
        QCOMPARE(s.wallpaperMode(), WallpaperMode::Centre);
        QCOMPARE(s.colourScheme(), QStringLiteral("Windows Standard"));
    }

    void readingDoesNotCreateFile()
    {
        const QString file = path(QStringLiteral("untouched.ini"));
        {
            Settings s(file);
            s.taskbarEdge();
            s.runHistory();
            s.colourScheme();
        }
        QVERIFY(!QFileInfo::exists(file));
    }

    void readingDoesNotRewriteFile()
    {
        const QString file = path(QStringLiteral("existing.ini"));
        const QByteArray contents = "[Taskbar]\nEdge = left ; odd spacing kept\nRows=banana\n";
        writeFile(file, contents);
        {
            Settings s(file);
            QCOMPARE(s.taskbarEdge(), TaskbarEdge::Left);
            QCOMPARE(s.taskbarRows(), 1);
        }
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), contents);
    }

    void roundTrip()
    {
        const QString file = path(QStringLiteral("roundtrip.ini"));
        {
            Settings s(file);
            s.setTaskbarEdge(TaskbarEdge::Right);
            s.setTaskbarRows(3);
            s.setTaskbarLocked(false);
            s.setTaskbarAlwaysOnTop(false);
            s.setTaskbarAutoHide(true);
            s.setShowClock(false);
            s.setSmallStartMenuIcons(true);
            s.setControlPanelCommand(QStringLiteral("  control-panel --flag "));
            s.setFindCommand(QStringLiteral("catfish"));
            s.addToRunHistory(QStringLiteral("notepad, with comma"));
            s.setWallpaperPath(QStringLiteral("/tmp/clouds.bmp"));
            s.setWallpaperMode(WallpaperMode::Tile);
            s.setColourScheme(QStringLiteral("High Contrast Black"));
        }
        Settings s(file);
        QCOMPARE(s.taskbarEdge(), TaskbarEdge::Right);
        QCOMPARE(s.taskbarRows(), 3);
        QCOMPARE(s.taskbarLocked(), false);
        QCOMPARE(s.taskbarAlwaysOnTop(), false);
        QCOMPARE(s.taskbarAutoHide(), true);
        QCOMPARE(s.showClock(), false);
        QCOMPARE(s.smallStartMenuIcons(), true);
        QCOMPARE(s.controlPanelCommand(), QStringLiteral("control-panel --flag"));
        QCOMPARE(s.findCommand(), QStringLiteral("catfish"));
        QCOMPARE(s.runHistory(), QStringList { QStringLiteral("notepad, with comma") });
        QCOMPARE(s.wallpaperPath(), QStringLiteral("/tmp/clouds.bmp"));
        QCOMPARE(s.wallpaperMode(), WallpaperMode::Tile);
        QCOMPARE(s.colourScheme(), QStringLiteral("High Contrast Black"));
    }

    void everyEdgeRoundTrips()
    {
        const QString file = path(QStringLiteral("edges.ini"));
        for (TaskbarEdge edge :
            { TaskbarEdge::Top, TaskbarEdge::Left, TaskbarEdge::Right, TaskbarEdge::Bottom }) {
            Settings(file).setTaskbarEdge(edge);
            QCOMPARE(Settings(file).taskbarEdge(), edge);
        }
    }

    void invalidValuesFallBack()
    {
        const QString file = path(QStringLiteral("invalid.ini"));
        writeFile(file,
            "[Taskbar]\nEdge=diagonal\nRows=0\nLocked=maybe\nAlwaysOnTop=yes please\n"
            "AutoHide=2\nShowClock=\nSmallStartMenuIcons=TRUE\n"
            "[Appearance]\nWallpaperMode=stretch\nColourScheme=   \n");
        Settings s(file);
        QCOMPARE(s.taskbarEdge(), TaskbarEdge::Bottom);
        QCOMPARE(s.taskbarRows(), 1);
        QCOMPARE(s.taskbarLocked(), true);
        QCOMPARE(s.taskbarAlwaysOnTop(), true);
        QCOMPARE(s.taskbarAutoHide(), false);
        QCOMPARE(s.showClock(), true);
        QCOMPARE(s.smallStartMenuIcons(), true); // case-insensitive "true" is valid
        QCOMPARE(s.wallpaperMode(), WallpaperMode::Centre);
        QCOMPARE(s.colourScheme(), QStringLiteral("Windows Standard"));
    }

    void negativeRowsAreClamped()
    {
        const QString file = path(QStringLiteral("rows.ini"));
        Settings s(file);
        s.setTaskbarRows(-4);
        QCOMPARE(s.taskbarRows(), 1);
    }

    void runHistoryIsMostRecentFirstAndDeduplicated()
    {
        Settings s(path(QStringLiteral("history.ini")));
        s.addToRunHistory(QStringLiteral("a"));
        s.addToRunHistory(QStringLiteral("b"));
        s.addToRunHistory(QStringLiteral(" a "));
        s.addToRunHistory(QStringLiteral("   "));
        QCOMPARE(s.runHistory(), (QStringList { QStringLiteral("a"), QStringLiteral("b") }));
    }

    void runHistoryIsLimitedTo26()
    {
        Settings s(path(QStringLiteral("long.ini")));
        for (int i = 0; i < 40; ++i)
            s.addToRunHistory(QStringLiteral("cmd%1").arg(i));
        const QStringList history = s.runHistory();
        QCOMPARE(history.size(), Settings::MaxRunHistory);
        QCOMPARE(history.first(), QStringLiteral("cmd39"));
        QCOMPARE(history.last(), QStringLiteral("cmd14"));
    }

    void changedSignal()
    {
        Settings s(path(QStringLiteral("signal.ini")));
        QSignalSpy spy(&s, &Settings::changed);
        s.setTaskbarLocked(false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Taskbar/Locked"));
        s.setTaskbarLocked(false); // unchanged: no signal
        QCOMPARE(spy.count(), 1);
    }

    void defaultPathIsUnderConfigHome()
    {
        qputenv("XDG_CONFIG_HOME", m_dir.path().toLocal8Bit());
        QCOMPARE(Settings::defaultFilePath(), path(QStringLiteral("daytona/daytona.ini")));
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
