// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayproperties.h"
#include "outputmanager.h"
#include "settings.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

// Stands in for the compositor: records scale requests and answers them at
// once, applying the scale when it accepts.
class FakeScaler : public ScreenScaler
{
public:
    double scale(const QString &screen) const override { return scales.value(screen); }
    QString identity(const QString &screen) const override
    {
        return QStringLiteral("Maker Model ") + screen;
    }
    void setScale(
        const QString &screen, double scale, const std::function<void(bool)> &done) override
    {
        requests.append({ screen, scale });
        if (accept)
            scales.insert(screen, scale);
        if (done)
            done(accept);
    }

    QHash<QString, double> scales { { QStringLiteral("DP-1"), 1.0 } };
    QList<QPair<QString, double>> requests;
    bool accept = true;
};

} // namespace

class TestDisplayProperties : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Keep the theme tool and settings inside a temporary home.
        QVERIFY(m_home.isValid());
        qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_home.filePath(QStringLiteral("config"))));
        qputenv("XDG_DATA_HOME", QFile::encodeName(m_home.filePath(QStringLiteral("data"))));
        qputenv("LABWC_PID", "");
    }

    void candidates()
    {
        QTemporaryDir a, b;
        for (const char *name : { "b.png", "A.bmp", "notes.txt", "c.JPG" })
            writeImage(a.filePath(QLatin1String(name)));
        writeImage(b.filePath(QStringLiteral("z.png")));
        const QStringList files = wallpaperCandidates({ a.path(), b.path(), a.path() });
        QCOMPARE(files,
            QStringList({ a.filePath(QStringLiteral("A.bmp")), a.filePath(QStringLiteral("b.png")),
                // Any case, as Windows-era files are often upper case.
                a.filePath(QStringLiteral("c.JPG")), b.filePath(QStringLiteral("z.png")) }));
        QCOMPARE(wallpaperCandidates({ a.path() }, 1).size(), 1);
        QVERIFY(wallpaperCandidates({ a.filePath(QStringLiteral("missing")) }).isEmpty());
    }

    void backgroundTab()
    {
        QTemporaryDir dir;
        const QString wall = dir.filePath(QStringLiteral("wall.png"));
        writeImage(wall);
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        settings.setWallpaperPath(wall);
        settings.setWallpaperMode(WallpaperMode::Tile);
        DisplayPropertiesDialog dialog(&settings);
        // The current wallpaper is listed and selected, after (None).
        QCOMPARE(dialog.wallpaperList()->item(0)->text(), QStringLiteral("(None)"));
        QCOMPARE(dialog.selectedWallpaper(), wall);
        QCOMPARE(dialog.selectedMode(), WallpaperMode::Tile);
        QVERIFY(!dialog.applyButton()->isEnabled());

        dialog.wallpaperList()->setCurrentRow(0); // (None)
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.apply();
        QVERIFY(settings.wallpaperPath().isEmpty());
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void appearanceTab()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        DisplayPropertiesDialog dialog(&settings);
        QCOMPARE(dialog.schemeBox()->currentText(), QStringLiteral("Windows Standard"));
        QCOMPARE(dialog.candidateColours().value(QStringLiteral("ActiveTitle")), QColor(0, 0, 128));
        dialog.schemeBox()->setCurrentText(QStringLiteral("High Contrast Black"));
        QCOMPARE(
            dialog.candidateColours().value(QStringLiteral("ActiveTitle")), QColor(128, 0, 128));

        // Loading a .theme file lists and selects it.
        const QString theme = QStringLiteral(DAYTONA_TEST_DATA "/themes/Sample.theme");
        dialog.loadThemeFile(theme);
        QCOMPARE(dialog.schemeBox()->currentText(), QStringLiteral("Sample"));
        QCOMPARE(dialog.candidateColours().value(QStringLiteral("ActiveTitle")), QColor(128, 0, 0));
        const int count = dialog.schemeBox()->count();
        dialog.loadThemeFile(theme); // not twice
        QCOMPARE(dialog.schemeBox()->count(), count);
    }

    void fontSizeChoices()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenScaler(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.prepare();
        QVERIFY(dialog.fontSizeBox()->isEnabled());
        QCOMPARE(dialog.fontSizeBox()->count(), 2);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Small Fonts"));
        QCOMPARE(dialog.selectedScale(), 1.0);
        QVERIFY(!dialog.applyButton()->isEnabled());

        dialog.selectScale(1.25);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Large Fonts"));
        QVERIFY(dialog.applyButton()->isEnabled());
        // A custom size gets one entry of its own, reused for the next one.
        dialog.selectScale(1.5);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Other (150%)"));
        dialog.selectScale(2.0);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Other (200%)"));
        QCOMPARE(dialog.fontSizeBox()->count(), 3);
        QCOMPARE(dialog.selectedScale(), 2.0);

        // Opening the dialog again shows the screen's actual scale.
        scaler.scales.insert(QStringLiteral("DP-1"), 1.75);
        dialog.prepare();
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Other (175%)"));
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void fontSizeKeptWhenConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenScaler(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        int asked = 0;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            ++asked;
            done(true);
        });
        dialog.prepare();

        dialog.selectScale(1.25);
        dialog.apply();
        QCOMPARE(scaler.requests.size(), 1);
        QCOMPARE(scaler.requests.first(), qMakePair(QStringLiteral("DP-1"), 1.25));
        QCOMPARE(asked, 1);
        // Saved for that monitor, not for the connector.
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 1.25);
        QCOMPARE(settings.outputScale(QStringLiteral("DP-1")), 0.0);
        QVERIFY(!dialog.applyButton()->isEnabled());

        // Applying again without a change asks nothing.
        dialog.apply();
        QCOMPARE(scaler.requests.size(), 1);
    }

    void fontSizeRevertsWhenNotConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenScaler(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.setScaleConfirmation([](const std::function<void(bool)> &done) { done(false); });
        dialog.prepare();

        dialog.selectScale(2.0);
        dialog.apply();
        // Set, then set back.
        QCOMPARE(scaler.requests.size(), 2);
        QCOMPARE(scaler.requests.at(0).second, 2.0);
        QCOMPARE(scaler.requests.at(1).second, 1.0);
        QCOMPARE(scaler.scale(QStringLiteral("DP-1")), 1.0);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Small Fonts"));
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 0.0);
        QVERIFY(!dialog.applyButton()->isEnabled());
    }

    void fontSizeRefusedByCompositor()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        scaler.accept = false;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenScaler(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        bool asked = false;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            asked = true;
            done(true);
        });
        dialog.prepare();

        dialog.selectScale(1.25);
        dialog.apply();
        QCOMPARE(scaler.requests.size(), 1);
        QVERIFY(!asked);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Small Fonts"));
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 0.0);
        // The warning is not modal; close it.
        const auto boxes = dialog.findChildren<QMessageBox *>();
        QCOMPARE(boxes.size(), 1);
        QCOMPARE(boxes.first()->text(), QStringLiteral("The font size could not be changed."));
        boxes.first()->close();
    }

    void fontSizeUnavailableWithoutCompositorSupport()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenScaler(&scaler);
        dialog.setScreenName(QStringLiteral("UNKNOWN-1")); // not reported
        dialog.prepare();
        QVERIFY(!dialog.fontSizeBox()->isEnabled());
        QVERIFY(!dialog.customFontSizeButton()->isEnabled());

        DisplayPropertiesDialog noScaler(&settings);
        QVERIFY(!noScaler.fontSizeBox()->isEnabled());
        noScaler.selectScale(1.25);
        noScaler.apply(); // nothing to apply it with; must not crash
    }

    void customFontSizeDialog()
    {
        CustomFontSizeDialog custom;
        QCOMPARE(custom.windowTitle(), QStringLiteral("Custom Font Size"));
        custom.setPercent(150);
        QCOMPARE(custom.percent(), 150);
        custom.setPercent(500);
        QCOMPARE(custom.percent(), 300);
        custom.setPercent(10);
        QCOMPARE(custom.percent(), 100);
        custom.percentBox()->setCurrentText(QStringLiteral("abc"));
        QCOMPARE(custom.percent(), 100);
    }

    void applyRunsThemeTool()
    {
        Settings settings(Settings::defaultFilePath());
        DisplayPropertiesDialog dialog(&settings);
        dialog.setThemeTool(QStringLiteral(DAYTONA_THEMETOOL));
        QSignalSpy applied(&dialog, &DisplayPropertiesDialog::schemeApplied);

        dialog.loadThemeFile(QStringLiteral(DAYTONA_TEST_DATA "/themes/Sample.theme"));
        dialog.apply();
        QVERIFY(applied.wait(10000));
        QCOMPARE(applied.first().first().toBool(), true);
        QVERIFY(QFile::exists(colourFilePath()));
        // The tool records the scheme name in the file; read it afresh.
        QCOMPARE(Settings(Settings::defaultFilePath()).colourScheme(), QStringLiteral("Sample"));
        QCOMPARE(settings.themeFiles().size(), 1);

        // Listed with the schemes next time.
        dialog.prepare();
        QCOMPARE(dialog.schemeBox()->currentText(), QStringLiteral("Sample"));
        QVERIFY(dialog.schemeBox()->findText(QStringLiteral("High Contrast White")) >= 0);
    }

private:
    static void writeImage(const QString &path)
    {
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(Qt::red);
        if (path.endsWith(QLatin1String(".txt"))) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            return;
        }
        QVERIFY(image.save(path, path.endsWith(QLatin1String(".JPG")) ? "JPG" : nullptr));
    }

    QTemporaryDir m_home;
};

QTEST_MAIN(TestDisplayProperties)
#include "tst_displayproperties.moc"
