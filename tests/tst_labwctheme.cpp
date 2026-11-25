// SPDX-License-Identifier: GPL-3.0-or-later

#include "colourscheme.h"
#include "labwctheme.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

QByteArray themercValue(const QByteArray &themerc, const QByteArray &key)
{
    for (const QByteArray &line : themerc.split('\n')) {
        if (line.startsWith(key + ':'))
            return line.mid(key.size() + 1).trimmed();
    }
    return {};
}

} // namespace

class TestLabwcTheme : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void standardMatchesShippedTheme()
    {
        const QByteArray themerc = labwcThemerc(standardColours());
        QCOMPARE(themercValue(themerc, "window.active.title.bg.color"), QByteArray("#000080"));
        QCOMPARE(themercValue(themerc, "window.inactive.label.text.color"), QByteArray("#c0c0c0"));
        QCOMPARE(themercValue(themerc, "menu.items.active.bg.color"), QByteArray("#000080"));
        // Layout keys are kept from the template.
        QCOMPARE(themercValue(themerc, "border.width"), QByteArray("4"));
        QCOMPARE(themercValue(themerc, "window.button.width"), QByteArray("16"));
    }

    void schemeColoursAreUsed()
    {
        const ColourScheme scheme = *builtInScheme(QStringLiteral("High Contrast Black"));
        const QByteArray themerc = labwcThemerc(scheme.colours);
        const auto value = [&](const char *key) { return themercValue(themerc, key); };
        QCOMPARE(value("window.active.title.bg.color"), QByteArray("#800080"));
        QCOMPARE(value("window.inactive.title.bg.color"), QByteArray("#008000"));
        QCOMPARE(value("window.active.label.text.color"), QByteArray("#ffffff"));
        QCOMPARE(value("window.inactive.label.text.color"), QByteArray("#ffffff"));
        QCOMPARE(value("window.active.border.color"), QByteArray("#ffff00"));
        QCOMPARE(value("window.inactive.border.color"), QByteArray("#008000"));
        QCOMPARE(value("menu.items.bg.color"), QByteArray("#000000"));
        QCOMPARE(value("menu.items.text.color"), QByteArray("#ffffff"));
        QCOMPARE(value("menu.items.active.bg.color"), QByteArray("#800080"));
        QCOMPARE(value("osd.bg.color"), QByteArray("#000000"));
        // The raised frame takes the scheme's 3D colours.
        QCOMPARE(value("window.border.bevel"), QByteArray("yes"));
        QCOMPARE(value("window.border.bevel.light.color"), QByteArray("#c0c0c0"));
        QCOMPARE(value("window.border.bevel.highlight.color"), QByteArray("#ffffff"));
        QCOMPARE(value("window.border.bevel.shadow.color"), QByteArray("#808080"));
        QCOMPARE(value("window.border.bevel.darkshadow.color"), QByteArray("#808080"));
        QCOMPARE(value("osd.window-switcher.preview.border.color"),
            QByteArray("#808080,#ffffff,#808080"));
    }

    void buttonsAreRecoloured()
    {
        SchemeColours colours = standardColours();
        colours.insert(QStringLiteral("ButtonFace"), QColor(1, 1, 1));
        colours.insert(QStringLiteral("ButtonText"), QColor(2, 2, 2));
        colours.insert(QStringLiteral("ButtonDkShadow"), QColor(3, 3, 3));
        colours.insert(QStringLiteral("ButtonLight"), QColor(4, 4, 4));
        const QString svg = QString::fromUtf8(labwcButtonSvg(QStringLiteral("close"), colours));
        QVERIFY(svg.contains(QLatin1String("<rect x=\"2\" y=\"0\" width=\"14\" height=\"14\" "
                                           "fill=\"#010101\"/>")));
        // Outer shadow line, then a glyph pixel.
        QVERIFY(svg.contains(QLatin1String("width=\"14\" height=\"1\" fill=\"#030303\"")));
        QVERIFY(svg.contains(
            QLatin1String("<rect x=\"5\" y=\"3\" width=\"1\" height=\"1\" fill=\"#020202\"/>")));
        QVERIFY(!svg.contains(QLatin1String("#000000")));
        QVERIFY(!svg.contains(QLatin1String("#c0c0c0")));
        for (const char *button : { "iconify", "max", "max_toggled" })
            QVERIFY(labwcButtonSvg(QLatin1String(button), colours).startsWith("<?xml"));
    }

    void writeAndRemove()
    {
        QTemporaryDir data;
        const QString dir = data.filePath(QStringLiteral("themes/Daytona/openbox-3"));
        const ColourScheme scheme = *builtInScheme(QStringLiteral("High Contrast White"));
        QString error;
        QVERIFY2(writeLabwcTheme(dir, scheme.colours, &error), qPrintable(error));
        const QStringList files = QDir(dir).entryList(QDir::Files);
        QCOMPARE(files.size(), 17); // themerc and 16 button images
        QVERIFY(files.contains(QStringLiteral("max_toggled_hover-inactive.svg")));
        QVERIFY(QFile::exists(dir + QStringLiteral("/.daytona-generated")));
        // Writing again replaces the generated theme.
        QVERIFY2(writeLabwcTheme(dir, standardColours(), &error), qPrintable(error));

        QVERIFY2(removeLabwcTheme(dir, &error), qPrintable(error));
        QVERIFY(!QDir(data.filePath(QStringLiteral("themes/Daytona"))).exists());
        // Removing when nothing was generated is fine.
        QVERIFY(removeLabwcTheme(dir, &error));
    }

    void userThemesAreLeftAlone()
    {
        QTemporaryDir data;
        const QString dir = data.filePath(QStringLiteral("openbox-3"));
        QVERIFY(QDir().mkpath(dir));
        QFile own(dir + QStringLiteral("/themerc"));
        QVERIFY(own.open(QIODevice::WriteOnly));
        own.write("# the user's own\n");
        own.close();
        QString error;
        QVERIFY(!writeLabwcTheme(dir, standardColours(), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(removeLabwcTheme(dir, &error));
        QVERIFY(QFile::exists(dir + QStringLiteral("/themerc")));
    }

    void userThemeDirFollowsXdg()
    {
        qputenv("XDG_DATA_HOME", "/tmp/xdg-data");
        QCOMPARE(labwcUserThemeDir(), QStringLiteral("/tmp/xdg-data/themes/Daytona/openbox-3"));
    }
};

QTEST_GUILESS_MAIN(TestLabwcTheme)
#include "tst_labwctheme.moc"
