// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keyboardlayout.h"
#include "screenmode.h"
#include "xcursor.h"

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <optional>

class QFileSystemWatcher;
class QTimer;

namespace Daytona {

enum class TaskbarEdge
{
    Bottom,
    Top,
    Left,
    Right
};

// NT 4.0's Display Properties offered Tile and Center. Stretch (Windows 98),
// Fit and Fill (Windows 7) are small modern additions (AGENT.md, Design Scope).
enum class ScreenSaver
{
    None,
    Blank
};

enum class WallpaperMode
{
    Centre,
    Tile,
    Stretch,
    Fit,
    Fill
};

// Every user setting the shell has, stored in one INI file.
//
// Only settings listed in AGENT.md (Configuration Policy) belong here. Values
// that are missing or invalid in the file fall back to their defaults.
// Reading never creates or rewrites the file; only setters write.
class Settings : public QObject
{
    Q_OBJECT

public:
    explicit Settings(const QString &filePath, QObject *parent = nullptr);

    // $XDG_CONFIG_HOME/daytona/daytona.ini
    static QString defaultFilePath();

    static constexpr int MaxRunHistory = 26;

    TaskbarEdge taskbarEdge() const;
    void setTaskbarEdge(TaskbarEdge edge);
    int taskbarRows() const;
    void setTaskbarRows(int rows);
    // Width of a vertical taskbar, in logical pixels; 0 for the default.
    int taskbarWidth() const;
    void setTaskbarWidth(int width);
    bool taskbarLocked() const;
    void setTaskbarLocked(bool locked);
    bool taskbarAlwaysOnTop() const;
    void setTaskbarAlwaysOnTop(bool onTop);
    bool taskbarAutoHide() const;
    void setTaskbarAutoHide(bool autoHide);
    bool showClock() const;
    void setShowClock(bool show);
    bool smallStartMenuIcons() const;
    void setSmallStartMenuIcons(bool small);

    QString findCommand() const;
    void setFindCommand(const QString &command);

    // Most recent first, at most MaxRunHistory entries, no duplicates.
    QStringList runHistory() const;
    void addToRunHistory(const QString &command);

    QString wallpaperPath() const;
    void setWallpaperPath(const QString &path);
    WallpaperMode wallpaperMode() const;
    void setWallpaperMode(WallpaperMode mode);
    // Mouse Properties' pointer scheme. daytona-session sets the cursor
    // size from it at log on (large: 64, extralarge: 96, else 32).
    PointerScheme pointerScheme() const;
    void setPointerScheme(PointerScheme scheme);

    // Keyboard Properties' input locales, as InputLocale codes ("gb",
    // "de(nodeadkeys)"), the default first; empty when none was chosen, and
    // the system's keyboard is used. daytona-session passes them to labwc
    // at log on (daytona-shell --labwc-environment).
    QStringList keyboardLayouts() const;
    void setKeyboardLayouts(const QStringList &codes);
    // The hot key that switches between them.
    LayoutHotkey layoutHotkey() const;
    void setLayoutHotkey(LayoutHotkey hotkey);

    // Display Properties' Screen Saver tab: the screen saver, its wait in
    // minutes (MinScreenSaverWait to MaxScreenSaverWait, as NT's spin box)
    // and whether ending it asks for the password.
    static constexpr int MinScreenSaverWait = 1;
    static constexpr int MaxScreenSaverWait = 60;
    ScreenSaver screenSaver() const;
    void setScreenSaver(ScreenSaver saver);
    int screenSaverWait() const;
    void setScreenSaverWait(int minutes);
    bool screenSaverPassword() const;
    void setScreenSaverPassword(bool protect);

    // Whether the Start button shows the operating system's logo (true) or
    // Daytona's own.
    bool systemStartLogo() const;
    void setSystemStartLogo(bool use);
    QString colourScheme() const;
    void setColourScheme(const QString &name);
    // .theme files loaded in Display Properties, listed with the schemes.
    QStringList themeFiles() const;
    void addThemeFile(const QString &path);

    // A screen's scale (NT's Small Fonts, Large Fonts or a custom size),
    // filed under ScreenScaler::identity(); 0 when none is saved.
    static constexpr double MinScreenScale = 1.0;
    static constexpr double MaxScreenScale = 3.0;
    double outputScale(const QString &screen) const;
    void setOutputScale(const QString &screen, double scale);
    // A screen's mode (NT's Desktop Area and Refresh Frequency), filed the
    // same way; invalid when none is saved.
    ScreenMode outputMode(const QString &screen) const;
    void setOutputMode(const QString &screen, const ScreenMode &mode);
    // Where a screen sits on the desktop (its top left, in logical pixels),
    // filed the same way; nothing when none is saved.
    std::optional<QPoint> outputPosition(const QString &screen) const;
    void setOutputPosition(const QString &screen, const QPoint &position);
    // Whether a screen is on (Windows 2000's "Extend my desktop onto this
    // monitor"); true unless it was switched off.
    bool outputEnabled(const QString &screen) const;
    void setOutputEnabled(const QString &screen, bool enabled);
    // The screen that holds the taskbar (Windows 2000's primary monitor),
    // filed the same way; empty to leave it to the compositor's first.
    QString primaryOutput() const;
    void setPrimaryOutput(const QString &screen);

    // Where each desktop icon was put, in logical pixels from the top left of
    // the desktop, by name: a file name in the Desktop folder, or
    // DesktopItem::key() for My Computer and the other system icons.
    QHash<QString, QPoint> desktopIconPositions() const;
    void setDesktopIconPositions(const QHash<QString, QPoint> &positions);
    // The desktop menu's Arrange Icons > Auto Arrange.
    bool desktopAutoArrange() const;
    void setDesktopAutoArrange(bool autoArrange);

    // Follows changes other programs make to the file, such as
    // daytona-themetool recording a new wallpaper.
    void watchFile();

Q_SIGNALS:
    // Emitted after a setter changes a value. key is the INI key, e.g.
    // "Taskbar/Edge", or empty when the file was changed by another program.
    void changed(const QString &key);

private:
    bool readBool(const QString &key, bool fallback) const;
    void write(const QString &key, const QVariant &value);

    mutable QSettings m_settings;
    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_reload = nullptr;
};

} // namespace Daytona
