// SPDX-License-Identifier: GPL-3.0-or-later

#include "soundtheme.h"

#include "gtktheme.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <algorithm>

namespace Daytona {

namespace {

    // The extensions the specification allows, in its order of preference.
    const char *const Extensions[] = { "disabled", "oga", "ogg", "wav" };

    // A theme's index.theme, or empty if dir does not hold the theme.
    QString indexFile(const QStringList &soundDirs, const QString &theme)
    {
        if (theme.isEmpty() || theme.contains(QLatin1Char('/'))
            || theme.startsWith(QLatin1Char('.')))
            return {};
        for (const QString &dir : soundDirs) {
            const QString index = QDir(dir).filePath(theme + QStringLiteral("/index.theme"));
            if (QFileInfo(index).isFile())
                return index;
        }
        return {};
    }

    struct ThemeIndex
    {
        QStringList directories; // absolute paths
        QStringList inherits;
    };

    ThemeIndex readIndex(const QStringList &soundDirs, const QString &theme)
    {
        ThemeIndex result;
        const QString index = indexFile(soundDirs, theme);
        if (index.isEmpty())
            return result;
        QSettings ini(index, QSettings::IniFormat);
        ini.beginGroup(QStringLiteral("Sound Theme"));
        const auto list = [&ini](const char *key) {
            // QSettings splits on commas itself; the files use them too.
            QStringList values = ini.value(QLatin1String(key)).toStringList();
            if (values.size() == 1)
                values = values.first().split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (QString &value : values)
                value = value.trimmed();
            values.removeAll(QString());
            return values;
        };
        const QStringList subdirs = list("Directories");
        result.inherits = list("Inherits");
        ini.endGroup();
        // Every copy of the theme counts, so a user can add to a system theme.
        for (const QString &dir : soundDirs) {
            const QDir themeDir(QDir(dir).filePath(theme));
            if (!QFileInfo(themeDir.filePath(QStringLiteral("index.theme"))).isFile())
                continue;
            for (const QString &subdir : subdirs) {
                if (subdir.contains(QLatin1String("..")))
                    continue;
                result.directories << themeDir.filePath(subdir);
            }
        }
        return result;
    }

    enum class Lookup
    {
        Found,
        Disabled,
        Missing
    };

    // name in one theme only, without its parents.
    Lookup lookupInTheme(const ThemeIndex &index, const QString &name, QString *file)
    {
        for (QString variant = name; !variant.isEmpty();) {
            for (const QString &dir : index.directories) {
                for (const char *extension : Extensions) {
                    const QString path
                        = QDir(dir).filePath(variant + QLatin1Char('.') + QLatin1String(extension));
                    if (!QFileInfo(path).isFile())
                        continue;
                    if (qstrcmp(extension, "disabled") == 0)
                        return Lookup::Disabled;
                    *file = path;
                    return Lookup::Found;
                }
            }
            const qsizetype dash = variant.lastIndexOf(QLatin1Char('-'));
            variant = dash > 0 ? variant.left(dash) : QString();
        }
        return Lookup::Missing;
    }

    Lookup lookup(const QStringList &soundDirs, const QString &theme, const QString &name,
        QSet<QString> *seen, QString *file)
    {
        if (seen->contains(theme) || seen->size() > 32)
            return Lookup::Missing;
        seen->insert(theme);
        const ThemeIndex index = readIndex(soundDirs, theme);
        const Lookup found = lookupInTheme(index, name, file);
        if (found != Lookup::Missing)
            return found;
        for (const QString &parent : index.inherits) {
            const Lookup inherited = lookup(soundDirs, parent, name, seen, file);
            if (inherited != Lookup::Missing)
                return inherited;
        }
        return Lookup::Missing;
    }

} // namespace

const QList<SoundEvent> &soundEvents()
{
    const auto tr = [](const char *text) {
        return QCoreApplication::translate("Daytona::SoundEvents", text);
    };
    using G = SoundEvent::Group;
    // Only the events the shell itself plays.
    static const QList<SoundEvent> events = {
        { QStringLiteral("battery-low"), tr("The battery is very low."), G::Alerts,
            { QStringLiteral("dialog-error") } },
        { QStringLiteral("dialog-error"), tr("An error message is shown."), G::Alerts, {} },
        { QStringLiteral("battery-caution"), tr("The battery is low."), G::Notifications,
            { QStringLiteral("dialog-warning") } },
        { QStringLiteral("bell"), tr("A message without an icon is shown."), G::Notifications, {} },
        { QStringLiteral("desktop-login"), tr("You log on."), G::Notifications,
            { QStringLiteral("service-login") } },
        { QStringLiteral("desktop-logout"), tr("You log off, shut down or restart."),
            G::Notifications, { QStringLiteral("service-logout") } },
        { QStringLiteral("dialog-information"), tr("An information message is shown."),
            G::Notifications, {} },
        { QStringLiteral("dialog-question"), tr("A question is asked."), G::Notifications,
            { QStringLiteral("window-question") } },
        { QStringLiteral("dialog-warning"), tr("A warning message is shown."), G::Notifications,
            {} },
        { QStringLiteral("message-new-instant"),
            tr("A program shows a notification balloon without a sound of its own."),
            G::Notifications, {} },
    };
    return events;
}

const SoundEvent *soundEvent(const QString &name)
{
    for (const SoundEvent &event : soundEvents()) {
        if (event.name == name)
            return &event;
    }
    return nullptr;
}

QStringList soundThemeDirs()
{
    QString home = qEnvironmentVariable("XDG_DATA_HOME");
    if (home.isEmpty())
        home = QDir::homePath() + QStringLiteral("/.local/share");
    QString system = qEnvironmentVariable("XDG_DATA_DIRS");
    if (system.isEmpty())
        system = QStringLiteral("/usr/local/share:/usr/share");
    QStringList dirs { home + QStringLiteral("/sounds") };
    for (const QString &dir : system.split(QLatin1Char(':'), Qt::SkipEmptyParts))
        dirs << QDir(dir).filePath(QStringLiteral("sounds"));
    dirs.removeDuplicates();
    return dirs;
}

QList<SoundThemeInfo> installedSoundThemes(const QStringList &soundDirs)
{
    QList<SoundThemeInfo> themes;
    QSet<QString> seen;
    for (const QString &dir : soundDirs) {
        const QStringList entries = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &id : entries) {
            const QString index = QDir(dir).filePath(id + QStringLiteral("/index.theme"));
            if (seen.contains(id) || !QFileInfo(index).isFile())
                continue;
            seen.insert(id);
            QSettings ini(index, QSettings::IniFormat);
            ini.beginGroup(QStringLiteral("Sound Theme"));
            if (ini.value(QStringLiteral("Hidden")).toString() == QLatin1String("true"))
                continue;
            QString name = ini.value(QStringLiteral("Name")).toString().trimmed();
            if (name.isEmpty())
                name = id;
            themes.append({ id, name.left(128) });
        }
    }
    std::sort(themes.begin(), themes.end(), [](const SoundThemeInfo &a, const SoundThemeInfo &b) {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    });
    return themes;
}

QString findSoundFile(const QStringList &soundDirs, const QString &theme, const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')))
        return {};
    QSet<QString> seen;
    QString file;
    Lookup found = lookup(soundDirs, theme, name, &seen, &file);
    if (found == Lookup::Missing)
        found
            = lookup(soundDirs, QString::fromLatin1(SoundScheme::DefaultTheme), name, &seen, &file);
    return found == Lookup::Found ? file : QString();
}

QString findEventSoundFile(
    const QStringList &soundDirs, const QString &theme, const SoundEvent &event)
{
    for (const QString &name : QStringList { event.name } + event.fallbacks) {
        const QString file = findSoundFile(soundDirs, theme, name);
        if (!file.isEmpty())
            return file;
    }
    return {};
}

QStringList soundThemeFiles(const QStringList &soundDirs, const QString &theme)
{
    QStringList files;
    QSet<QString> names;
    for (const QString &dir : readIndex(soundDirs, theme).directories) {
        const QFileInfoList entries = QDir(dir).entryInfoList(
            { QStringLiteral("*.oga"), QStringLiteral("*.ogg"), QStringLiteral("*.wav") },
            QDir::Files, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (names.contains(entry.completeBaseName()))
                continue;
            names.insert(entry.completeBaseName());
            files << entry.absoluteFilePath();
        }
    }
    std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
        return QFileInfo(a).completeBaseName() < QFileInfo(b).completeBaseName();
    });
    return files;
}

GtkSoundSettings readGtkSoundSettings(const QString &path)
{
    GtkSoundSettings result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        return result;
    bool inSettings = false;
    for (const QByteArray &raw : file.readAll().split('\n')) {
        const QByteArray line = raw.trimmed();
        if (line.startsWith('[')) {
            inSettings = line == "[Settings]";
            continue;
        }
        const qsizetype eq = line.indexOf('=');
        if (!inSettings || eq <= 0)
            continue;
        const QByteArray key = line.left(eq).trimmed();
        QByteArray value = line.mid(eq + 1).trimmed();
        if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
            value = value.mid(1, value.size() - 2);
        if (key == "gtk-sound-theme-name" && !value.isEmpty()) {
            result.theme = QString::fromUtf8(value);
        } else if (key == "gtk-enable-event-sounds") {
            const QByteArray lower = value.toLower();
            result.enabled = !(lower == "0" || lower == "false");
        }
    }
    return result;
}

bool writeGtkSoundSettings(const QString &path, const SoundScheme &scheme, QString *error)
{
    return setGtkSettingValues(path,
        { { "gtk-sound-theme-name", scheme.theme.toUtf8() },
            { "gtk-enable-event-sounds", scheme.enabled ? "1" : "0" } },
        error);
}

} // namespace Daytona
