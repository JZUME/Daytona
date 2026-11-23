// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QTest>

// Loads the built plugin through Qt's plugin system, as a third-party
// application using QT_STYLE_OVERRIDE=daytona would.
class TestStylePlugin : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pluginIsFoundByKey()
    {
        QVERIFY2(QStyleFactory::keys().contains(QStringLiteral("daytona"), Qt::CaseInsensitive),
            qPrintable(QStyleFactory::keys().join(QStringLiteral(", "))));
        std::unique_ptr<QStyle> style(QStyleFactory::create(QStringLiteral("daytona")));
        QVERIFY(style);
        QCOMPARE(style->objectName(), QStringLiteral("daytona"));
    }

    // Run with QT_STYLE_OVERRIDE=daytona and XDG_CONFIG_HOME pointing at
    // tests/data/config: a third-party application picks up colors.ini.
    void overrideAppliesColourScheme()
    {
        QCOMPARE(QApplication::style()->objectName(), QStringLiteral("daytona"));
        const QPalette palette = QApplication::palette();
        QCOMPARE(palette.color(QPalette::Highlight), QColor(128, 0, 128));
        QCOMPARE(palette.color(QPalette::Button), QColor(0, 0, 0));
        QCOMPARE(palette.color(QPalette::ButtonText), QColor(255, 255, 255));
        // Keys the file leaves out are Windows Standard.
        QCOMPARE(palette.color(QPalette::Base), QColor(255, 255, 255));
    }
};

QTEST_MAIN(TestStylePlugin)
#include "tst_styleplugin.moc"
