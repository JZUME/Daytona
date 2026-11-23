// SPDX-License-Identifier: GPL-3.0-or-later

#include "colourscheme.h"
#include "gtktheme.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeAll(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

} // namespace

class TestGtkTheme : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cssImportsChicagoAndRedefinesColours()
    {
        const ColourScheme scheme = *builtInScheme(QStringLiteral("High Contrast Black"));
        const QByteArray css
            = gtkCss(scheme.colours, QStringLiteral("/usr/share/themes/Chicago 95/gtk.css"));
        QVERIFY(css.contains("@import url(\"file:///usr/share/themes/Chicago%2095/gtk.css\");"));
        QVERIFY(css.contains("@define-color bg_color #000000;"));
        QVERIFY(css.contains("@define-color fg_color #ffffff;"));
        QVERIFY(css.contains("@define-color selected_bg_color #800080;"));
        QVERIFY(css.contains("@define-color text_color_light #00ff00;"));
        QVERIFY(css.contains("@define-color window_title_bg_color #800080;"));
        QVERIFY(css.contains("@define-color inactive_title_bg_color #008000;"));
        // The import comes before the definitions, which must override it.
        QVERIFY(css.indexOf("@import") < css.indexOf("@define-color"));
    }

    void writeAndRemove()
    {
        QTemporaryDir data;
        const QString dir = data.filePath(QStringLiteral("themes/Daytona/gtk-3.0"));
        QString error;
        QVERIFY(!writeGtkTheme(dir, standardColours(), QString(), &error)); // no Chicago95
        QVERIFY2(writeGtkTheme(dir, standardColours(), QStringLiteral("/c.css"), &error),
            qPrintable(error));
        QVERIFY(readAll(dir + QStringLiteral("/gtk.css")).contains("file:///c.css"));
        QVERIFY(removeGtkTheme(dir, &error));
        QVERIFY(!QDir(data.filePath(QStringLiteral("themes/Daytona"))).exists());

        // A theme the user made is never touched.
        writeAll(dir + QStringLiteral("/gtk.css"), "/* mine */");
        QVERIFY(!writeGtkTheme(dir, standardColours(), QStringLiteral("/c.css"), &error));
        QVERIFY(removeGtkTheme(dir, &error));
        QCOMPARE(readAll(dir + QStringLiteral("/gtk.css")), QByteArray("/* mine */"));
    }

    void settingsCreated()
    {
        QTemporaryDir config;
        const QString path = config.filePath(QStringLiteral("gtk-3.0/settings.ini"));
        QVERIFY(setGtkSettings(path, QStringLiteral("Icons"), QStringLiteral("Cursors")));
        QCOMPARE(readAll(path),
            QByteArray("[Settings]\ngtk-icon-theme-name=Icons\ngtk-cursor-theme-name=Cursors\n"));
        QVERIFY(!QFile::exists(path + QStringLiteral(".daytona-backup")));
    }

    void settingsPreservedAndBackedUp()
    {
        QTemporaryDir config;
        const QString path = config.filePath(QStringLiteral("settings.ini"));
        const QByteArray original = "# my settings\n[Settings]\ngtk-font-name=Sans 9\n"
                                    "gtk-icon-theme-name = Papirus\n\n[Other]\nx=1\n";
        writeAll(path, original);
        QVERIFY(setGtkSettings(path, QStringLiteral("Chicago95"), QStringLiteral("C95")));
        QCOMPARE(readAll(path),
            QByteArray("# my settings\n[Settings]\ngtk-font-name=Sans 9\n"
                       "gtk-icon-theme-name=Chicago95\ngtk-cursor-theme-name=C95\n\n"
                       "[Other]\nx=1\n"));
        QCOMPARE(readAll(path + QStringLiteral(".daytona-backup")), original);

        // A second apply keeps the first backup, the user's original.
        QVERIFY(setGtkSettings(path, QStringLiteral("Chicago95"), QStringLiteral("C95b")));
        QCOMPARE(readAll(path + QStringLiteral(".daytona-backup")), original);
        QVERIFY(readAll(path).contains("gtk-cursor-theme-name=C95b\n"));
    }

    void settingsGroupAdded()
    {
        QTemporaryDir config;
        const QString path = config.filePath(QStringLiteral("settings.ini"));
        writeAll(path, "[Other]\nx=1\n");
        QVERIFY(setGtkSettings(path, QStringLiteral("I"), QStringLiteral("C")));
        QCOMPARE(readAll(path),
            QByteArray("[Other]\nx=1\n\n[Settings]\ngtk-icon-theme-name=I\n"
                       "gtk-cursor-theme-name=C\n"));
    }
};

QTEST_GUILESS_MAIN(TestGtkTheme)
#include "tst_gtktheme.moc"
