// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QSettings>

class QFileSystemWatcher;
class QTimer;
#include <QString>
#include <QStringList>

namespace Daytona {

enum class TaskbarEdge
{
    Bottom,
    Top,
    Left,
    Right
};

// NT 4.0's Display Properties offered Tile and Center only.
enum class WallpaperMode
{
    Centre,
    Tile
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

    QString controlPanelCommand() const;
    void setControlPanelCommand(const QString &command);
    QString findCommand() const;
    void setFindCommand(const QString &command);

    // Most recent first, at most MaxRunHistory entries, no duplicates.
    QStringList runHistory() const;
    void addToRunHistory(const QString &command);

    QString wallpaperPath() const;
    void setWallpaperPath(const QString &path);
    WallpaperMode wallpaperMode() const;
    void setWallpaperMode(WallpaperMode mode);
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
