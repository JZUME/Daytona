// SPDX-License-Identifier: GPL-3.0-or-later

#include "printscreen.h"

#include <QTest>

using Daytona::PrintScreen;

class TestPrintScreen : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyScreenToStandardOutput()
    {
        QCOMPARE(PrintScreen::grimArguments({}, {}), QStringList({ "-" }));
    }

    void oneScreenToStandardOutput()
    {
        QCOMPARE(PrintScreen::grimArguments(QStringLiteral("HDMI-A-1"), {}),
            QStringList({ "-o", "HDMI-A-1", "-" }));
    }

    void toFile()
    {
        QCOMPARE(PrintScreen::grimArguments(QStringLiteral("DP-1"), QStringLiteral("/tmp/a.png")),
            QStringList({ "-o", "DP-1", "/tmp/a.png" }));
        QCOMPARE(PrintScreen::grimArguments({}, QStringLiteral("/tmp/-o.png")),
            QStringList({ "/tmp/-o.png" }));
    }
};

QTEST_GUILESS_MAIN(TestPrintScreen)
#include "tst_printscreen.moc"
