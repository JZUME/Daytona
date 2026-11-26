// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using Daytona::PointerScheme;
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

    void everyWallpaperModeRoundTrips()
    {
        const QString file = path(QStringLiteral("modes.ini"));
        for (WallpaperMode mode : { WallpaperMode::Tile, WallpaperMode::Stretch, WallpaperMode::Fit,
                 WallpaperMode::Fill, WallpaperMode::Centre }) {
            Settings(file).setWallpaperMode(mode);
            QCOMPARE(Settings(file).wallpaperMode(), mode);
        }
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
            "[Appearance]\nWallpaperMode=zoom\nColourScheme=   \n");
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

    void pointerScheme()
    {
        const QString file = path(QStringLiteral("pointer.ini"));
        {
            Settings s(file);
            QCOMPARE(s.pointerScheme(), PointerScheme::Standard);
            s.setPointerScheme(PointerScheme::ExtraLarge);
        }
        QCOMPARE(Settings(file).pointerScheme(), PointerScheme::ExtraLarge);
        // daytona-session reads these exact words.
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(f.readAll().contains("[Mouse]\nPointerScheme=extralarge"));
        Settings(file).setPointerScheme(PointerScheme::Large);
        QCOMPARE(Settings(file).pointerScheme(), PointerScheme::Large);

        const QString bad = path(QStringLiteral("badpointer.ini"));
        writeFile(bad, "[Mouse]\nPointerScheme=huge\n");
        QCOMPARE(Settings(bad).pointerScheme(), PointerScheme::Standard);
    }

    void systemStartLogo()
    {
        const QString file = path(QStringLiteral("logo.ini"));
        {
            Settings s(file);
            QCOMPARE(s.systemStartLogo(), true); // on unless turned off
            QSignalSpy spy(&s, &Settings::changed);
            s.setSystemStartLogo(false);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.first().first().toString(), QStringLiteral("Appearance/SystemStartLogo"));
        }
        QCOMPARE(Settings(file).systemStartLogo(), false);
    }

    void screenModes()
    {
        const QString file = path(QStringLiteral("modes.ini"));
        {
            Settings s(file);
            QVERIFY(!s.outputMode(QStringLiteral("eDP-1")).isValid());
            Daytona::ScreenMode mode;
            mode.size = QSize(1920, 1080);
            mode.refresh = 59940;
            s.setOutputMode(QStringLiteral("eDP-1"), mode);
            s.setOutputMode(QStringLiteral("DP-1"), Daytona::ScreenMode()); // ignored
        }
        Settings s(file);
        QCOMPARE(s.outputMode(QStringLiteral("eDP-1")).size, QSize(1920, 1080));
        QCOMPARE(s.outputMode(QStringLiteral("eDP-1")).refresh, 59940);
        QVERIFY(!s.outputMode(QStringLiteral("DP-1")).isValid());
        const QString bad = path(QStringLiteral("badmodes.ini"));
        writeFile(bad, "[ScreenMode]\neDP-1=huge\nDP-1=800x600\n");
        QVERIFY(!Settings(bad).outputMode(QStringLiteral("eDP-1")).isValid());
        QCOMPARE(Settings(bad).outputMode(QStringLiteral("DP-1")).size, QSize(800, 600));
        QCOMPARE(Settings(bad).outputMode(QStringLiteral("DP-1")).refresh, 0);
    }

    void screenScales()
    {
        const QString file = path(QStringLiteral("scales.ini"));
        {
            Settings s(file);
            QCOMPARE(s.outputScale(QStringLiteral("eDP-1")), 0.0);
            s.setOutputScale(QStringLiteral("eDP-1"), 1.25);
            // Monitors with a serial number are filed by make, model and
            // serial, which may contain spaces and slashes.
            s.setOutputScale(QStringLiteral("Dell Inc. DELL U2720Q AB/12\\34"), 2.0);
            s.setOutputScale(QStringLiteral("HDMI-A-1"), 9.0); // clamped
            s.setOutputScale(QString(), 2.0); // ignored
        }
        Settings s(file);
        QCOMPARE(s.outputScale(QStringLiteral("eDP-1")), 1.25);
        QCOMPARE(s.outputScale(QStringLiteral("Dell Inc. DELL U2720Q AB/12\\34")), 2.0);
        QCOMPARE(s.outputScale(QStringLiteral("HDMI-A-1")), Settings::MaxScreenScale);
        QCOMPARE(s.outputScale(QStringLiteral("DP-2")), 0.0);
        QCOMPARE(s.outputScale(QString()), 0.0);

        const QString bad = path(QStringLiteral("badscales.ini"));
        writeFile(bad, "[ScreenScale]\neDP-1=big\nDP-1=0.5\nDP-2=1.5\n");
        Settings b(bad);
        QCOMPARE(b.outputScale(QStringLiteral("eDP-1")), 0.0);
        QCOMPARE(b.outputScale(QStringLiteral("DP-1")), 0.0); // below 100%
        QCOMPARE(b.outputScale(QStringLiteral("DP-2")), 1.5);
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
