// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

namespace Daytona {

// The event sounds the shell plays, named by the freedesktop Sound Naming
// Specification, so that one sound theme serves Daytona and every other
// program. Grouped as the specification groups them.
struct SoundEvent
{
    enum class Group
    {
        Alerts,
        Notifications
    };

    QString name; // the specification's name, e.g. "dialog-warning"
    QString description; // what it is for, from the specification
    Group group = Group::Notifications;
    // Names to try when the theme has no sound for name: the freedesktop
    // theme, for one, has no desktop-login, which plays service-login.
    QStringList fallbacks;
};

const QList<SoundEvent> &soundEvents();
// The event named name, or nullptr.
const SoundEvent *soundEvent(const QString &name);

// An installed sound theme (a directory under sounds/ with an index.theme).
struct SoundThemeInfo
{
    QString id; // the directory name, e.g. "freedesktop"
    QString name; // its Name, e.g. "Default"
};

// The sound theme directories, most important first:
// $XDG_DATA_HOME/sounds, then sounds/ in each of $XDG_DATA_DIRS.
QStringList soundThemeDirs();

// The themes in soundDirs, by name. A theme found in more than one directory
// is listed once, from the first; Hidden themes are left out.
QList<SoundThemeInfo> installedSoundThemes(const QStringList &soundDirs);

// The file theme plays for name, following the Sound Theme Specification:
// the theme's directories, then the themes it inherits, then "freedesktop";
// for each theme the name itself, then with its last "-part" removed
// ("dialog-warning-auth", then "dialog-warning", then "dialog"). Empty if no
// theme has it, or a theme turns it off with a .disabled file.
QString findSoundFile(const QStringList &soundDirs, const QString &theme, const QString &name);
// The same for an event, trying its fallbacks too.
QString findEventSoundFile(
    const QStringList &soundDirs, const QString &theme, const SoundEvent &event);

// Every sound file in theme itself (not the themes it inherits), by name.
QStringList soundThemeFiles(const QStringList &soundDirs, const QString &theme);

// The sound theme setting other programs read: GTK's gtk-sound-theme-name
// and gtk-enable-event-sounds (in settings.ini, and in GSettings as
// org.gnome.desktop.sound theme-name and event-sounds). "No Sounds" is
// event sounds turned off.
struct SoundScheme
{
    static constexpr const char *DefaultTheme = "freedesktop";

    QString theme = QString::fromLatin1(DefaultTheme);
    bool enabled = true;

    bool operator==(const SoundScheme &other) const
    {
        return theme == other.theme && enabled == other.enabled;
    }
};

// What a GTK settings.ini says, each part only if it is set there.
struct GtkSoundSettings
{
    std::optional<QString> theme;
    std::optional<bool> enabled;
};
GtkSoundSettings readGtkSoundSettings(const QString &path);
bool writeGtkSoundSettings(
    const QString &path, const SoundScheme &scheme, QString *error = nullptr);

} // namespace Daytona
