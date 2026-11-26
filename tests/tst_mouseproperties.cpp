// SPDX-License-Identifier: GPL-3.0-or-later

#include "mouseproperties.h"
#include "settings.h"

#include <QComboBox>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace Daytona;

class TestMouseProperties : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // The cursor theme this build generated.
        qputenv("XCURSOR_PATH", DAYTONA_CURSOR_ICONS);
    }

    void schemes()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        MousePropertiesDialog dialog(&settings);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Mouse Properties"));
        QCOMPARE(dialog.schemeBox()->count(), 3);
        QCOMPARE(dialog.schemeBox()->currentText(), QStringLiteral("Standard"));
        QCOMPARE(dialog.pointerList()->topLevelItemCount(), 13);
        QCOMPARE(dialog.pointerList()->topLevelItem(0)->text(0), QStringLiteral("Normal Select"));
        QVERIFY(!dialog.applyButton()->isEnabled());

        dialog.schemeBox()->setCurrentIndex(2);
        QCOMPARE(dialog.selectedScheme(), PointerScheme::ExtraLarge);
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.apply();
        QCOMPARE(settings.pointerScheme(), PointerScheme::ExtraLarge);
        QVERIFY(!dialog.applyButton()->isEnabled());
        // It takes effect at the next log on, and says so (not modal).
        const auto boxes = dialog.findChildren<QMessageBox *>();
        QCOMPARE(boxes.size(), 1);
        QVERIFY(boxes.first()->text().contains(QStringLiteral("next time you log on")));
        boxes.first()->close();

        MousePropertiesDialog again(&settings);
        QCOMPARE(again.schemeBox()->currentText(), QStringLiteral("Standard (extra large)"));
    }

    void previews()
    {
        // Each scheme's pointers, drawn at its size from the cursor theme.
        for (const auto &[scheme, size] :
            { std::pair { PointerScheme::Standard, 32 }, std::pair { PointerScheme::Large, 64 },
                std::pair { PointerScheme::ExtraLarge, 96 } }) {
            for (qreal ratio : { 1.0, 2.0 }) {
                const QPixmap pixmap = pointerPreview(QStringLiteral("left_ptr"), scheme, ratio);
                QVERIFY(!pixmap.isNull());
                QCOMPARE(pixmap.deviceIndependentSize().toSize(), QSize(size, size));
            }
        }
        QVERIFY(
            pointerPreview(QStringLiteral("no-such-cursor"), PointerScheme::Standard, 1).isNull());

        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        MousePropertiesDialog dialog(&settings);
        for (int i = 0; i < dialog.pointerList()->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = dialog.pointerList()->topLevelItem(i);
            QVERIFY2(!item->icon(1).isNull(), qPrintable(item->text(0)));
        }
    }
};

QTEST_MAIN(TestMouseProperties)
#include "tst_mouseproperties.moc"
