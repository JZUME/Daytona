// SPDX-License-Identifier: GPL-3.0-or-later

#include "toplevellist.h"
#include "windowmenu.h"

#include <QTest>

using namespace Daytona;
using Item = WindowMenuState::Item;

Q_DECLARE_METATYPE(Item)

class TestWindowMenu : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void states_data()
    {
        QTest::addColumn<bool>("minimized");
        QTest::addColumn<bool>("maximized");
        QTest::addColumn<bool>("restore");
        QTest::addColumn<bool>("minimize");
        QTest::addColumn<bool>("maximize");
        QTest::addColumn<Item>("defaultItem");
        QTest::newRow("normal") << false << false << false << true << true << Item::Close;
        QTest::newRow("maximised") << false << true << true << true << false << Item::Close;
        QTest::newRow("minimised") << true << false << true << false << true << Item::Restore;
        QTest::newRow("minimised from maximised")
            << true << true << true << false << true << Item::Restore;
    }

    void states()
    {
        QFETCH(bool, minimized);
        QFETCH(bool, maximized);
        Toplevel t;
        t.minimized = minimized;
        t.maximized = maximized;
        const WindowMenuState s = windowMenuState(t);
        QTEST(s.restore, "restore");
        QTEST(s.minimize, "minimize");
        QTEST(s.maximize, "maximize");
        QTEST(s.defaultItem, "defaultItem");
        QVERIFY(s.close);
        // No foreign-toplevel request can start a move or resize.
        QVERIFY(!s.move);
        QVERIFY(!s.size);
    }
};

QTEST_GUILESS_MAIN(TestWindowMenu)
#include "tst_windowmenu.moc"
