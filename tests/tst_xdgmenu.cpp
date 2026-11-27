// SPDX-License-Identifier: GPL-3.0-or-later

#include "xdgmenu.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

const QString Desktop = QStringLiteral("Daytona");

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

QByteArray app(const char *name, const char *categories, const char *extra = "")
{
    return QByteArray("[Desktop Entry]\nType=Application\nExec=true\nName=") + name
        + "\nCategories=" + categories + "\n" + extra;
}

QByteArray menu(const QByteArray &body)
{
    return "<!DOCTYPE Menu PUBLIC \"-//freedesktop//DTD Menu 1.0//EN\"\n"
           " \"http://www.freedesktop.org/standards/menu-spec/1.0/menu.dtd\">\n<Menu>"
        + body + "</Menu>\n";
}

// The tree as text: folders as "Name[...]", entries by name, separators as
// "-".
QString outline(const MenuNode &node)
{
    QStringList parts;
    for (const MenuNode &child : node.children) {
        switch (child.kind) {
        case MenuNode::Kind::Folder:
            parts << child.name + QLatin1Char('[') + outline(child) + QLatin1Char(']');
            break;
        case MenuNode::Kind::Entry:
            parts << child.entry.name;
            break;
        case MenuNode::Kind::Separator:
            parts << QStringLiteral("-");
            break;
        }
    }
    return parts.join(QLatin1Char(','));
}

} // namespace

class TestXdgMenu : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;
    XdgMenuPaths m_paths;
    QString m_userConfig;
    QString m_systemConfig;
    QString m_userData;
    QString m_systemData;

    QString load(const QString &file = {})
    {
        const QString path
            = file.isEmpty() ? findApplicationsMenu(m_paths, QStringLiteral("daytona-")) : file;
        return outline(loadXdgMenu(path, m_paths, Desktop, QStringLiteral("C")).root);
    }

    void systemApp(const QString &id, const QByteArray &contents)
    {
        writeFile(m_systemData + QStringLiteral("/applications/") + id, contents);
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        QDir(m_tmp.path()).removeRecursively();
        QDir().mkpath(m_tmp.path());
        m_userConfig = m_tmp.filePath(QStringLiteral("home/config"));
        m_systemConfig = m_tmp.filePath(QStringLiteral("etc/xdg"));
        m_userData = m_tmp.filePath(QStringLiteral("home/share"));
        m_systemData = m_tmp.filePath(QStringLiteral("usr/share"));
        for (const QString &dir : { m_userConfig, m_systemConfig, m_userData, m_systemData })
            QDir().mkpath(dir);
        m_paths.configDirs = { m_userConfig, m_systemConfig };
        m_paths.dataDirs = { m_userData, m_systemData };

        systemApp(QStringLiteral("notepad.desktop"), app("Notepad", "Utility;TextEditor;"));
        systemApp(QStringLiteral("calc.desktop"), app("Calculator", "Utility;"));
        systemApp(QStringLiteral("paint.desktop"), app("Paint", "Graphics;"));
        systemApp(QStringLiteral("odd.desktop"), app("Odd", "GTK;"));
        systemApp(QStringLiteral("mixer.desktop"), app("Mixer", "Settings;System;"));
        systemApp(QStringLiteral("hidden.desktop"), app("Hidden", "Utility;", "NoDisplay=true\n"));
        systemApp(QStringLiteral("kde-only.desktop"), app("KDE", "Utility;", "OnlyShowIn=KDE;\n"));
    }

    void builtInMenuKeepsTheFolders()
    {
        // Nothing installed: the built-in Daytona menu, with today's folders.
        QCOMPARE(findApplicationsMenu(m_paths, QStringLiteral("daytona-")),
            QString::fromLatin1(BuiltInMenuFile));
        QCOMPARE(load(),
            QStringLiteral("Accessories[Calculator,Notepad],Graphics[Paint],Settings[Mixer],Odd"));
    }

    void findsPrefixedMenuFirst()
    {
        const QString plain = m_systemConfig + QStringLiteral("/menus/applications.menu");
        writeFile(plain, menu("<Name>A</Name>"));
        QCOMPARE(findApplicationsMenu(m_paths, QStringLiteral("daytona-")), plain);
        const QString prefixed
            = m_systemConfig + QStringLiteral("/menus/daytona-applications.menu");
        writeFile(prefixed, menu("<Name>A</Name>"));
        QCOMPARE(findApplicationsMenu(m_paths, QStringLiteral("daytona-")), prefixed);
        QCOMPARE(findApplicationsMenu(m_paths, QString()), plain);
        const QString user = m_userConfig + QStringLiteral("/menus/daytona-applications.menu");
        writeFile(user, menu("<Name>A</Name>"));
        QCOMPARE(findApplicationsMenu(m_paths, QStringLiteral("daytona-")), user);
    }

    void directoryFilesNameAndHideFolders()
    {
        writeFile(
            m_systemData + QStringLiteral("/desktop-directories/daytona-accessories.directory"),
            "[Desktop Entry]\nType=Directory\nName=Accessories\nName[de]=Andere Programme\n");
        writeFile(m_systemData + QStringLiteral("/desktop-directories/daytona-graphics.directory"),
            "[Desktop Entry]\nType=Directory\nName=Graphics\nNoDisplay=true\n");
        const XdgMenu tree = loadXdgMenu(
            QString::fromLatin1(BuiltInMenuFile), m_paths, Desktop, QStringLiteral("de_DE.UTF-8"));
        QCOMPARE(outline(tree.root),
            QStringLiteral("Andere Programme[Calculator,Notepad],Settings[Mixer],Odd"));
        QVERIFY(tree.watchFiles.contains(
            m_systemData + QStringLiteral("/desktop-directories/daytona-accessories.directory")));
        QVERIFY(tree.watchDirs.contains(m_systemData + QStringLiteral("/applications")));
    }

    void menuEditorChangesApply()
    {
        // What menulibre and alacarte write: the user's menu merges the
        // system one and adds to it.
        writeFile(m_userConfig + QStringLiteral("/menus/daytona-applications.menu"),
            menu("<Name>Applications</Name><MergeFile type=\"parent\">/etc/xdg/menus/"
                 "daytona-applications.menu</MergeFile>"
                 "<Menu><Name>Accessories</Name><Exclude><Filename>notepad.desktop</Filename>"
                 "</Exclude></Menu>"
                 "<Menu><Name>Settings</Name><Deleted/></Menu>"
                 "<Menu><Name>Writing</Name><Include><Filename>notepad.desktop</Filename>"
                 "</Include></Menu>"));
        QCOMPARE(
            load(), QStringLiteral("Accessories[Calculator],Graphics[Paint],Writing[Notepad],Odd"));

        // An entry the user hid with a copy of its .desktop file.
        writeFile(m_userData + QStringLiteral("/applications/paint.desktop"),
            app("Paint", "Graphics;", "Hidden=true\n"));
        QCOMPARE(load(), QStringLiteral("Accessories[Calculator],Writing[Notepad],Odd"));
    }

    void mergeDirsAddFolders()
    {
        writeFile(m_systemConfig + QStringLiteral("/menus/applications-merged/extra.menu"),
            menu("<Name>Applications</Name><Menu><Name>Drawing</Name>"
                 "<Include><Category>Graphics</Category></Include></Menu>"));
        QCOMPARE(load(),
            QStringLiteral("Accessories[Calculator,Notepad],Drawing[Paint],Graphics[Paint],"
                           "Settings[Mixer],Odd"));
    }

    void rules()
    {
        const QString file = m_systemConfig + QStringLiteral("/menus/test.menu");
        writeFile(file,
            menu("<Name>Root</Name><DefaultAppDirs/>"
                 "<Menu><Name>A</Name><Include><And><Category>Utility</Category>"
                 "<Not><Category>TextEditor</Category></Not></And></Include></Menu>"
                 "<Menu><Name>B</Name><Include><Or><Filename>odd.desktop</Filename>"
                 "<Category>Graphics</Category></Or></Include>"
                 "<Exclude><Filename>paint.desktop</Filename></Exclude></Menu>"
                 "<Menu><Name>C</Name><Include><All/></Include>"
                 "<Exclude><Category>Utility</Category><Category>Settings</Category></Exclude>"
                 "</Menu>"));
        QCOMPARE(load(file), QStringLiteral("A[Calculator],B[Odd],C[Odd,Paint]"));
    }

    void onlyUnallocatedTakesTheRest()
    {
        const QString file = m_systemConfig + QStringLiteral("/menus/test.menu");
        writeFile(file,
            menu("<Name>Root</Name><DefaultAppDirs/>"
                 "<Menu><Name>Other</Name><OnlyUnallocated/><Include><All/></Include></Menu>"
                 "<Menu><Name>Tools</Name><Include><Category>Utility</Category></Include></Menu>"));
        QCOMPARE(load(file), QStringLiteral("Other[Mixer,Odd,Paint],Tools[Calculator,Notepad]"));
    }

    void duplicateMenusMergeAndMovesApply()
    {
        const QString file = m_systemConfig + QStringLiteral("/menus/test.menu");
        writeFile(file,
            menu("<Name>Root</Name><DefaultAppDirs/>"
                 "<Menu><Name>A</Name><Include><Filename>calc.desktop</Filename></Include></Menu>"
                 "<Menu><Name>A</Name><Include><Filename>paint.desktop</Filename></Include></Menu>"
                 "<Menu><Name>B</Name><Include><Filename>odd.desktop</Filename></Include></Menu>"
                 "<Move><Old>B</Old><New>A/Inner</New></Move>"));
        QCOMPARE(load(file), QStringLiteral("A[Inner[Odd],Calculator,Paint]"));
    }

    void layoutOrdersItems()
    {
        const QString file = m_systemConfig + QStringLiteral("/menus/test.menu");
        writeFile(file,
            menu("<Name>Root</Name><DefaultAppDirs/>"
                 "<Include><Category>Utility</Category><Category>Graphics</Category></Include>"
                 "<Menu><Name>Z</Name><Include><Filename>odd.desktop</Filename></Include></Menu>"
                 "<Menu><Name>Y</Name><Include><Filename>mixer.desktop</Filename></Include></Menu>"
                 "<Layout><Filename>paint.desktop</Filename><Separator/><Merge type=\"files\"/>"
                 "<Separator/><Menuname>Z</Menuname><Merge type=\"menus\"/><Separator/></Layout>"));
        QCOMPARE(load(file), QStringLiteral("Paint,-,Calculator,Notepad,-,Z[Odd],Y[Mixer]"));
    }

    void emptyFoldersAreLeftOut()
    {
        const QString file = m_systemConfig + QStringLiteral("/menus/test.menu");
        writeFile(file,
            menu("<Name>Root</Name><DefaultAppDirs/>"
                 "<Menu><Name>Empty</Name><Menu><Name>AlsoEmpty</Name></Menu>"
                 "<Include><Filename>hidden.desktop</Filename><Filename>kde-only.desktop</Filename>"
                 "</Include></Menu>"
                 "<Menu><Name>Full</Name><Include><Filename>calc.desktop</Filename></Include></"
                 "Menu>"));
        QCOMPARE(load(file), QStringLiteral("Full[Calculator]"));
    }

    void badFilesAreSurvived()
    {
        const QString broken = m_systemConfig + QStringLiteral("/menus/broken.menu");
        writeFile(broken, "<Menu><Name>Root</Name><Include>");
        QCOMPARE(load(broken), QString());

        // A menu merging itself, directly and through another file.
        const QString loop = m_systemConfig + QStringLiteral("/menus/loop.menu");
        const QString other = m_systemConfig + QStringLiteral("/menus/other.menu");
        writeFile(loop,
            menu(
                "<Name>Root</Name><DefaultAppDirs/><MergeFile>loop.menu</MergeFile>"
                "<MergeFile>other.menu</MergeFile>"
                "<Menu><Name>A</Name><Include><Filename>calc.desktop</Filename></Include></Menu>"));
        writeFile(other, menu("<Name>Other</Name><MergeFile>loop.menu</MergeFile>"));
        QCOMPARE(load(loop), QStringLiteral("A[Calculator]"));
    }

    void flattenListsEachProgramOnce()
    {
        writeFile(m_systemConfig + QStringLiteral("/menus/applications-merged/extra.menu"),
            menu("<Name>Applications</Name><Menu><Name>Drawing</Name>"
                 "<Include><Category>Graphics</Category></Include></Menu>"));
        const XdgMenu tree = loadXdgMenu(
            QString::fromLatin1(BuiltInMenuFile), m_paths, Desktop, QStringLiteral("C"));
        QStringList names;
        for (const DesktopEntry &entry : flattenMenu(tree.root))
            names << entry.name;
        QCOMPARE(names,
            QStringList({ QStringLiteral("Calculator"), QStringLiteral("Mixer"),
                QStringLiteral("Notepad"), QStringLiteral("Odd"), QStringLiteral("Paint") }));
    }
};

QTEST_GUILESS_MAIN(TestXdgMenu)
#include "tst_xdgmenu.moc"
