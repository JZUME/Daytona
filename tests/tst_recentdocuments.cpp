// SPDX-License-Identifier: GPL-3.0-or-later

#include "recentdocuments.h"
#include "startmenu.h"

#include <QAction>
#include <QFile>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace Daytona;

class TestRecentDocuments : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString touch(const QString &name)
    {
        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("cannot create fixture");
        return path;
    }

    QString writeXbel(const QByteArray &contents)
    {
        const QString path = m_dir.filePath(QStringLiteral("recently-used.xbel"));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            qFatal("cannot write xbel");
        f.write(contents);
        return path;
    }

    static QByteArray bookmark(const QString &path, const char *modified)
    {
        // As in a real XBEL file, '&' in the URL is escaped for XML.
        QByteArray href = QUrl::fromLocalFile(path).toEncoded();
        href.replace("&", "&amp;");
        return "<bookmark href=\"" + href + "\" added=\"" + modified + "\" modified=\"" + modified
            + "\" visited=\"" + modified + "\"/>\n";
    }

private Q_SLOTS:
    void newestFirst()
    {
        const QString a = touch(QStringLiteral("a.txt"));
        const QString b = touch(QStringLiteral("b & c.txt"));
        const QString xbel = writeXbel("<?xml version=\"1.0\"?>\n<xbel version=\"1.0\">\n"
            + bookmark(a, "2026-01-01T10:00:00.000000Z") + bookmark(b, "2026-03-01T10:00:00Z")
            + "</xbel>\n");
        const QList<RecentDocument> docs = readRecentDocuments(xbel);
        QCOMPARE(docs.size(), 2);
        QCOMPARE(docs[0].path, b);
        QCOMPARE(docs[1].path, a);
    }

    void limitedTo15()
    {
        QByteArray body = "<xbel version=\"1.0\">\n";
        for (int i = 0; i < 20; ++i) {
            const QString path = touch(QStringLiteral("doc%1.txt").arg(i));
            const QByteArray when = QStringLiteral("2026-01-%1T00:00:00Z")
                                        .arg(i + 1, 2, 10, QLatin1Char('0'))
                                        .toLatin1();
            body += bookmark(path, when.constData());
        }
        body += "</xbel>\n";
        const QList<RecentDocument> docs = readRecentDocuments(writeXbel(body));
        QCOMPARE(docs.size(), 15);
        QVERIFY(docs.first().path.endsWith(QStringLiteral("doc19.txt")));
    }

    void skipsMissingAndNonLocal()
    {
        const QString kept = touch(QStringLiteral("kept.txt"));
        const QString xbel = writeXbel("<xbel version=\"1.0\">\n"
            + bookmark(m_dir.filePath(QStringLiteral("gone.txt")), "2026-01-02T00:00:00Z")
            + "<bookmark href=\"https://example.com/x.pdf\" modified=\"2026-01-03T00:00:00Z\"/>\n"
            + "<bookmark href=\"sftp://host/y.txt\" modified=\"2026-01-04T00:00:00Z\"/>\n"
            + "<bookmark modified=\"2026-01-05T00:00:00Z\"/>\n"
            + bookmark(m_dir.path(), "2026-01-06T00:00:00Z") // a directory, not a file
            + bookmark(kept, "2026-01-01T00:00:00Z") + "</xbel>\n");
        const QList<RecentDocument> docs = readRecentDocuments(xbel);
        QCOMPARE(docs.size(), 1);
        QCOMPARE(docs.first().path, kept);
    }

    void malformedXml()
    {
        const QString first = touch(QStringLiteral("first.txt"));
        const QString xbel = writeXbel("<xbel version=\"1.0\">\n"
            + bookmark(first, "2026-01-01T00:00:00Z") + "<bookmark href=\"oops\n<<<garbage");
        const QList<RecentDocument> docs = readRecentDocuments(xbel);
        QCOMPARE(docs.size(), 1); // what came before the error is still used
        QVERIFY(readRecentDocuments(writeXbel("not xml at all")).isEmpty());
        QVERIFY(readRecentDocuments(m_dir.filePath(QStringLiteral("missing.xbel"))).isEmpty());
    }

    void documentsMenu()
    {
        StartMenu menu(nullptr, nullptr);
        menu.setRecentDocumentsFile(m_dir.filePath(QStringLiteral("none.xbel")));
        Q_EMIT menu.documentsMenu()->aboutToShow();
        QList<QAction *> actions = menu.documentsMenu()->actions();
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions.first()->text(), QStringLiteral("(Empty)"));
        QVERIFY(!actions.first()->isEnabled());

        const QString doc = touch(QStringLiteral("R&D notes.txt"));
        menu.setRecentDocumentsFile(writeXbel(
            "<xbel version=\"1.0\">" + bookmark(doc, "2026-02-02T00:00:00Z") + "</xbel>"));
        Q_EMIT menu.documentsMenu()->aboutToShow();
        actions = menu.documentsMenu()->actions();
        QCOMPARE(actions.size(), 1);
        QCOMPARE(actions.first()->text(), QStringLiteral("R&&D notes.txt")); // no stray mnemonic
        QVERIFY(actions.first()->isEnabled());
    }
};

QTEST_MAIN(TestRecentDocuments)
#include "tst_recentdocuments.moc"
