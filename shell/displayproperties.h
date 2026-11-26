// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"
#include "settings.h"

#include <QDialog>
#include <QImage>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;

namespace Daytona {

class ScreenScaler;

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

// NT's Display Properties, with the Background, Appearance and Settings
// tabs. The Settings tab has NT's Font Size: Small Fonts (100%), Large
// Fonts (150%; NT's was 125%) or a custom size, applied as the scale of the screen the
// dialog is on and remembered for that screen.
class DisplayPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisplayPropertiesDialog(Settings *settings, QWidget *parent = nullptr);

    // Reads the current settings into both tabs.
    void prepare();

    // The theme tool run on Apply; the one next to the shell by default.
    void setThemeTool(const QString &program) { m_themeTool = program; }

    // What changes screen scales; without one the font size cannot be
    // changed.
    void setScreenScaler(ScreenScaler *scaler) { m_scaler = scaler; }
    // Asks whether to keep a new font size and calls done(keep). By default
    // a message box that reverts after 15 seconds.
    using Confirmation = std::function<void(const std::function<void(bool)> &done)>;
    void setScaleConfirmation(Confirmation confirm) { m_confirm = std::move(confirm); }
    // The screen the Settings tab changes; the dialog's own by default.
    void setScreenName(const QString &name) { m_screenName = name; }

    QListWidget *wallpaperList() const { return m_wallpapers; }
    QComboBox *fontSizeBox() const { return m_fontSize; }
    QPushButton *customFontSizeButton() const { return m_customFontSize; }
    double selectedScale() const;
    // Selects scale in the font size list, adding a custom entry if needed.
    void selectScale(double scale);
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
    // The theme tool finished applying a scheme (ok false if it failed).
    void schemeApplied(bool ok);

private:
    QWidget *createBackgroundTab();
    QWidget *createAppearanceTab();
    QWidget *createSettingsTab();
    QString screenName() const;
    void applyScale();
    void confirmScale(const std::function<void(bool)> &done);
    void selectWallpaper(const QString &path);
    void browse();
    void updateBackgroundPreview();
    void updateAppearancePreview();
    void changed();

    Settings *m_settings;
    QString m_themeTool;
    DesktopPreview *m_desktopPreview;
    QListWidget *m_wallpapers;
    QRadioButton *m_tile;
    QRadioButton *m_centre;
    AppearancePreview *m_appearancePreview;
    QComboBox *m_schemes;
    QCheckBox *m_systemLogo;
    QPushButton *m_apply;
    ScreenScaler *m_scaler = nullptr;
    Confirmation m_confirm;
    QString m_screenName;
    QLabel *m_screenLabel;
    QComboBox *m_fontSize;
    QPushButton *m_customFontSize;
    double m_appliedScale = 1.0;
    QString m_appliedScheme; // what the combo showed when last prepared or applied
    bool m_preparing = false;
};

} // namespace Daytona
