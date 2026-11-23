// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationindex.h"
#include "desktopentry.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

class TestDesktopEntry : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tmp;

    QString write(const QString &relativePath, const QByteArray &contents)
    {
        const QString path = m_tmp.filePath(relativePath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("cannot write fixture");
        f.write(contents);
        return path;
    }

    DesktopEntry parse(const QByteArray &contents, const QString &locale = QStringLiteral("C"))
    {
        const QString path = write(QStringLiteral("single.desktop"), contents);
        std::optional<DesktopEntry> e
            = DesktopEntry::parse(path, QStringLiteral("single.desktop"), locale);
        if (!e)
            qFatal("fixture did not parse");
        return *e;
    }

    static DesktopEntry withExec(const QString &exec)
    {
        DesktopEntry e;
        e.exec = exec;
        e.name = QStringLiteral("My App");
        e.icon = QStringLiteral("my-icon");
        e.filePath = QStringLiteral("/usr/share/applications/my.desktop");
        return e;
    }

private Q_SLOTS:
    void basicKeys()
    {
        const DesktopEntry e
            = parse("# comment\n[Desktop Entry]\nType=Application\nName=Notepad\n"
                    "Icon=accessories-text-editor\nExec=notepad %F\nTerminal=false\n"
                    "Categories=Utility;TextEditor;\nPath=/tmp\n"
                    "[Desktop Action New]\nName=Ignored\nExec=ignored\n");
        QCOMPARE(e.type, QStringLiteral("Application"));
        QCOMPARE(e.name, QStringLiteral("Notepad"));
        QCOMPARE(e.icon, QStringLiteral("accessories-text-editor"));
        QCOMPARE(e.exec, QStringLiteral("notepad %F"));
        QCOMPARE(e.categories,
            (QStringList { QStringLiteral("Utility"), QStringLiteral("TextEditor") }));
        QCOMPARE(e.workingDirectory, QStringLiteral("/tmp"));
        QVERIFY(!e.terminal);
    }

    void noMainGroupIsRejected()
    {
        const QString path = write(QStringLiteral("bad.desktop"), "[Other]\nName=x\n");
        QVERIFY(!DesktopEntry::parse(path, QStringLiteral("bad.desktop"), QStringLiteral("C")));
        QVERIFY(!DesktopEntry::parse(m_tmp.filePath(QStringLiteral("missing.desktop")),
            QStringLiteral("missing.desktop"), QStringLiteral("C")));
    }

    void escapesAndLists()
    {
        const DesktopEntry e = parse("[Desktop Entry]\nName=Tab\\tand\\sspace\\\\\n"
                                     "Categories=A\\;B;C;;\n");
        QCOMPARE(e.name, QStringLiteral("Tab\tand space\\"));
        QCOMPARE(e.categories, (QStringList { QStringLiteral("A;B"), QStringLiteral("C") }));
    }

    void localisedName()
    {
        const QByteArray file = "[Desktop Entry]\nName=Calculator\nName[de]=Rechner\n"
                                "Name[de_CH]=Taschenrechner\nName[sr@latin]=Kalkulator\n";
        QCOMPARE(parse(file, QStringLiteral("de_DE.UTF-8")).name, QStringLiteral("Rechner"));
        QCOMPARE(parse(file, QStringLiteral("de_CH.UTF-8")).name, QStringLiteral("Taschenrechner"));
        QCOMPARE(parse(file, QStringLiteral("sr_RS@latin")).name, QStringLiteral("Kalkulator"));
        QCOMPARE(parse(file, QStringLiteral("fr_FR.UTF-8")).name, QStringLiteral("Calculator"));
        QCOMPARE(parse(file, QStringLiteral("C")).name, QStringLiteral("Calculator"));
    }

    void localeKeyOrder()
    {
        QCOMPARE(localeKeys(QStringLiteral("sr_RS.UTF-8@latin")),
            (QStringList { QStringLiteral("sr_RS@latin"), QStringLiteral("sr_RS"),
                QStringLiteral("sr@latin"), QStringLiteral("sr") }));
        QCOMPARE(localeKeys(QStringLiteral("C.UTF-8")), QStringList());
    }

    void visibility()
    {
        DesktopEntry e = withExec(QStringLiteral("app"));
        e.type = QStringLiteral("Application");
        QVERIFY(e.shouldShow(QStringLiteral("Daytona")));

        DesktopEntry link = e;
        link.type = QStringLiteral("Link");
        QVERIFY(!link.shouldShow(QStringLiteral("Daytona")));

        DesktopEntry noDisplay = e;
        noDisplay.noDisplay = true;
        QVERIFY(!noDisplay.shouldShow(QStringLiteral("Daytona")));

        DesktopEntry hidden = e;
        hidden.hidden = true;
        QVERIFY(!hidden.shouldShow(QStringLiteral("Daytona")));

        DesktopEntry onlyGnome = e;
        onlyGnome.onlyShowIn = { QStringLiteral("GNOME") };
        QVERIFY(!onlyGnome.shouldShow(QStringLiteral("Daytona")));
        onlyGnome.onlyShowIn << QStringLiteral("Daytona");
        QVERIFY(onlyGnome.shouldShow(QStringLiteral("Daytona")));

        DesktopEntry notHere = e;
        notHere.notShowIn = { QStringLiteral("Daytona") };
        QVERIFY(!notHere.shouldShow(QStringLiteral("Daytona")));
    }

    void tryExec()
    {
        DesktopEntry e = withExec(QStringLiteral("app"));
        e.type = QStringLiteral("Application");
        e.tryExec = QStringLiteral("/bin/sh");
        QVERIFY(e.shouldShow(QStringLiteral("Daytona")));
        e.tryExec = QStringLiteral("sh");
        QVERIFY(e.shouldShow(QStringLiteral("Daytona")));
        e.tryExec = QStringLiteral("/nonexistent/program");
        QVERIFY(!e.shouldShow(QStringLiteral("Daytona")));
        e.tryExec = QStringLiteral("no-such-program-daytona");
        QVERIFY(!e.shouldShow(QStringLiteral("Daytona")));
    }

    void mainCategory()
    {
        DesktopEntry e;
        e.categories
            = { QStringLiteral("GTK"), QStringLiteral("Office"), QStringLiteral("Utility") };
        QCOMPARE(e.mainCategory(), QStringLiteral("Office")); // first main category listed
        e.categories = { QStringLiteral("GTK"), QStringLiteral("TextEditor") };
        QCOMPARE(e.mainCategory(), QString());
    }

    void execFieldCodes_data()
    {
        QTest::addColumn<QString>("exec");
        QTest::addColumn<QStringList>("files");
        QTest::addColumn<QStringList>("expected");
        const QStringList none;
        const QStringList two = { QStringLiteral("/tmp/a b.txt"), QStringLiteral("/tmp/c.txt") };
        QTest::newRow("plain") << "app --flag" << none << QStringList { "app", "--flag" };
        QTest::newRow("%f first file") << "app %f" << two << QStringList { "app", "/tmp/a b.txt" };
        QTest::newRow("%f no files") << "app %f" << none << QStringList { "app" };
        QTest::newRow("%F all files")
            << "app %F" << two << QStringList { "app", "/tmp/a b.txt", "/tmp/c.txt" };
        QTest::newRow("%U no files") << "app %U --x" << none << QStringList { "app", "--x" };
        QTest::newRow("%u first url") << "app %u" << two << QStringList { "app", "/tmp/a b.txt" };
        QTest::newRow("%i with icon")
            << "app %i" << none << QStringList { "app", "--icon", "my-icon" };
        QTest::newRow("%c name") << "app --title=%c" << none
                                 << QStringList { "app", "--title=My App" };
        QTest::newRow("%k file") << "app %k" << none
                                 << QStringList { "app", "/usr/share/applications/my.desktop" };
        QTest::newRow("%% literal") << "app 100%%" << none << QStringList { "app", "100%" };
        QTest::newRow("deprecated removed")
            << "app %d%D%n%N%v%m x" << none << QStringList { "app", "", "x" };
        QTest::newRow("quoted space")
            << "\"my app\" \"a b\"" << none << QStringList { "my app", "a b" };
        QTest::newRow("quoted escapes") << "app \"say \\\"hi\\\" \\$HOME \\\\ \\`x\\`\"" << none
                                        << QStringList { "app", "say \"hi\" $HOME \\ `x`" };
        QTest::newRow("shell syntax is literal")
            << "app ; rm -rf / && x | y" << none
            << QStringList { "app", ";", "rm", "-rf", "/", "&&", "x", "|", "y" };
        QTest::newRow("tabs separate") << "app\t-x" << none << QStringList { "app", "-x" };
    }

    void execFieldCodes()
    {
        QFETCH(QString, exec);
        QFETCH(QStringList, files);
        QFETCH(QStringList, expected);
        const std::optional<QStringList> cmd = withExec(exec).commandLine(files);
        QVERIFY(cmd);
        QCOMPARE(*cmd, expected);
    }

    void invalidExec_data()
    {
        QTest::addColumn<QString>("exec");
        QTest::newRow("empty") << "";
        QTest::newRow("unbalanced quote") << "app \"oops";
        QTest::newRow("unknown code") << "app %z";
        QTest::newRow("trailing percent") << "app %";
        QTest::newRow("%F inside an argument") << "app --files=%F";
        QTest::newRow("only a field code") << "%f";
    }

    void invalidExec()
    {
        QFETCH(QString, exec);
        QVERIFY(!withExec(exec).commandLine().has_value());
    }

    void scanPrecedenceAndOrder()
    {
        const QString home = m_tmp.filePath(QStringLiteral("home/applications"));
        const QString system = m_tmp.filePath(QStringLiteral("system/applications"));
        const QByteArray app = "[Desktop Entry]\nType=Application\nExec=x\nName=";
        write(QStringLiteral("system/applications/zeta.desktop"), app + "Zeta\n");
        write(QStringLiteral("system/applications/alpha.desktop"), app + "Alpha (system)\n");
        write(QStringLiteral("home/applications/alpha.desktop"), app + "Alpha (user)\n");
        // A user's Hidden=true entry hides the system one entirely.
        write(QStringLiteral("system/applications/gone.desktop"), app + "Gone\n");
        write(QStringLiteral("home/applications/gone.desktop"), app + "Gone\nHidden=true\n");
        // Subdirectories make IDs with '-'.
        write(QStringLiteral("system/applications/kde/konsole.desktop"), app + "Konsole\n");
        write(
            QStringLiteral("system/applications/hidden.desktop"), app + "Hidden\nNoDisplay=true\n");

        const QList<DesktopEntry> entries = ApplicationIndex::scan(
            { home, system }, QStringLiteral("Daytona"), QStringLiteral("C"));
        QStringList names;
        QStringList ids;
        for (const DesktopEntry &e : entries) {
            names << e.name;
            ids << e.id;
        }
        QCOMPARE(names,
            (QStringList { QStringLiteral("Alpha (user)"), QStringLiteral("Konsole"),
                QStringLiteral("Zeta") }));
        QVERIFY(ids.contains(QStringLiteral("kde-konsole.desktop")));
    }

    void indexWatchesForChanges()
    {
        QTemporaryDir dir;
        const QString apps = dir.filePath(QStringLiteral("applications"));
        QDir().mkpath(apps);
        ApplicationIndex index;
        QSignalSpy changed(&index, &ApplicationIndex::changed);
        index.start({ apps });
        QVERIFY(changed.wait(5000));
        QCOMPARE(index.entries().size(), 0);

        QFile f(apps + QStringLiteral("/new.desktop"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[Desktop Entry]\nType=Application\nName=New\nExec=new\n");
        f.close();
        QVERIFY(changed.wait(5000));
        QCOMPARE(index.entries().size(), 1);
        QCOMPARE(index.entries().first().name, QStringLiteral("New"));
    }

    void launchUsesNoShell()
    {
        // The file name contains shell syntax; it must arrive as one
        // argument and create exactly that file.
        const QString target = m_tmp.filePath(QStringLiteral("made; echo pwned"));
        DesktopEntry e = withExec(QStringLiteral("touch %f"));
        QVERIFY(launch(e, { target }).ok);
        QElapsedTimer t;
        t.start();
        while (!QFile::exists(target) && t.elapsed() < 5000)
            QTest::qWait(50);
        QVERIFY(QFile::exists(target));
    }

    void launchFailures()
    {
        QVERIFY(!launch(withExec(QStringLiteral("app \"broken"))).ok);
        QVERIFY(!launch(withExec(QStringLiteral("/nonexistent/daytona-program"))).ok);
        DesktopEntry terminal = withExec(QStringLiteral("top"));
        terminal.terminal = true;
        QVERIFY(!launch(terminal, {}, {}).ok); // no terminal configured
    }
};

QTEST_GUILESS_MAIN(TestDesktopEntry)
#include "tst_desktopentry.moc"
