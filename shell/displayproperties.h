// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"
#include "settings.h"

#include <QDialog>
#include <QImage>

class QComboBox;
class QListWidget;
class QPushButton;
class QRadioButton;

namespace Daytona {

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

// NT's Display Properties, with the Background and Appearance tabs.
class DisplayPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisplayPropertiesDialog(Settings *settings, QWidget *parent = nullptr);

    // Reads the current settings into both tabs.
    void prepare();

    // The theme tool run on Apply; the one next to the shell by default.
    void setThemeTool(const QString &program) { m_themeTool = program; }

    QListWidget *wallpaperList() const { return m_wallpapers; }
    QComboBox *schemeBox() const { return m_schemes; }
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
    QPushButton *m_apply;
    QString m_appliedScheme; // what the combo showed when last prepared or applied
    bool m_preparing = false;
};

} // namespace Daytona
