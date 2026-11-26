// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>

Q_LOGGING_CATEGORY(lcSettings, "daytona.settings", QtInfoMsg)

namespace Daytona {

namespace {

    const QString KeyEdge = QStringLiteral("Taskbar/Edge");
    const QString KeyRows = QStringLiteral("Taskbar/Rows");
    const QString KeyWidth = QStringLiteral("Taskbar/Width");
    const QString KeyLocked = QStringLiteral("Taskbar/Locked");
    const QString KeyOnTop = QStringLiteral("Taskbar/AlwaysOnTop");
    const QString KeyAutoHide = QStringLiteral("Taskbar/AutoHide");
    const QString KeyShowClock = QStringLiteral("Taskbar/ShowClock");
    const QString KeySmallIcons = QStringLiteral("Taskbar/SmallStartMenuIcons");
    const QString KeyFind = QStringLiteral("Commands/Find");
    const QString KeyRunHistory = QStringLiteral("Run/History");
    const QString KeyWallpaper = QStringLiteral("Appearance/Wallpaper");
    const QString KeyWallpaperMode = QStringLiteral("Appearance/WallpaperMode");
    const QString KeyScheme = QStringLiteral("Appearance/ColourScheme");
    const QString KeyThemeFiles = QStringLiteral("Appearance/ThemeFiles");
    const QString KeySystemLogo = QStringLiteral("Appearance/SystemStartLogo");
    // Read by daytona-session too, which sets the cursor size from it.
    const QString KeyPointerScheme = QStringLiteral("Mouse/PointerScheme");

    const QString DefaultScheme = QStringLiteral("Windows Standard");

    QString edgeName(TaskbarEdge edge)
    {
        switch (edge) {
        case TaskbarEdge::Bottom:
            return QStringLiteral("bottom");
        case TaskbarEdge::Top:
            return QStringLiteral("top");
        case TaskbarEdge::Left:
            return QStringLiteral("left");
        case TaskbarEdge::Right:
            return QStringLiteral("right");
        }
        return QStringLiteral("bottom");
    }

    QString modeName(WallpaperMode mode)
    {
        switch (mode) {
        case WallpaperMode::Tile:
            return QStringLiteral("tile");
        case WallpaperMode::Stretch:
            return QStringLiteral("stretch");
        case WallpaperMode::Fit:
            return QStringLiteral("fit");
        case WallpaperMode::Fill:
            return QStringLiteral("fill");
        case WallpaperMode::Centre:
            break;
        }
        return QStringLiteral("centre");
    }

} // namespace

Settings::Settings(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_settings(filePath, QSettings::IniFormat)
{
}

void Settings::watchFile()
{
    if (m_watcher)
        return;
    m_watcher = new QFileSystemWatcher(this);
    m_reload = new QTimer(this);
    m_reload->setSingleShot(true);
    // Writers replace the file (QSaveFile); let a burst of changes settle.
    m_reload->setInterval(100);
    const auto rewatch = [this] {
        const QString file = m_settings.fileName();
        if (!m_watcher->files().isEmpty())
            m_watcher->removePaths(m_watcher->files());
        if (QFileInfo::exists(file))
            m_watcher->addPath(file);
        const QString dir = QFileInfo(file).absolutePath();
        if (m_watcher->directories().isEmpty() && QFileInfo(dir).isDir())
            m_watcher->addPath(dir);
    };
    connect(m_reload, &QTimer::timeout, this, [this, rewatch] {
        rewatch();
        m_settings.sync();
        Q_EMIT changed(QString());
    });
    connect(m_watcher, &QFileSystemWatcher::fileChanged, m_reload, qOverload<>(&QTimer::start));
    connect(
        m_watcher, &QFileSystemWatcher::directoryChanged, m_reload, qOverload<>(&QTimer::start));
    rewatch();
}

QString Settings::defaultFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return QDir(base).filePath(QStringLiteral("daytona/daytona.ini"));
}

bool Settings::readBool(const QString &key, bool fallback) const
{
    if (!m_settings.contains(key))
        return fallback;
    const QString text = m_settings.value(key).toString().trimmed().toLower();
    if (text == QLatin1String("true") || text == QLatin1String("1"))
        return true;
    if (text == QLatin1String("false") || text == QLatin1String("0"))
        return false;
    qCWarning(lcSettings) << "Invalid value for" << key << "- using default" << fallback;
    return fallback;
}

void Settings::write(const QString &key, const QVariant &value)
{
    if (m_settings.contains(key) && m_settings.value(key) == value)
        return;
    m_settings.setValue(key, value);
    m_settings.sync();
    if (m_settings.status() != QSettings::NoError)
        qCWarning(lcSettings) << "Could not save" << key << "to" << m_settings.fileName();
    Q_EMIT changed(key);
}

TaskbarEdge Settings::taskbarEdge() const
{
    if (!m_settings.contains(KeyEdge))
        return TaskbarEdge::Bottom;
    const QString text = m_settings.value(KeyEdge).toString().trimmed().toLower();
    for (TaskbarEdge edge :
        { TaskbarEdge::Bottom, TaskbarEdge::Top, TaskbarEdge::Left, TaskbarEdge::Right }) {
        if (text == edgeName(edge))
            return edge;
    }
    qCWarning(lcSettings) << "Invalid value for" << KeyEdge << "- using bottom";
    return TaskbarEdge::Bottom;
}

void Settings::setTaskbarEdge(TaskbarEdge edge)
{
    write(KeyEdge, edgeName(edge));
}

int Settings::taskbarRows() const
{
    if (!m_settings.contains(KeyRows))
        return 1;
    bool ok = false;
    const int rows = m_settings.value(KeyRows).toString().trimmed().toInt(&ok);
    if (!ok || rows < 1) {
        qCWarning(lcSettings) << "Invalid value for" << KeyRows << "- using 1";
        return 1;
    }
    return rows;
}

void Settings::setTaskbarRows(int rows)
{
    write(KeyRows, qMax(1, rows));
}

int Settings::taskbarWidth() const
{
    bool ok = false;
    const int width = m_settings.value(KeyWidth).toString().trimmed().toInt(&ok);
    return ok && width > 0 ? width : 0;
}

void Settings::setTaskbarWidth(int width)
{
    write(KeyWidth, qMax(0, width));
}

bool Settings::taskbarLocked() const
{
    return readBool(KeyLocked, true);
}
void Settings::setTaskbarLocked(bool locked)
{
    write(KeyLocked, locked);
}

bool Settings::taskbarAlwaysOnTop() const
{
    return readBool(KeyOnTop, true);
}
void Settings::setTaskbarAlwaysOnTop(bool onTop)
{
    write(KeyOnTop, onTop);
}

bool Settings::taskbarAutoHide() const
{
    return readBool(KeyAutoHide, false);
}
void Settings::setTaskbarAutoHide(bool autoHide)
{
    write(KeyAutoHide, autoHide);
}

bool Settings::showClock() const
{
    return readBool(KeyShowClock, true);
}
void Settings::setShowClock(bool show)
{
    write(KeyShowClock, show);
}

bool Settings::smallStartMenuIcons() const
{
    return readBool(KeySmallIcons, false);
}
void Settings::setSmallStartMenuIcons(bool small)
{
    write(KeySmallIcons, small);
}

QString Settings::findCommand() const
{
    return m_settings.value(KeyFind).toString().trimmed();
}
void Settings::setFindCommand(const QString &command)
{
    write(KeyFind, command.trimmed());
}

QStringList Settings::runHistory() const
{
    QStringList history;
    const QStringList stored = m_settings.value(KeyRunHistory).toStringList();
    for (const QString &entry : stored) {
        const QString command = entry.trimmed();
        if (!command.isEmpty() && !history.contains(command))
            history.append(command);
        if (history.size() == MaxRunHistory)
            break;
    }
    return history;
}

void Settings::addToRunHistory(const QString &command)
{
    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
        return;
    QStringList history = runHistory();
    history.removeAll(trimmed);
    history.prepend(trimmed);
    while (history.size() > MaxRunHistory)
        history.removeLast();
    write(KeyRunHistory, history);
}

QString Settings::wallpaperPath() const
{
    return m_settings.value(KeyWallpaper).toString();
}
void Settings::setWallpaperPath(const QString &path)
{
    write(KeyWallpaper, path);
}

WallpaperMode Settings::wallpaperMode() const
{
    if (!m_settings.contains(KeyWallpaperMode))
        return WallpaperMode::Centre;
    const QString text = m_settings.value(KeyWallpaperMode).toString().trimmed().toLower();
    for (WallpaperMode mode : { WallpaperMode::Centre, WallpaperMode::Tile, WallpaperMode::Stretch,
             WallpaperMode::Fit, WallpaperMode::Fill }) {
        if (text == modeName(mode))
            return mode;
    }
    qCWarning(lcSettings) << "Invalid value for" << KeyWallpaperMode << "- using centre";
    return WallpaperMode::Centre;
}

void Settings::setWallpaperMode(WallpaperMode mode)
{
    write(KeyWallpaperMode, modeName(mode));
}

PointerScheme Settings::pointerScheme() const
{
    const QString text = m_settings.value(KeyPointerScheme).toString().trimmed().toLower();
    if (text == QLatin1String("large"))
        return PointerScheme::Large;
    if (text == QLatin1String("extralarge"))
        return PointerScheme::ExtraLarge;
    if (!text.isEmpty() && text != QLatin1String("standard"))
        qCWarning(lcSettings) << "Invalid value for" << KeyPointerScheme << "- using standard";
    return PointerScheme::Standard;
}

void Settings::setPointerScheme(PointerScheme scheme)
{
    write(KeyPointerScheme,
        scheme == PointerScheme::Large            ? QStringLiteral("large")
            : scheme == PointerScheme::ExtraLarge ? QStringLiteral("extralarge")
                                                  : QStringLiteral("standard"));
}

bool Settings::systemStartLogo() const
{
    return readBool(KeySystemLogo, true);
}

void Settings::setSystemStartLogo(bool use)
{
    write(KeySystemLogo, use);
}

QString Settings::colourScheme() const
{
    const QString name = m_settings.value(KeyScheme).toString().trimmed();
    return name.isEmpty() ? DefaultScheme : name;
}

void Settings::setColourScheme(const QString &name)
{
    write(KeyScheme, name.trimmed());
}

QStringList Settings::themeFiles() const
{
    QStringList files = m_settings.value(KeyThemeFiles).toStringList();
    files.removeAll(QString());
    return files.mid(0, 50);
}

void Settings::addThemeFile(const QString &path)
{
    QStringList files = themeFiles();
    if (files.contains(path))
        return;
    files << path;
    write(KeyThemeFiles, files.mid(qMax(0, files.size() - 50)));
}

namespace {

    // The key for a screen's scale. '/' and '\\' would start a subgroup in
    // QSettings; QSettings escapes everything else itself.
    // The key for a screen's setting in group. '/' and '\\' would start a
    // subgroup in QSettings; QSettings escapes everything else itself.
    QString screenKey(const QString &group, const QString &screen)
    {
        QString name = screen;
        name.replace(QLatin1Char('/'), QLatin1Char('_'));
        name.replace(QLatin1Char('\\'), QLatin1Char('_'));
        return group + QLatin1Char('/') + name;
    }

    QString scaleKey(const QString &screen)
    {
        return screenKey(QStringLiteral("ScreenScale"), screen);
    }

} // namespace

ScreenMode Settings::outputMode(const QString &screen) const
{
    const QString key = screenKey(QStringLiteral("ScreenMode"), screen);
    if (screen.isEmpty() || !m_settings.contains(key))
        return {};
    const ScreenMode mode = parseModeKey(m_settings.value(key).toString());
    if (!mode.isValid())
        qCWarning(lcSettings) << "Invalid value for" << key << "- ignoring it";
    return mode;
}

void Settings::setOutputMode(const QString &screen, const ScreenMode &mode)
{
    if (screen.isEmpty() || !mode.isValid())
        return;
    write(screenKey(QStringLiteral("ScreenMode"), screen), modeKey(mode));
}

double Settings::outputScale(const QString &screen) const
{
    const QString key = scaleKey(screen);
    if (screen.isEmpty() || !m_settings.contains(key))
        return 0;
    bool ok = false;
    const double scale = m_settings.value(key).toString().trimmed().toDouble(&ok);
    if (!ok || scale < MinScreenScale || scale > MaxScreenScale) {
        qCWarning(lcSettings) << "Invalid value for" << key << "- ignoring it";
        return 0;
    }
    return scale;
}

void Settings::setOutputScale(const QString &screen, double scale)
{
    if (screen.isEmpty())
        return;
    write(scaleKey(screen), QString::number(qBound(MinScreenScale, scale, MaxScreenScale), 'g', 6));
}

} // namespace Daytona
