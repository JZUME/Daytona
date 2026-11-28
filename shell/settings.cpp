// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include "colourscheme.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>
#include <cstdlib>

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
    const QString KeyIconPositions = QStringLiteral("Desktop/IconPositions");
    const QString KeyAutoArrange = QStringLiteral("Desktop/AutoArrange");
    const QString KeyWallpaper = QStringLiteral("Appearance/Wallpaper");
    const QString KeyWallpaperMode = QStringLiteral("Appearance/WallpaperMode");
    const QString KeyScheme = QStringLiteral("Appearance/ColourScheme");
    const QString KeyThemeFiles = QStringLiteral("Appearance/ThemeFiles");
    const QString KeySystemLogo = QStringLiteral("Appearance/SystemStartLogo");
    const QString KeyScreenSaver = QStringLiteral("ScreenSaver/Saver");
    const QString KeyScreenSaverWait = QStringLiteral("ScreenSaver/Wait");
    const QString KeyScreenSaverPassword = QStringLiteral("ScreenSaver/PasswordProtected");
    const QString KeyLockNoCtrlAltDelete = QStringLiteral("Lock/DisableCtrlAltDelete");
    // Read by daytona-session too, which sets the cursor size from it.
    const QString KeyPointerScheme = QStringLiteral("Mouse/PointerScheme");
    const QString KeyKeyboardLayouts = QStringLiteral("Keyboard/Layouts");
    const QString KeyLayoutHotkey = QStringLiteral("Keyboard/Hotkey");

    const QString DefaultScheme = QStringLiteral("Daytona Classic");

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

const QString Settings::FlatProgramsKey = QStringLiteral("StartMenu/FlatPrograms");

bool Settings::flatProgramsMenu() const
{
    return readBool(FlatProgramsKey, false);
}
void Settings::setFlatProgramsMenu(bool flat)
{
    write(FlatProgramsKey, flat);
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

QHash<QString, QPoint> Settings::desktopIconPositions() const
{
    // One "x,y,name" string per icon: names may hold any character but "/",
    // which INI keys would not keep.
    QHash<QString, QPoint> positions;
    const QStringList stored = m_settings.value(KeyIconPositions).toStringList();
    for (const QString &entry : stored) {
        const int first = entry.indexOf(QLatin1Char(','));
        const int second = entry.indexOf(QLatin1Char(','), first + 1);
        if (first < 0 || second < 0 || second + 1 >= entry.size())
            continue;
        bool okX = false;
        bool okY = false;
        const int x = entry.left(first).toInt(&okX);
        const int y = entry.mid(first + 1, second - first - 1).toInt(&okY);
        if (okX && okY && x >= 0 && y >= 0)
            positions.insert(entry.mid(second + 1), QPoint(x, y));
    }
    return positions;
}
void Settings::setDesktopIconPositions(const QHash<QString, QPoint> &positions)
{
    QStringList stored;
    for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
        stored.append(
            QStringLiteral("%1,%2,%3").arg(it.value().x()).arg(it.value().y()).arg(it.key()));
    }
    stored.sort();
    write(KeyIconPositions, stored);
}

const QString Settings::EventSoundNone = QStringLiteral("none");

QString Settings::eventSound(const QString &name) const
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')))
        return {};
    const QString value = m_settings.value(QStringLiteral("Sounds/") + name).toString();
    // A file must be absolute: the INI file is not tied to a directory.
    if (value == EventSoundNone || QDir::isAbsolutePath(value))
        return value;
    return {};
}
void Settings::setEventSound(const QString &name, const QString &sound)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')))
        return;
    const QString key = QStringLiteral("Sounds/") + name;
    if (sound.isEmpty()) {
        if (!m_settings.contains(key))
            return;
        m_settings.remove(key);
        m_settings.sync();
        Q_EMIT changed(key);
        return;
    }
    write(key, sound);
}

bool Settings::desktopAutoArrange() const
{
    return readBool(KeyAutoArrange, false);
}
void Settings::setDesktopAutoArrange(bool autoArrange)
{
    write(KeyAutoArrange, autoArrange);
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

QStringList Settings::keyboardLayouts() const
{
    QStringList codes;
    for (const QString &code : m_settings.value(KeyKeyboardLayouts).toStringList()) {
        if (InputLocale::fromCode(code).isValid())
            codes << code.trimmed();
        else
            qCWarning(lcSettings) << "Ignoring invalid keyboard layout" << code;
    }
    return codes;
}

void Settings::setKeyboardLayouts(const QStringList &codes)
{
    write(KeyKeyboardLayouts, codes);
}

LayoutHotkey Settings::layoutHotkey() const
{
    const QString text = m_settings.value(KeyLayoutHotkey).toString().trimmed().toLower();
    if (text == QLatin1String("ctrlshift"))
        return LayoutHotkey::CtrlShift;
    if (text == QLatin1String("none"))
        return LayoutHotkey::None;
    if (!text.isEmpty() && text != QLatin1String("leftaltshift"))
        qCWarning(lcSettings) << "Invalid value for" << KeyLayoutHotkey << "- using leftaltshift";
    return LayoutHotkey::LeftAltShift;
}

void Settings::setLayoutHotkey(LayoutHotkey hotkey)
{
    write(KeyLayoutHotkey,
        hotkey == LayoutHotkey::CtrlShift  ? QStringLiteral("ctrlshift")
            : hotkey == LayoutHotkey::None ? QStringLiteral("none")
                                           : QStringLiteral("leftaltshift"));
}

ScreenSaver Settings::screenSaver() const
{
    const QString text = m_settings.value(KeyScreenSaver).toString().trimmed().toLower();
    if (text == QLatin1String("none"))
        return ScreenSaver::None;
    if (!text.isEmpty() && text != QLatin1String("blank"))
        qCWarning(lcSettings) << "Invalid value for" << KeyScreenSaver << "- using blank";
    return ScreenSaver::Blank;
}

void Settings::setScreenSaver(ScreenSaver saver)
{
    write(KeyScreenSaver,
        saver == ScreenSaver::None ? QStringLiteral("none") : QStringLiteral("blank"));
}

int Settings::screenSaverWait() const
{
    constexpr int fallback = 15;
    if (!m_settings.contains(KeyScreenSaverWait))
        return fallback;
    bool ok = false;
    const int minutes = m_settings.value(KeyScreenSaverWait).toString().trimmed().toInt(&ok);
    if (!ok || minutes < MinScreenSaverWait || minutes > MaxScreenSaverWait) {
        qCWarning(lcSettings) << "Invalid value for" << KeyScreenSaverWait << "- using" << fallback;
        return fallback;
    }
    return minutes;
}

void Settings::setScreenSaverWait(int minutes)
{
    write(KeyScreenSaverWait, std::clamp(minutes, MinScreenSaverWait, MaxScreenSaverWait));
}

bool Settings::screenSaverPassword() const
{
    return readBool(KeyScreenSaverPassword, false);
}

void Settings::setScreenSaverPassword(bool protect)
{
    write(KeyScreenSaverPassword, protect);
}

bool Settings::lockDisablesCtrlAltDelete() const
{
    return readBool(KeyLockNoCtrlAltDelete, false);
}

void Settings::setLockDisablesCtrlAltDelete(bool disable)
{
    write(KeyLockNoCtrlAltDelete, disable);
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
    // A daytona.ini from before the rename may hold "Windows Standard".
    return name.isEmpty() ? DefaultScheme : currentSchemeName(name);
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

std::optional<QPoint> Settings::outputPosition(const QString &screen) const
{
    const QString key = screenKey(QStringLiteral("ScreenPosition"), screen);
    if (screen.isEmpty() || !m_settings.contains(key))
        return std::nullopt;
    // Written unquoted, QSettings reads "x,y" as a list of two.
    const QVariant value = m_settings.value(key);
    const QStringList parts = value.typeId() == QMetaType::QStringList
        ? value.toStringList()
        : value.toString().split(QLatin1Char(','));
    bool okX = false;
    bool okY = false;
    const int x = parts.value(0).trimmed().toInt(&okX);
    const int y = parts.value(1).trimmed().toInt(&okY);
    // Compositors keep the desktop within 32-bit coordinates.
    constexpr int Limit = 1 << 20;
    if (parts.size() != 2 || !okX || !okY || std::abs(x) > Limit || std::abs(y) > Limit) {
        qCWarning(lcSettings) << "Invalid value for" << key << "- ignoring it";
        return std::nullopt;
    }
    return QPoint(x, y);
}

void Settings::setOutputPosition(const QString &screen, const QPoint &position)
{
    if (screen.isEmpty())
        return;
    write(screenKey(QStringLiteral("ScreenPosition"), screen),
        QStringLiteral("%1,%2").arg(position.x()).arg(position.y()));
}

bool Settings::outputEnabled(const QString &screen) const
{
    if (screen.isEmpty())
        return true;
    return readBool(screenKey(QStringLiteral("ScreenEnabled"), screen), true);
}

void Settings::setOutputEnabled(const QString &screen, bool enabled)
{
    if (screen.isEmpty())
        return;
    write(screenKey(QStringLiteral("ScreenEnabled"), screen), enabled);
}

QString Settings::primaryOutput() const
{
    return m_settings.value(QStringLiteral("Display/PrimaryScreen")).toString().trimmed();
}

void Settings::setPrimaryOutput(const QString &screen)
{
    write(QStringLiteral("Display/PrimaryScreen"), screen);
}

} // namespace Daytona
