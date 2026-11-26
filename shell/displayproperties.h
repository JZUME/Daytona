// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"
#include "screenmode.h"
#include "settings.h"

#include <QDialog>
#include <QImage>
#include <QList>
#include <QPoint>
#include <functional>
#include <utility>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;

namespace Daytona {

class MonitorArrangement;
class ScreenSettings;

// Image files directly in each directory (no subdirectories), sorted by
// name within each, without duplicates. At most limit files.
QStringList wallpaperCandidates(const QStringList &directories, int limit = 500);

// Where Display Properties looks for wallpapers: Daytona's own folder
// (where the theme tool copies them) and the system's backgrounds and
// wallpapers folders.
QStringList wallpaperDirectories();

// The Background tab's picture: NT's monitor with the desktop colour and
// the wallpaper, scaled down with the screen.
class DesktopPreview : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopPreview(QWidget *parent = nullptr);
    void setDesktop(
        const QColor &colour, const QImage &wallpaper, WallpaperMode mode, const QSize &screen);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_colour;
    QImage m_wallpaper;
    WallpaperMode m_mode = WallpaperMode::Centre;
    QSize m_screen { 1024, 768 };
};

// The Appearance tab's picture, as NT drew it: an inactive and an active
// window, a menu bar with normal, disabled and selected items, window text
// and a message box, all in the candidate scheme's colours.
class AppearancePreview : public QWidget
{
    Q_OBJECT

public:
    explicit AppearancePreview(QWidget *parent = nullptr);
    void setColours(const SchemeColours &colours);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    SchemeColours m_colours;
};

// NT's Custom Font Size dialog: the scale as a percentage of normal size,
// with a sample.
class CustomFontSizeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomFontSizeDialog(QWidget *parent = nullptr);
    void setPercent(int percent);
    int percent() const;
    QComboBox *percentBox() const { return m_percent; }

private:
    void updateSample();

    QComboBox *m_percent;
    QLabel *m_sample;
};

// NT's Display Properties, with the Background, Screen Saver, Appearance and
// Settings tabs. The Screen Saver tab offers (None) and Blank Screen, the
// wait and password protection. The Settings tab has NT's Desktop Area,
// Refresh Frequency and Font Size: Small Fonts (100%), Large Fonts (150%;
// NT's was 125%) or a custom size, applied to a screen and remembered for
// it. With more than one screen it also has Windows 2000's monitor diagram,
// Display list and the primary monitor and Extend check boxes.
class DisplayPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisplayPropertiesDialog(Settings *settings, QWidget *parent = nullptr);

    // Reads the current settings into both tabs.
    void prepare();

    // The theme tool run on Apply; the one next to the shell by default.
    void setThemeTool(const QString &program) { m_themeTool = program; }

    // What changes screen modes and scales; without one the Settings tab
    // cannot change anything.
    void setScreenSettings(ScreenSettings *screens) { m_screens = screens; }
    // Asks whether to keep new display settings and calls done(keep). By
    // default a message box that reverts after 15 seconds.
    using Confirmation = std::function<void(const std::function<void(bool)> &done)>;
    void setScaleConfirmation(Confirmation confirm) { m_confirm = std::move(confirm); }
    // The screen the Settings tab shows first; the dialog's own by default.
    void setScreenName(const QString &name) { m_screenName = name; }

    QListWidget *wallpaperList() const { return m_wallpapers; }
    QComboBox *fontSizeBox() const { return m_fontSize; }
    QPushButton *customFontSizeButton() const { return m_customFontSize; }
    QSlider *desktopAreaSlider() const { return m_desktopArea; }
    QLabel *desktopAreaLabel() const { return m_areaLabel; }
    QComboBox *refreshBox() const { return m_refresh; }
    // With more than one screen: the diagram, the Display list (one entry
    // per screen, in the diagram's order) and the check boxes.
    MonitorArrangement *monitorArrangement() const { return m_arrangement; }
    QComboBox *displayBox() const { return m_display; }
    QCheckBox *primaryBox() const { return m_primaryBox; }
    QCheckBox *extendBox() const { return m_extend; }
    // The mode the Desktop Area and Refresh Frequency choose.
    ScreenMode selectedScreenMode() const;
    void selectScreenMode(const ScreenMode &mode);
    double selectedScale() const;
    // Selects scale in the font size list, adding a custom entry if needed.
    void selectScale(double scale);
    QComboBox *screenSaverBox() const { return m_screenSaver; }
    QSpinBox *screenSaverWaitBox() const { return m_wait; }
    QCheckBox *screenSaverPasswordBox() const { return m_password; }
    QPushButton *screenSaverPreviewButton() const { return m_previewSaver; }
    QComboBox *schemeBox() const { return m_schemes; }
    QCheckBox *systemLogoBox() const { return m_systemLogo; }
    QPushButton *applyButton() const { return m_apply; }
    QString selectedWallpaper() const;
    WallpaperMode selectedMode() const;
    SchemeColours candidateColours() const;

public Q_SLOTS:
    void accept() override;
    void apply();
    // Adds a .theme file to the schemes and selects it (as "Load .theme
    // file..." does after choosing a file).
    void loadThemeFile(const QString &path);

Q_SIGNALS:
    // The screen saver's Preview button was pressed.
    void screenSaverPreviewRequested();
    // The theme tool finished applying a scheme (ok false if it failed).
    void schemeApplied(bool ok);

private:
    QWidget *createBackgroundTab();
    QWidget *createScreenSaverTab();
    void updateScreenSaverControls();
    QWidget *createAppearanceTab();
    QWidget *createSettingsTab();
    QString screenName() const;
    // A screen as the Settings tab shows it: as chosen, and as last applied.
    struct Monitor
    {
        QString name;
        QString id; // ScreenSettings::identity
        bool canChange = false;
        QList<ScreenMode> modes;
        ScreenMode mode;
        ScreenMode appliedMode;
        double scale = 1.0;
        double appliedScale = 1.0;
        QPoint position;
        QPoint appliedPosition;
        bool enabled = true;
        bool appliedEnabled = true;
    };
    void prepareMonitors();
    void selectMonitor(int index);
    void showMonitor();
    void screenControlChanged();
    void setExtended(bool extend);
    void setPrimary();
    void monitorMoved(int index, const QPoint &position);
    QSize monitorSize(const Monitor &monitor) const;
    void normaliseMonitors();
    void updateArrangement();
    void applyScreen();
    void confirmSettings(const std::function<void(bool)> &done);
    void updateRefreshList(int keepRefresh);
    void selectWallpaper(const QString &path);
    void browse();
    void updateBackgroundPreview();
    void updateAppearancePreview();
    void changed();

    Settings *m_settings;
    QString m_themeTool;
    DesktopPreview *m_desktopPreview;
    QListWidget *m_wallpapers;
    // The Display options, in the order shown.
    QList<std::pair<WallpaperMode, QRadioButton *>> m_displayModes;
    DesktopPreview *m_saverPreview;
    QComboBox *m_screenSaver;
    QPushButton *m_saverSettings;
    QPushButton *m_previewSaver;
    QCheckBox *m_password;
    QSpinBox *m_wait;
    AppearancePreview *m_appearancePreview;
    QComboBox *m_schemes;
    QCheckBox *m_systemLogo;
    QPushButton *m_apply;
    ScreenSettings *m_screens = nullptr;
    Confirmation m_confirm;
    QString m_screenName;
    QLabel *m_screenLabel;
    QWidget *m_arrangementBox;
    MonitorArrangement *m_arrangement;
    QComboBox *m_display;
    QCheckBox *m_primaryBox;
    QCheckBox *m_extend;
    QList<Monitor> m_monitors;
    int m_monitor = 0; // the one the controls show
    QString m_primary; // identity
    QString m_appliedPrimary;
    QComboBox *m_fontSize;
    QPushButton *m_customFontSize;
    QSlider *m_desktopArea;
    QLabel *m_areaLabel;
    QComboBox *m_refresh;
    QList<ScreenMode> m_modes;
    QList<QSize> m_sizes;
    QString m_appliedScheme; // what the combo showed when last prepared or applied
    bool m_preparing = false;
};

} // namespace Daytona
