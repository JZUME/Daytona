// SPDX-License-Identifier: GPL-3.0-or-later

#include "colours.h"
#include "daytonastyle.h"

#include <QApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

class TestColours : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void standardHasEveryKey()
    {
        QCOMPARE(colourKeys().size(), 29);
        for (const QString &key : colourKeys())
            QVERIFY2(standardColours().value(key).isValid(), qPrintable(key));
        QCOMPARE(standardColours().value(QStringLiteral("ButtonFace")), QColor(192, 192, 192));
    }

    void parsesRgb()
    {
        QCOMPARE(parseRgb(QStringLiteral(" 1  2 3 ")), QColor(1, 2, 3));
        QVERIFY(!parseRgb(QStringLiteral("1 2")));
        QVERIFY(!parseRgb(QStringLiteral("1 2 256")));
        QVERIFY(!parseRgb(QStringLiteral("a b c")));
        QVERIFY(!parseRgb(QStringLiteral("-1 0 0")));
    }

    void mapsKeysToRoles_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<int>("group");
        QTest::addColumn<int>("role");
        const auto row = [](const char *key, QPalette::ColorGroup group, QPalette::ColorRole role) {
            QTest::addRow("%s -> %d/%d", key, int(group), int(role))
                << QString::fromLatin1(key) << int(group) << int(role);
        };
        row("ButtonFace", QPalette::Active, QPalette::Button);
        row("ButtonFace", QPalette::Active, QPalette::Window);
        row("ButtonFace", QPalette::Active, QPalette::AlternateBase);
        row("ButtonFace", QPalette::Disabled, QPalette::Base);
        row("ButtonText", QPalette::Active, QPalette::ButtonText);
        row("ButtonText", QPalette::Active, QPalette::WindowText);
        row("ButtonHilight", QPalette::Active, QPalette::Light);
        row("ButtonHilight", QPalette::Active, QPalette::BrightText);
        row("ButtonLight", QPalette::Active, QPalette::Midlight);
        row("ButtonShadow", QPalette::Active, QPalette::Mid);
        row("ButtonShadow", QPalette::Active, QPalette::Dark);
        row("ButtonShadow", QPalette::Disabled, QPalette::Highlight);
        row("ButtonDkShadow", QPalette::Active, QPalette::Shadow);
        row("Window", QPalette::Active, QPalette::Base);
        row("WindowText", QPalette::Active, QPalette::Text);
        row("Hilight", QPalette::Active, QPalette::Highlight);
        row("HilightText", QPalette::Active, QPalette::HighlightedText);
        row("InfoWindow", QPalette::Active, QPalette::ToolTipBase);
        row("InfoText", QPalette::Active, QPalette::ToolTipText);
        row("HotTrackingColor", QPalette::Active, QPalette::Link);
        row("GrayText", QPalette::Active, QPalette::PlaceholderText);
        row("GrayText", QPalette::Disabled, QPalette::WindowText);
        row("GrayText", QPalette::Disabled, QPalette::ButtonText);
        row("GrayText", QPalette::Disabled, QPalette::Text);
        row("GrayText", QPalette::Disabled, QPalette::HighlightedText);
        // Inactive windows use the same colours as active ones.
        row("Hilight", QPalette::Inactive, QPalette::Highlight);
    }

    void mapsKeysToRoles()
    {
        QFETCH(QString, key);
        QFETCH(int, group);
        QFETCH(int, role);
        // Give every key its own colour so a wrong mapping cannot pass.
        SchemeColours colours;
        int i = 1;
        for (const QString &k : colourKeys()) {
            colours.insert(k, QColor(i, 255 - i, i * 3));
            ++i;
        }
        const QPalette palette = paletteFromColours(colours);
        QCOMPARE(palette.color(QPalette::ColorGroup(group), QPalette::ColorRole(role)),
            colours.value(key));
    }

    void missingKeysAreStandard()
    {
        const QPalette palette
            = paletteFromColours({ { QStringLiteral("Hilight"), QColor(1, 2, 3) } });
        QCOMPARE(palette.color(QPalette::Highlight), QColor(1, 2, 3));
        QCOMPARE(palette.color(QPalette::Button), QColor(192, 192, 192));
        QCOMPARE(paletteFromColours({}), Style::ntPalette());
    }

    void fileRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/colors.ini"));
        SchemeColours colours = standardColours();
        colours.insert(QStringLiteral("ActiveTitle"), QColor(128, 0, 0));
        const QString name = QStringLiteral("Brick \"red\", 1\\2; [x]");
        QString error;
        QVERIFY2(writeColourFile(path, { name, colours }, &error), qPrintable(error));
        const std::optional<ColourFile> read = readColourFile(path);
        QVERIFY(read);
        QCOMPARE(read->name, name);
        QCOMPARE(read->colours, colours);
    }

    void badFileValuesFallBack()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("colors.ini"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Colors]\nHilight=1 2 3\nButtonFace=300 0 0\nWindow=1, 2, 3\nJunk=x\n");
        file.close();
        const std::optional<ColourFile> read = readColourFile(path);
        QVERIFY(read);
        QCOMPARE(read->colours.value(QStringLiteral("Hilight")), QColor(1, 2, 3));
        QCOMPARE(read->colours.value(QStringLiteral("ButtonFace")), QColor(192, 192, 192));
        QCOMPARE(read->colours.value(QStringLiteral("Window")), QColor(255, 255, 255));
        QCOMPARE(read->colours.size(), colourKeys().size());
        QVERIFY(!readColourFile(dir.filePath(QStringLiteral("missing.ini"))));
    }

    void oversizedFileIsRefused()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("colors.ini"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[Colors]\nHilight=1 2 3\n");
        file.write(QByteArray(100 * 1024, ';'));
        file.close();
        QVERIFY(!readColourFile(path));
    }

    void styleFollowsTheFile()
    {
        QTemporaryDir dir;
        // Two levels missing, as on a first run with no ~/.config.
        const QString path = dir.filePath(QStringLiteral("config/daytona/colors.ini"));
        auto *style = new Style;
        style->setColourFile(path);
        QApplication::setStyle(style); // the application owns it from here
        // No file yet: Windows Standard.
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), QColor(0, 0, 128));

        // The directory and file appear later, as with a first apply.
        QSignalSpy changed(style, &Style::coloursChanged);
        SchemeColours colours = standardColours();
        colours.insert(QStringLiteral("Hilight"), QColor(128, 0, 128));
        colours.insert(QStringLiteral("Background"), QColor(1, 2, 3));
        QVERIFY(writeColourFile(path, { QStringLiteral("One"), colours }));
        QVERIFY(changed.wait(3000));
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), QColor(128, 0, 128));
        QCOMPARE(Style::schemeColour(QStringLiteral("Background")), QColor(1, 2, 3));

        // Replaced again (a rename, so the old watch is gone).
        colours.insert(QStringLiteral("Hilight"), QColor(0, 128, 0));
        QVERIFY(writeColourFile(path, { QStringLiteral("Two"), colours }));
        QVERIFY(changed.wait(3000));
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), QColor(0, 128, 0));

        // Removed: back to Windows Standard.
        QVERIFY(QFile::remove(path));
        QVERIFY(changed.wait(3000));
        QCOMPARE(QApplication::palette().color(QPalette::Highlight), QColor(0, 0, 128));
    }
};

QTEST_MAIN(TestColours)
#include "tst_colours.moc"
