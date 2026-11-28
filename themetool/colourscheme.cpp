// SPDX-License-Identifier: GPL-3.0-or-later

#include "colourscheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>
#include <utility>

namespace Daytona {

namespace {

    struct Rgb
    {
        const char *key;
        int r, g, b;
    };

    // Approximations of NT's high-contrast schemes; only the colours that differ
    // from Windows Standard are listed.
    const Rgb HighContrastBlack[] = {
        { "Scrollbar", 0, 0, 0 },
        { "Background", 0, 0, 0 },
        { "ActiveTitle", 128, 0, 128 },
        { "InactiveTitle", 0, 128, 0 },
        { "Menu", 0, 0, 0 },
        { "Window", 0, 0, 0 },
        { "WindowFrame", 255, 255, 255 },
        { "MenuText", 255, 255, 255 },
        { "WindowText", 255, 255, 255 },
        { "TitleText", 255, 255, 255 },
        { "ActiveBorder", 255, 255, 0 },
        { "InactiveBorder", 0, 128, 0 },
        { "AppWorkspace", 0, 0, 0 },
        { "Hilight", 128, 0, 128 },
        { "HilightText", 255, 255, 255 },
        { "ButtonFace", 0, 0, 0 },
        { "ButtonShadow", 128, 128, 128 },
        { "GrayText", 0, 255, 0 },
        { "ButtonText", 255, 255, 255 },
        { "InactiveTitleText", 255, 255, 255 },
        { "ButtonHilight", 255, 255, 255 },
        { "ButtonDkShadow", 128, 128, 128 },
        { "ButtonLight", 192, 192, 192 },
        { "InfoText", 255, 255, 255 },
        { "InfoWindow", 0, 0, 0 },
        { "GradientActiveTitle", 128, 0, 128 },
        { "GradientInactiveTitle", 0, 128, 0 },
    };

    const Rgb HighContrastWhite[] = {
        { "Scrollbar", 255, 255, 255 },
        { "Background", 255, 255, 255 },
        { "ActiveTitle", 0, 0, 0 },
        { "InactiveTitle", 255, 255, 255 },
        { "Menu", 255, 255, 255 },
        { "Window", 255, 255, 255 },
        { "WindowFrame", 0, 0, 0 },
        { "MenuText", 0, 0, 0 },
        { "WindowText", 0, 0, 0 },
        { "TitleText", 255, 255, 255 },
        { "ActiveBorder", 128, 128, 128 },
        { "InactiveBorder", 192, 192, 192 },
        { "AppWorkspace", 128, 128, 128 },
        { "Hilight", 0, 0, 0 },
        { "HilightText", 255, 255, 255 },
        { "ButtonFace", 255, 255, 255 },
        { "ButtonShadow", 128, 128, 128 },
        { "GrayText", 0, 128, 0 },
        { "ButtonText", 0, 0, 0 },
        { "InactiveTitleText", 0, 0, 0 },
        { "ButtonHilight", 192, 192, 192 },
        { "ButtonDkShadow", 0, 0, 0 },
        { "ButtonLight", 255, 255, 255 },
        { "InfoText", 0, 0, 0 },
        { "InfoWindow", 255, 255, 255 },
        { "GradientActiveTitle", 0, 0, 0 },
        { "GradientInactiveTitle", 255, 255, 255 },
    };

    template <size_t N> void apply(ColourScheme &scheme, const Rgb (&colours)[N])
    {
        for (const Rgb &c : colours)
            scheme.colours.insert(QString::fromLatin1(c.key), QColor(c.r, c.g, c.b));
    }

    ColourScheme standardScheme()
    {
        ColourScheme scheme;
        scheme.name = QStringLiteral("Windows Standard");
        scheme.colours = standardColours();
        return scheme;
    }

    QString decode(const QByteArray &data)
    {
        if (data.startsWith("\xff\xfe") || data.startsWith("\xfe\xff")) {
            QStringDecoder utf16(QStringDecoder::Utf16);
            return utf16(data);
        }
        QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
        const QString text = utf8(data);
        if (!utf8.hasError())
            return text;
        // Plus! themes were written in the Windows ANSI code page.
        QStringDecoder windows1252("windows-1252");
        if (windows1252.isValid())
            return windows1252(data);
        QStringDecoder latin1(QStringDecoder::Latin1);
        return latin1(data);
    }

    QString unquote(QString value)
    {
        value = value.trimmed();
        if (value.size() >= 2 && value.startsWith(QLatin1Char('"'))
            && value.endsWith(QLatin1Char('"')))
            value = value.mid(1, value.size() - 2);
        return value;
    }

    // A Windows path from a .theme file ("%WinDir%MEDIA\\X.BMP",
    // "C:\\Plus!\\Themes\\X.BMP", "%ThemeDir%X.bmp"). Only its file name is
    // used, and only a file of that name in the theme's own directory is
    // accepted, so nothing outside that directory can be referenced.
    QString resolveInThemeDir(const QString &windowsPath, const QString &themeDir)
    {
        QString fileName = windowsPath;
        // "%ThemeDir%name.bmp" has no separator after the variable.
        static const QRegularExpression variable(QStringLiteral("%[^%]*%"));
        fileName.replace(variable, QStringLiteral("/"));
        fileName.replace(QLatin1Char('\\'), QLatin1Char('/'));
        fileName = fileName.section(QLatin1Char('/'), -1).trimmed();
        if (fileName.isEmpty() || fileName == QLatin1String(".") || fileName == QLatin1String(".."))
            return {};
        // Windows file names are case-insensitive.
        const QDir dir(themeDir);
        const QStringList entries = dir.entryList(QDir::Files);
        for (const QString &entry : entries) {
            if (entry.compare(fileName, Qt::CaseInsensitive) == 0)
                return QFileInfo(dir.filePath(entry)).canonicalFilePath();
        }
        return {};
    }

} // namespace

QStringList builtInSchemeNames()
{
    return { QStringLiteral("Windows Standard"), QStringLiteral("High Contrast Black"),
        QStringLiteral("High Contrast White") };
}

std::optional<ColourScheme> builtInScheme(const QString &name)
{
    ColourScheme scheme = standardScheme();
    if (name.compare(QLatin1String("Windows Standard"), Qt::CaseInsensitive) == 0)
        return scheme;
    if (name.compare(QLatin1String("High Contrast Black"), Qt::CaseInsensitive) == 0) {
        apply(scheme, HighContrastBlack);
    } else if (name.compare(QLatin1String("High Contrast White"), Qt::CaseInsensitive) == 0) {
        apply(scheme, HighContrastWhite);
    } else {
        return std::nullopt;
    }
    scheme.name = name;
    return scheme;
}

ColourScheme parseTheme(const QByteArray &data, const QString &name, const QString &themeDir)
{
    ColourScheme scheme = standardScheme();
    scheme.name = name;

    // Section names are case-insensitive; store them lowercased.
    QMap<QString, QMap<QString, QString>> sections;
    QString section;
    const QStringList lines = decode(data).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            section = line.mid(1, line.size() - 2).trimmed().toLower();
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed().toLower();
        if (!sections[section].contains(key))
            sections[section].insert(key, unquote(line.mid(eq + 1)));
    }

    const auto colours = sections.value(QStringLiteral("control panel\\colors"));
    for (const QString &key : colourKeys()) {
        const QString value = colours.value(key.toLower());
        if (value.isEmpty())
            continue;
        if (std::optional<QColor> colour = parseRgb(value))
            scheme.colours.insert(key, *colour);
        else
            scheme.warnings << QStringLiteral(
                "Colour %1 is not valid (\"%2\"); using Windows Standard")
                                   .arg(key, value.left(40));
    }

    // Windows 95 and NT .theme files have no Gradient keys. Their title bars
    // are solid, so the gradient colour follows the title colour rather than
    // Windows Standard's.
    for (const auto &[gradient, title] : { std::pair { "GradientActiveTitle", "ActiveTitle" },
             std::pair { "GradientInactiveTitle", "InactiveTitle" } }) {
        if (colours.value(QString::fromLatin1(gradient).toLower()).isEmpty())
            scheme.colours.insert(
                QString::fromLatin1(gradient), scheme.colours.value(QString::fromLatin1(title)));
    }

    const auto desktop = sections.value(QStringLiteral("control panel\\desktop"));
    const QString wallpaper = desktop.value(QStringLiteral("wallpaper"));
    if (!wallpaper.isEmpty()
        && wallpaper.compare(QLatin1String("(none)"), Qt::CaseInsensitive) != 0) {
        scheme.wallpaper = resolveInThemeDir(wallpaper, themeDir);
        if (scheme.wallpaper.isEmpty())
            scheme.warnings << QStringLiteral("Wallpaper \"%1\" was not found next to the theme")
                                   .arg(wallpaper.left(200));
    }
    // TileWallpaper=1 tiles whatever the style. WallpaperStyle came with
    // Windows 98 (2 stretch) and Windows 7 (6 fit, 10 fill); anything else
    // centres.
    const QString style = desktop.value(QStringLiteral("wallpaperstyle")).trimmed();
    if (desktop.value(QStringLiteral("tilewallpaper")).trimmed() == QLatin1String("1"))
        scheme.wallpaperMode = QStringLiteral("tile");
    else if (style == QLatin1String("2"))
        scheme.wallpaperMode = QStringLiteral("stretch");
    else if (style == QLatin1String("6"))
        scheme.wallpaperMode = QStringLiteral("fit");
    else if (style == QLatin1String("10"))
        scheme.wallpaperMode = QStringLiteral("fill");

    const auto cursors = sections.value(QStringLiteral("control panel\\cursors"));
    for (auto it = cursors.constBegin(); it != cursors.constEnd(); ++it) {
        if (it.value().isEmpty() || it.key() == QLatin1String("default"))
            continue;
        const QString path = resolveInThemeDir(it.value(), themeDir);
        if (!path.isEmpty())
            scheme.cursors.insert(it.key(), path);
    }
    if (!cursors.isEmpty())
        scheme.warnings << QStringLiteral(
            "Windows cursors (.cur/.ani) are not supported; the Chicago95 cursors stay in use");

    // Parts of a Plus! theme Daytona does not apply.
    bool sounds = false;
    bool icons = false;
    for (auto it = sections.constBegin(); it != sections.constEnd(); ++it) {
        if (it.key().startsWith(QLatin1String("appevents\\")))
            sounds = true;
        if (it.key().contains(QLatin1String("defaulticon")))
            icons = true;
    }
    if (sounds)
        scheme.warnings << QStringLiteral("Sound events are not supported");
    if (icons)
        scheme.warnings << QStringLiteral("Desktop icons are not supported");
    if (sections.value(QStringLiteral("boot")).contains(QStringLiteral("scrnsave.exe")))
        scheme.warnings << QStringLiteral("Screen savers are not supported");
    const auto metrics = sections.value(QStringLiteral("control panel\\desktop\\windowmetrics"));
    if (!metrics.isEmpty())
        scheme.warnings << QStringLiteral("Fonts and window metrics are not supported yet");
    return scheme;
}

std::optional<ColourScheme> readThemeFile(const QString &path)
{
    QFile file(path);
    // .theme files are small text files; refuse anything unreasonable.
    if (!file.open(QIODevice::ReadOnly) || file.size() > 4 * 1024 * 1024)
        return std::nullopt;
    const QFileInfo info(path);
    return parseTheme(file.readAll(), info.completeBaseName(), info.absolutePath());
}

} // namespace Daytona
