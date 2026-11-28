// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemlogo.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

class TestSystemLogo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parse()
    {
        const auto fields
            = parseOsRelease(QStringLiteral("# comment\n"
                                            "PRETTY_NAME=\"Debian GNU/Linux 13 (trixie)\"\n"
                                            "ID=debian\n"
                                            "  LOGO='ubuntu-logo'  \n"
                                            "NAME=\"Quote \\\"inside\\\" and \\\\ backslash\"\n"
                                            "BROKEN\n"
                                            "=novalue\n"));
        QCOMPARE(fields.value(QStringLiteral("ID")), QStringLiteral("debian"));
        QCOMPARE(fields.value(QStringLiteral("PRETTY_NAME")),
            QStringLiteral("Debian GNU/Linux 13 (trixie)"));
        QCOMPARE(fields.value(QStringLiteral("LOGO")), QStringLiteral("ubuntu-logo"));
        QCOMPARE(fields.value(QStringLiteral("NAME")),
            QStringLiteral("Quote \"inside\" and \\ backslash"));
        QCOMPARE(fields.size(), 4);
    }

    void name()
    {
        // The Start menu banner's text.
        QTemporaryDir empty;
        QCOMPARE(systemName(empty.path()), QStringLiteral("Linux"));
        QTemporaryDir debian;
        write(debian, "usr/lib/os-release", "NAME=\"Debian GNU/Linux\"\nID=debian\n");
        QCOMPARE(systemName(debian.path()), QStringLiteral("Debian GNU/Linux"));
        write(debian, "etc/os-release", "NAME=Ubuntu\n");
        QCOMPARE(systemName(debian.path()), QStringLiteral("Ubuntu"));
        QTemporaryDir noName;
        write(noName, "etc/os-release", "ID=x\nNAME=\"\"\n");
        QCOMPARE(systemName(noName.path()), QStringLiteral("Linux"));
    }

    void logoFromOsRelease()
    {
        // A distribution that names its logo (as Ubuntu, Fedora and Arch do).
        QTemporaryDir root;
        write(root, "etc/os-release", "ID=example\nLOGO=example-logo\n");
        writeImage(root, "usr/share/pixmaps/example-logo.png", Qt::red);
        const QIcon icon = systemLogo(root.path());
        QVERIFY(!icon.isNull());
        QCOMPARE(icon.pixmap(16).toImage().pixelColor(8, 8), QColor(Qt::red));
    }

    void debianKnownFiles()
    {
        // Debian names no logo; its own files are used, sharper ones first.
        QTemporaryDir root;
        write(root, "usr/lib/os-release", "ID=debian\n"); // /etc/os-release missing
        writeImage(root, "usr/share/pixmaps/debian-logo.png", Qt::blue);
        QCOMPARE(systemLogo(root.path()).pixmap(16).toImage().pixelColor(8, 8), QColor(Qt::blue));
        write(root, "usr/share/icons/desktop-base/scalable/emblems/emblem-debian.svg",
            "<svg xmlns='http://www.w3.org/2000/svg' width='48' height='48'>"
            "<rect width='48' height='48' fill='#00ff00'/></svg>");
        QCOMPARE(systemLogo(root.path()).pixmap(16).toImage().pixelColor(8, 8), QColor(Qt::green));
    }

    void missingLogoFallsBack()
    {
        QTemporaryDir root;
        // A LOGO that is not installed, and an ID without known files.
        write(root, "etc/os-release", "ID=unknown\nLOGO=unknown-logo\n");
        QVERIFY(systemLogo(root.path()).isNull());
        // No os-release at all.
        QTemporaryDir empty;
        QVERIFY(systemLogo(empty.path()).isNull());
        // A derivative is not given its parent's logo.
        QTemporaryDir derivative;
        write(derivative, "etc/os-release", "ID=raspbian\nID_LIKE=debian\n");
        writeImage(derivative, "usr/share/pixmaps/debian-logo.png", Qt::blue);
        QVERIFY(systemLogo(derivative.path()).isNull());
    }

    void logoMustBeAName()
    {
        // LOGO names an icon; a path is not followed.
        QTemporaryDir root;
        writeImage(root, "secret.png", Qt::red);
        write(root, "etc/os-release", "ID=x\nLOGO=../../secret\n");
        QVERIFY(systemLogo(root.path()).isNull());
    }

    void brokenFileIsSkipped()
    {
        QTemporaryDir root;
        write(root, "etc/os-release", "ID=debian\n");
        write(root, "usr/share/icons/desktop-base/scalable/emblems/emblem-debian.svg",
            "not an image");
        writeImage(root, "usr/share/pixmaps/debian-logo.png", Qt::blue);
        QCOMPARE(systemLogo(root.path()).pixmap(16).toImage().pixelColor(8, 8), QColor(Qt::blue));
    }

private:
    static void write(const QTemporaryDir &root, const char *path, const QByteArray &contents)
    {
        const QString file = root.filePath(QString::fromLatin1(path));
        QVERIFY(QDir().mkpath(QFileInfo(file).path()));
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(contents);
    }

    static void writeImage(const QTemporaryDir &root, const char *path, Qt::GlobalColor colour)
    {
        const QString file = root.filePath(QString::fromLatin1(path));
        QVERIFY(QDir().mkpath(QFileInfo(file).path()));
        QImage image(48, 48, QImage::Format_ARGB32);
        image.fill(colour);
        QVERIFY(image.save(file));
    }
};

QTEST_MAIN(TestSystemLogo)
#include "tst_systemlogo.moc"
