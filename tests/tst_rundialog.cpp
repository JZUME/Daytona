// SPDX-License-Identifier: GPL-3.0-or-later

#include "rundialog.h"
#include "settings.h"

#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;
using Kind = RunDecision::Kind;

class TestRunDialog : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private Q_SLOTS:
    void decisions()
    {
        QFile doc(m_dir.filePath(QStringLiteral("letter.txt")));
        QVERIFY(doc.open(QIODevice::WriteOnly));
        doc.close();

        QCOMPARE(decideRun(QStringLiteral("   ")).kind, Kind::Nothing);

        RunDecision d = decideRun(doc.fileName());
        QCOMPARE(d.kind, Kind::Open);
        QCOMPARE(d.target, doc.fileName());

        d = decideRun(QLatin1Char('"') + m_dir.path() + QLatin1Char('"'));
        QCOMPARE(d.kind, Kind::Open); // a quoted folder

        d = decideRun(QStringLiteral("~"));
        QCOMPARE(d.kind, Kind::Open);
        QCOMPARE(d.target, QDir::homePath());

        d = decideRun(QStringLiteral("https://example.com/page"));
        QCOMPARE(d.kind, Kind::Open);
        d = decideRun(QStringLiteral("mailto:someone@example.com"));
        QCOMPARE(d.kind, Kind::Open);

        d = decideRun(QStringLiteral("mousepad notes.txt"));
        QCOMPARE(d.kind, Kind::Command);
        QCOMPARE(d.program, QStringLiteral("mousepad"));
        QCOMPARE(d.target, QStringLiteral("mousepad notes.txt"));

        // An executable path runs rather than opens.
        d = decideRun(QStringLiteral("/bin/true"));
        QCOMPARE(d.kind, Kind::Command);
        QCOMPARE(d.program, QStringLiteral("/bin/true"));
    }

    void programLookup()
    {
        QVERIFY(programExists(QStringLiteral("sh")));
        QVERIFY(programExists(QStringLiteral("/bin/sh")));
        QVERIFY(!programExists(QStringLiteral("no-such-program-daytona")));
        QVERIFY(!programExists(QStringLiteral("/nonexistent/sh")));
        QVERIFY(!programExists(QString()));
    }

    void successAddsToHistory()
    {
        Settings settings(m_dir.filePath(QStringLiteral("ok.ini")));
        RunDialog dialog(&settings);
        dialog.prepare();
        const QString marker = m_dir.filePath(QStringLiteral("ran"));
        dialog.commandBox()->setEditText(QStringLiteral("touch '%1'").arg(marker));
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        QCOMPARE(settings.runHistory().value(0), QStringLiteral("touch '%1'").arg(marker));
        QElapsedTimer t;
        t.start();
        while (!QFile::exists(marker) && t.elapsed() < 5000)
            QTest::qWait(50);
        QVERIFY(QFile::exists(marker));

        // The next time, the last command is shown first.
        dialog.prepare();
        QCOMPARE(dialog.commandBox()->currentText(), QStringLiteral("touch '%1'").arg(marker));
    }

    void missingProgramKeepsDialogOpen()
    {
        Settings settings(m_dir.filePath(QStringLiteral("fail.ini")));
        RunDialog dialog(&settings);
        QString shownMessage;
        dialog.showError = [&](const QString &, const QString &message) { shownMessage = message; };
        dialog.prepare();
        dialog.show();
        dialog.commandBox()->setEditText(QStringLiteral("no-such-program-daytona --flag"));
        dialog.accept();
        QVERIFY(shownMessage.contains(QStringLiteral("no-such-program-daytona")));
        QVERIFY(dialog.isVisible());
        QVERIFY(settings.runHistory().isEmpty());
    }
};

QTEST_MAIN(TestRunDialog)
#include "tst_rundialog.moc"
