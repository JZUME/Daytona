// SPDX-License-Identifier: GPL-3.0-or-later

#include "colourscheme.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

class TestThemeFile : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString write(const QString &name, const QByteArray &contents)
    {
        const QString path = m_dir.filePath(name);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("cannot write fixture");
        f.write(contents);
        return path;
    }

    ColourScheme parse(const QByteArray &contents)
    {
        return parseTheme(contents, QStringLiteral("Test"), m_dir.path());
    }

private Q_SLOTS:
    void builtInSchemes()
    {
        QCOMPARE(builtInSchemeNames().first(), QStringLiteral("Windows Standard"));
        for (const QString &name : builtInSchemeNames()) {
            const std::optional<ColourScheme> s = builtInScheme(name);
            QVERIFY(s);
            QCOMPARE(s->name, name);
            for (const QString &key : colourKeys())
                QVERIFY2(s->colour(key).isValid(), qPrintable(name + QLatin1Char(' ') + key));
        }
        QCOMPARE(
            builtInScheme(QStringLiteral("Windows Standard"))->colour(QStringLiteral("Background")),
            QColor(0, 128, 128));
        QCOMPARE(
            builtInScheme(QStringLiteral("high contrast black"))->colour(QStringLiteral("Window")),
            QColor(Qt::black));
        QVERIFY(!builtInScheme(QStringLiteral("Nonexistent")));
    }

    void everyColourKeyIsRead()
    {
        QByteArray file = "[Control Panel\\Colors]\n";
        int n = 1;
        for (const QString &key : colourKeys()) {
            file += key.toLatin1() + "=" + QByteArray::number(n) + " " + QByteArray::number(n + 1)
                + " " + QByteArray::number(n + 2) + "\r\n";
            n += 3;
        }
        const ColourScheme s = parse(file);
        n = 1;
        for (const QString &key : colourKeys()) {
            QCOMPARE(s.colour(key), QColor(n, n + 1, n + 2));
            n += 3;
        }
        QVERIFY(s.warnings.isEmpty());
    }

    void missingColoursFallBackToStandard()
    {
        const ColourScheme s = parse("[control panel\\colors]\nactivetitle=1 2 3\n");
        QCOMPARE(s.colour(QStringLiteral("ActiveTitle")), QColor(1, 2, 3)); // case-insensitive
        QCOMPARE(s.colour(QStringLiteral("ButtonFace")), QColor(192, 192, 192));
        QCOMPARE(s.colours.size(), colourKeys().size());
    }

    void wallpaperStyle_data()
    {
        QTest::addColumn<QByteArray>("desktop");
        QTest::addColumn<QString>("mode");
        QTest::newRow("none") << QByteArray() << QStringLiteral("centre");
        QTest::newRow("centre") << QByteArray("WallpaperStyle=0\n") << QStringLiteral("centre");
        QTest::newRow("stretch") << QByteArray("WallpaperStyle=2\n") << QStringLiteral("stretch");
        QTest::newRow("fit") << QByteArray("WallpaperStyle=6\n") << QStringLiteral("fit");
        QTest::newRow("fill") << QByteArray("WallpaperStyle=10\n") << QStringLiteral("fill");
        QTest::newRow("unknown") << QByteArray("WallpaperStyle=22\n") << QStringLiteral("centre");
        // TileWallpaper wins, as in Windows.
        QTest::newRow("tile") << QByteArray("WallpaperStyle=2\nTileWallpaper=1\n")
                              << QStringLiteral("tile");
    }

    void wallpaperStyle()
    {
        QFETCH(QByteArray, desktop);
        QFETCH(QString, mode);
        QCOMPARE(parse("[Control Panel\\Desktop]\n" + desktop).wallpaperMode, mode);
    }

    void invalidColoursWarn()
    {
        const ColourScheme s = parse("[Control Panel\\Colors]\nMenu=300 0 0\nWindow=1 2\n"
                                     "WindowText=a b c\nnot a line\n=nokey\n");
        QCOMPARE(s.colour(QStringLiteral("Menu")), QColor(192, 192, 192));
        QCOMPARE(s.colour(QStringLiteral("Window")), QColor(255, 255, 255));
        QCOMPARE(s.warnings.size(), 3);
    }

    void sampleFixture()
    {
        const std::optional<ColourScheme> s
            = readThemeFile(QFINDTESTDATA("data/themes/Sample.theme"));
        QVERIFY(s);
        QCOMPARE(s->name, QStringLiteral("Sample"));
        QCOMPARE(s->colour(QStringLiteral("ActiveTitle")), QColor(128, 0, 0));
        QCOMPARE(s->colour(QStringLiteral("Background")), QColor(0, 64, 64)); // quoted value
        QVERIFY(s->wallpaper.endsWith(QStringLiteral("Sample Wallpaper.bmp"))); // case-insensitive
        QCOMPARE(s->wallpaperMode, QStringLiteral("tile"));
        QVERIFY(s->warnings.join(QLatin1Char(' ')).contains(QStringLiteral("Sound")));
    }

    void windowsPathsResolveOnlyInsideThemeDir_data()
    {
        QTest::addColumn<QByteArray>("wallpaper");
        QTest::addColumn<bool>("found");
        QTest::newRow("WinDir style") << QByteArray("%WinDir%MEDIA\\inside.bmp") << true;
        QTest::newRow("absolute drive path") << QByteArray("C:\\Plus!\\Themes\\INSIDE.BMP") << true;
        QTest::newRow("parent traversal") << QByteArray("..\\outside.bmp") << false;
        QTest::newRow("unix absolute") << QByteArray("/etc/passwd") << false;
        QTest::newRow("dot dot") << QByteArray("..") << false;
        QTest::newRow("missing") << QByteArray("%ThemeDir%nothere.bmp") << false;
    }

    void windowsPathsResolveOnlyInsideThemeDir()
    {
        QFETCH(QByteArray, wallpaper);
        QFETCH(bool, found);
        write(QStringLiteral("themes/inside.bmp"), "BM");
        write(QStringLiteral("outside.bmp"), "BM"); // next to, not inside, the theme dir
        const ColourScheme s = parseTheme("[Control Panel\\Desktop]\nWallpaper=" + wallpaper + "\n",
            QStringLiteral("T"), m_dir.filePath(QStringLiteral("themes")));
        QCOMPARE(!s.wallpaper.isEmpty(), found);
        if (found)
            QVERIFY(s.wallpaper.startsWith(
                QDir(m_dir.filePath(QStringLiteral("themes"))).canonicalPath()));
        else
            QVERIFY(!s.warnings.isEmpty());
    }

    void unsupportedPartsAreReported()
    {
        const ColourScheme s
            = parse("[Control Panel\\Cursors]\nArrow=%ThemeDir%a.cur\n"
                    "[boot]\nSCRNSAVE.EXE=%ThemeDir%x.scr\n"
                    "[Software\\Classes\\CLSID\\{x}\\DefaultIcon]\nDefaultValue=x.ico\n"
                    "[Control Panel\\Desktop\\WindowMetrics]\nCaptionFont=x\n");
        const QString all = s.warnings.join(QLatin1Char('\n'));
        QVERIFY(all.contains(QStringLiteral("cursors")));
        QVERIFY(all.contains(QStringLiteral("Screen savers")));
        QVERIFY(all.contains(QStringLiteral("icons")));
        QVERIFY(all.contains(QStringLiteral("Fonts")));
    }

    void encodings()
    {
        // Windows-1252 (an e-acute in a comment) must not stop the colours.
        const ColourScheme ansi = parse("; caf\xe9\n[Control Panel\\Colors]\nMenu=1 2 3\n");
        QCOMPARE(ansi.colour(QStringLiteral("Menu")), QColor(1, 2, 3));
        // UTF-16 LE with a byte order mark.
        const QString text = QStringLiteral("[Control Panel\\Colors]\r\nMenu=4 5 6\r\n");
        QByteArray utf16("\xff\xfe", 2);
        utf16.append(reinterpret_cast<const char *>(text.utf16()), text.size() * 2);
        QCOMPARE(parse(utf16).colour(QStringLiteral("Menu")), QColor(4, 5, 6));
    }

    void randomBytesNeverCrash()
    {
        QRandomGenerator rng(1996);
        for (int i = 0; i < 300; ++i) {
            QByteArray junk(int(rng.bounded(4096)), Qt::Uninitialized);
            for (char &c : junk)
                c = char(rng.bounded(256));
            if (i % 3 == 0)
                junk.prepend("[Control Panel\\Colors]\nMenu=");
            const ColourScheme s = parse(junk);
            QCOMPARE(s.colours.size(), colourKeys().size());
        }
    }

    void unreadableFile()
    {
        QVERIFY(!readThemeFile(m_dir.filePath(QStringLiteral("missing.theme"))));
    }
};

QTEST_GUILESS_MAIN(TestThemeFile)
#include "tst_themefile.moc"
