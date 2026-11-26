// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayproperties.h"
#include "outputmanager.h"
#include "settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QTemporaryDir>
#include <QTest>

using namespace Daytona;

namespace {

// Stands in for the compositor: records mode and scale requests and
// answers them at once, applying them when it accepts.
class FakeScaler : public ScreenSettings
{
public:
    double scale(const QString &screen) const override { return scales.value(screen); }
    QList<ScreenMode> modes(const QString &screen) const override
    {
        return screen == QStringLiteral("DP-1") ? screenModes : QList<ScreenMode>();
    }
    ScreenMode currentMode(const QString &screen) const override
    {
        return screen == QStringLiteral("DP-1") ? current : ScreenMode();
    }
    QString identity(const QString &screen) const override
    {
        return QStringLiteral("Maker Model ") + screen;
    }
    void configure(const QString &screen, const ScreenMode &mode, double scale,
        const std::function<void(bool)> &done) override
    {
        if (scale > 0)
            requests.append({ screen, scale });
        if (mode.isValid())
            modeRequests.append(modeKey(mode));
        if (accept) {
            if (scale > 0)
                scales.insert(screen, scale);
            if (mode.isValid())
                current = mode;
        }
        if (done)
            done(accept);
    }

    static ScreenMode mode(int w, int h, int refresh, bool preferred = false)
    {
        ScreenMode m;
        m.size = QSize(w, h);
        m.refresh = refresh;
        m.preferred = preferred;
        return m;
    }

    QHash<QString, double> scales { { QStringLiteral("DP-1"), 1.0 } };
    QList<ScreenMode> screenModes { mode(1920, 1080, 60000, true), mode(1920, 1080, 59940),
        mode(1920, 1080, 144000), mode(1024, 768, 75000), mode(1024, 768, 60000),
        mode(800, 600, 60000) };
    ScreenMode current = mode(1920, 1080, 60000, true);
    QList<QPair<QString, double>> requests; // scale requests
    QStringList modeRequests;
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

    void systemLogoSetting()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        DisplayPropertiesDialog dialog(&settings);
        QVERIFY(dialog.systemLogoBox()->isChecked());
        QVERIFY(!dialog.applyButton()->isEnabled());
        dialog.systemLogoBox()->setChecked(false);
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.apply();
        QCOMPARE(settings.systemStartLogo(), false);
        dialog.prepare();
        QVERIFY(!dialog.systemLogoBox()->isChecked());
    }

    void fontSizeChoices()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.prepare();
        QVERIFY(dialog.fontSizeBox()->isEnabled());
        QCOMPARE(dialog.fontSizeBox()->count(), 2);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Small Fonts"));
        QCOMPARE(dialog.selectedScale(), 1.0);
        QVERIFY(!dialog.applyButton()->isEnabled());

        // Large Fonts is 150% (NT's 125% is a custom size).
        dialog.selectScale(1.5);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Large Fonts"));
        QVERIFY(dialog.applyButton()->isEnabled());
        // A custom size gets one entry of its own, reused for the next one.
        dialog.selectScale(1.25);
        QCOMPARE(dialog.fontSizeBox()->currentText(), QStringLiteral("Other (125%)"));
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
        dialog.setScreenSettings(&scaler);
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
        dialog.setScreenSettings(&scaler);
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
        dialog.setScreenSettings(&scaler);
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
        QCOMPARE(
            boxes.first()->text(), QStringLiteral("The display settings could not be changed."));
        boxes.first()->close();
    }

    void fontSizeUnavailableWithoutCompositorSupport()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("UNKNOWN-1")); // not reported
        dialog.prepare();
        QVERIFY(!dialog.fontSizeBox()->isEnabled());
        QVERIFY(!dialog.customFontSizeButton()->isEnabled());
        QVERIFY(!dialog.desktopAreaSlider()->isEnabled());
        QVERIFY(!dialog.refreshBox()->isEnabled());
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("Not available"));

        DisplayPropertiesDialog noScaler(&settings);
        QVERIFY(!noScaler.fontSizeBox()->isEnabled());
        noScaler.selectScale(1.25);
        noScaler.apply(); // nothing to apply it with; must not crash
    }

    void desktopArea()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.prepare();
        // Less to more, one notch per size; the current one selected.
        QVERIFY(dialog.desktopAreaSlider()->isEnabled());
        QCOMPARE(dialog.desktopAreaSlider()->maximum(), 2);
        QCOMPARE(dialog.desktopAreaSlider()->value(), 2);
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("1920 by 1080 pixels"));
        // 59.94 and 60 Hz would both read "60", so they keep their decimals.
        QStringList rates;
        for (int i = 0; i < dialog.refreshBox()->count(); ++i)
            rates << dialog.refreshBox()->itemText(i);
        QCOMPARE(rates,
            QStringList({ QStringLiteral("59.94 Hertz"), QStringLiteral("60.00 Hertz"),
                QStringLiteral("144 Hertz") }));
        QCOMPARE(dialog.refreshBox()->currentText(), QStringLiteral("60.00 Hertz"));
        QVERIFY(!dialog.applyButton()->isEnabled());

        // A smaller size: its own rates, keeping 60 Hz.
        dialog.desktopAreaSlider()->setValue(1);
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("1024 by 768 pixels"));
        QCOMPARE(dialog.refreshBox()->count(), 2);
        QCOMPARE(dialog.refreshBox()->currentText(), QStringLiteral("60 Hertz"));
        QVERIFY(dialog.applyButton()->isEnabled());
        dialog.desktopAreaSlider()->setValue(0);
        QCOMPARE(modeKey(dialog.selectedScreenMode()), QStringLiteral("800x600@60000"));
        // Back up: the size's preferred mode wins over a rate it lacks.
        dialog.refreshBox()->setCurrentIndex(0);
        dialog.desktopAreaSlider()->setValue(2);
        QCOMPARE(modeKey(dialog.selectedScreenMode()), QStringLiteral("1920x1080@60000"));
    }

    void modeKeptWhenConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        int asked = 0;
        dialog.setScaleConfirmation([&asked](const std::function<void(bool)> &done) {
            ++asked;
            done(true);
        });
        dialog.prepare();

        // Mode and font size together: one change, one question.
        dialog.desktopAreaSlider()->setValue(1);
        dialog.refreshBox()->setCurrentIndex(dialog.refreshBox()->findData(75000));
        dialog.selectScale(1.5);
        dialog.apply();
        QCOMPARE(scaler.modeRequests, QStringList({ QStringLiteral("1024x768@75000") }));
        QCOMPARE(scaler.requests.size(), 1);
        QCOMPARE(asked, 1);
        QCOMPARE(modeKey(settings.outputMode(QStringLiteral("Maker Model DP-1"))),
            QStringLiteral("1024x768@75000"));
        QCOMPARE(settings.outputScale(QStringLiteral("Maker Model DP-1")), 1.5);

        // Only what changed is sent and saved.
        dialog.refreshBox()->setCurrentIndex(dialog.refreshBox()->findData(60000));
        dialog.apply();
        QCOMPARE(scaler.modeRequests.size(), 2);
        QCOMPARE(scaler.requests.size(), 1);
        QCOMPARE(modeKey(settings.outputMode(QStringLiteral("Maker Model DP-1"))),
            QStringLiteral("1024x768@60000"));
    }

    void modeRevertsWhenNotConfirmed()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(QStringLiteral("daytona.ini")));
        FakeScaler scaler;
        DisplayPropertiesDialog dialog(&settings);
        dialog.setScreenSettings(&scaler);
        dialog.setScreenName(QStringLiteral("DP-1"));
        dialog.setScaleConfirmation([](const std::function<void(bool)> &done) { done(false); });
        dialog.prepare();

        dialog.desktopAreaSlider()->setValue(0);
        dialog.apply();
        QCOMPARE(scaler.modeRequests,
            QStringList({ QStringLiteral("800x600@60000"), QStringLiteral("1920x1080@60000") }));
        QCOMPARE(scaler.requests.size(), 0); // the font size was not touched
        QCOMPARE(modeKey(scaler.current), QStringLiteral("1920x1080@60000"));
        QCOMPARE(dialog.desktopAreaLabel()->text(), QStringLiteral("1920 by 1080 pixels"));
        QVERIFY(!settings.outputMode(QStringLiteral("Maker Model DP-1")).isValid());
        QVERIFY(!dialog.applyButton()->isEnabled());
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
