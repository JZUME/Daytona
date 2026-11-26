// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenchooser.h"

#include <QListWidget>
#include <QTest>

using namespace Daytona;

class TestScreenChooser : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parse()
    {
        // xdg-desktop-portal-wlr 0.8: name, then description.
        ScreenSource s = parseScreenSource(QStringLiteral("Monitor: DP-1 Dell Inc. U2415 (DP-1)"));
        QVERIFY(!s.window);
        QCOMPARE(s.name, QStringLiteral("DP-1"));
        QCOMPARE(s.description, QStringLiteral("Dell Inc. U2415 (DP-1)"));

        s = parseScreenSource(QStringLiteral("Monitor: HEADLESS-1"));
        QCOMPARE(s.name, QStringLiteral("HEADLESS-1"));
        QVERIFY(s.description.isEmpty());

        // The window's identifier is left out; brackets in its title stay.
        s = parseScreenSource(QStringLiteral("Window: Notes (draft) - FeatherPad (a1b2)"));
        QVERIFY(s.window);
        QCOMPARE(s.name, QStringLiteral("Notes (draft) - FeatherPad"));
        s = parseScreenSource(QStringLiteral("Window: a1b2"));
        QCOMPARE(s.name, QStringLiteral("a1b2"));

        // Earlier versions list only output names.
        s = parseScreenSource(QStringLiteral("eDP-1"));
        QVERIFY(!s.window);
        QCOMPARE(s.name, QStringLiteral("eDP-1"));
        QCOMPARE(s.line, QStringLiteral("eDP-1"));
    }

    void labels()
    {
        const ScreenSource window = parseScreenSource(QStringLiteral("Window: Inbox (c3)"));
        QCOMPARE(
            screenSourceLabels({ parseScreenSource(QStringLiteral("Monitor: DP-1 Dell")), window }),
            QStringList({ QStringLiteral("Entire screen"), QStringLiteral("Inbox") }));
        QCOMPARE(screenSourceLabels({ parseScreenSource(QStringLiteral("Monitor: DP-1 Dell")),
                     parseScreenSource(QStringLiteral("HDMI-A-1")) }),
            QStringList({ QStringLiteral("Dell"), QStringLiteral("HDMI-A-1") }));
    }

    void chooses()
    {
        const QList<ScreenSource> sources
            = { parseScreenSource(QStringLiteral("Monitor: DP-1 Dell")),
                  parseScreenSource(QStringLiteral("Window: Inbox (c3)")) };
        ScreenChooser chooser(sources);
        // The first source is chosen until another is picked; the line goes
        // back to xdg-desktop-portal-wlr unchanged.
        QCOMPARE(chooser.chosenLine(), QStringLiteral("Monitor: DP-1 Dell"));
        chooser.list()->setCurrentRow(1);
        QCOMPARE(chooser.chosenLine(), QStringLiteral("Window: Inbox (c3)"));

        ScreenChooser empty({});
        QVERIFY(empty.chosenLine().isEmpty());
    }
};

QTEST_MAIN(TestScreenChooser)
#include "tst_screenchooser.moc"
