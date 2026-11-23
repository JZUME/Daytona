// SPDX-License-Identifier: GPL-3.0-or-later

#include "tasklayout.h"

#include <QTest>

using Daytona::computeTaskLayout;
using Daytona::TaskLayout;

Q_DECLARE_METATYPE(TaskLayout)

class TestTaskLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void layout_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<int>("available");
        QTest::addColumn<TaskLayout>("expected");
        // Defaults: max 160, min 40, spacing 3, arrows 16.
        QTest::newRow("no windows") << 0 << 800 << TaskLayout {};
        QTest::newRow("no space") << 3 << 0 << TaskLayout {};
        QTest::newRow("one window is capped at 160") << 1 << 800 << TaskLayout { 160, 1, false };
        QTest::newRow("five capped") << 5 << 820 << TaskLayout { 160, 5, false };
        QTest::newRow("shared equally") << 10 << 800 << TaskLayout { 77, 10, false };
        QTest::newRow("exactly the minimum") << 4 << 169 << TaskLayout { 40, 4, false };
        // One pixel short of the minimum: page, leaving room for the arrows.
        // 168 - 16 - 3 = 149 for buttons -> 3 per page at (149 - 6) / 3 = 47.
        QTest::newRow("pages when too narrow") << 4 << 168 << TaskLayout { 47, 3, true };
        QTest::newRow("many windows") << 50 << 800 << TaskLayout { 40, 18, true };
        QTest::newRow("tiny space still shows one")
            << 5 << 30 << TaskLayout { 11, 1, true }; // 30 - 16 - 3
    }

    void layout()
    {
        QFETCH(int, count);
        QFETCH(int, available);
        QFETCH(TaskLayout, expected);
        const TaskLayout actual = computeTaskLayout(count, available);
        QCOMPARE(actual.buttonWidth, expected.buttonWidth);
        QCOMPARE(actual.perPage, expected.perPage);
        QCOMPARE(actual.paged, expected.paged);
    }

    void pageFitsInAvailableSpace()
    {
        for (int count = 1; count < 80; ++count) {
            for (int available = 20; available < 1200; available += 37) {
                const TaskLayout l = computeTaskLayout(count, available);
                const int used
                    = l.perPage * l.buttonWidth + (l.perPage - 1) * 3 + (l.paged ? 3 + 16 : 0);
                QVERIFY2(used <= available,
                    qPrintable(QStringLiteral("%1 windows in %2px used %3")
                            .arg(count)
                            .arg(available)
                            .arg(used)));
                QVERIFY(l.buttonWidth <= 160);
                QVERIFY(l.perPage >= 1 && l.perPage <= count);
            }
        }
    }
};

QTEST_GUILESS_MAIN(TestTaskLayout)
#include "tst_tasklayout.moc"
