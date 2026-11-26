// SPDX-License-Identifier: GPL-3.0-or-later

#include "colourscheme.h"
#include "gtktheme.h"
#include "labwctheme.h"
#include "xcursor.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <cstdio>

namespace {

void print(const Daytona::ColourScheme &scheme)
{
    std::printf("Scheme: %s\n", qPrintable(scheme.name));
    for (const QString &key : Daytona::colourKeys()) {
        const QColor c = scheme.colour(key);
        std::printf("  %-22s %3d %3d %3d\n", qPrintable(key), c.red(), c.green(), c.blue());
    }
    if (scheme.wallpaper.isEmpty())
        std::printf("Wallpaper: (none)\n");
    else
        std::printf(
            "Wallpaper: %s (%s)\n", qPrintable(scheme.wallpaper), qPrintable(scheme.wallpaperMode));
    for (auto it = scheme.cursors.constBegin(); it != scheme.cursors.constEnd(); ++it)
        std::printf("Cursor %s: %s\n", qPrintable(it.key()), qPrintable(it.value()));
    for (const QString &warning : scheme.warnings)
        std::printf("Warning: %s\n", qPrintable(warning));
}

QString settingsPath()
{
    // daytona.ini, next to colors.ini.
    return QFileInfo(Daytona::colourFilePath()).dir().filePath(QStringLiteral("daytona.ini"));
}

// Records the scheme name in daytona.ini.
void recordSchemeName(const QString &name)
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Appearance/ColourScheme"), name);
}

// Copies the theme's wallpaper into Daytona's data directory and records it,
// with its display mode (tile, centre, stretch, ...), in daytona.ini. Qt reads BMP files itself, so
// they are copied as they are.
void applyWallpaper(const Daytona::ColourScheme &scheme)
{
    if (scheme.wallpaper.isEmpty())
        return;
    QString base = qEnvironmentVariable("XDG_DATA_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/share");
    const QDir dir(QDir(base).filePath(QStringLiteral("daytona/wallpaper")));
    const QString target = dir.filePath(QFileInfo(scheme.wallpaper).fileName());
    QDir().mkpath(dir.path());
    if (QFileInfo(target).canonicalFilePath() != QFileInfo(scheme.wallpaper).canonicalFilePath()) {
        QFile::remove(target);
        if (!QFile::copy(scheme.wallpaper, target)) {
            std::fprintf(stderr, "Cannot copy the wallpaper to '%s'\n", qPrintable(target));
            return;
        }
    }
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("Appearance/Wallpaper"), target);
    settings.setValue(QStringLiteral("Appearance/WallpaperMode"), scheme.wallpaperMode);
    std::printf("Wallpaper: %s (%s)\n", qPrintable(target), qPrintable(scheme.wallpaperMode));
}

// GTK 3 applications: the Daytona GTK theme in the scheme's colours, and the
// Chicago95 icon theme and Daytona's cursor theme in settings.ini.
void applyGtk(const Daytona::ColourScheme &scheme, bool standard)
{
    QString error;
    const QString dir = Daytona::gtkUserThemeDir();
    const bool ok = standard
        ? Daytona::removeGtkTheme(dir, &error)
        : Daytona::writeGtkTheme(dir, scheme.colours, Daytona::findChicago95GtkCss(), &error);
    if (!ok)
        std::fprintf(stderr, "GTK colours not changed: %s\n", qPrintable(error));
    const QString settings = Daytona::gtkSettingsPath();
    if (!Daytona::setGtkSettings(settings, QStringLiteral("Chicago95"),
            QString::fromLatin1(Daytona::CursorThemeName), &error))
        std::fprintf(stderr, "GTK settings not changed: %s\n", qPrintable(error));
    // The pointer scheme's size, as daytona-session set it.
    bool sizeSet = false;
    const int size = qEnvironmentVariableIntValue("XCURSOR_SIZE", &sizeSet);
    if (!Daytona::setGtkCursorSettings(settings, QString::fromLatin1(Daytona::CursorThemeName),
            sizeSet && size > 0 ? size
                                : Daytona::pointerSchemeSize(Daytona::PointerScheme::Standard),
            &error))
        std::fprintf(stderr, "GTK settings not changed: %s\n", qPrintable(error));
}

int apply(const Daytona::ColourScheme &scheme)
{
    const QString path = Daytona::colourFilePath();
    QString error;
    if (!Daytona::writeColourFile(path, { scheme.name, scheme.colours }, &error)) {
        std::fprintf(stderr, "Cannot write '%s': %s\n", qPrintable(path), qPrintable(error));
        return 1;
    }
    recordSchemeName(scheme.name);

    // Window frames: a generated labwc theme, or the shipped one for
    // Windows Standard.
    const QString themeDir = Daytona::labwcUserThemeDir();
    const bool standard = scheme.colours == Daytona::standardColours();
    const bool themeOk = standard ? Daytona::removeLabwcTheme(themeDir, &error)
                                  : Daytona::writeLabwcTheme(themeDir, scheme.colours, &error);
    if (!themeOk)
        std::fprintf(stderr, "Window frames not changed: %s\n", qPrintable(error));
    else if (!Daytona::reconfigureLabwc())
        std::printf("labwc is not running here; window frames change when it next starts\n");
    applyGtk(scheme, standard);
    applyWallpaper(scheme);
    std::printf("Applied colour scheme '%s' to %s\n", qPrintable(scheme.name), qPrintable(path));
    // What the theme had that Daytona does not apply.
    for (const QString &warning : scheme.warnings)
        std::printf("Warning: %s\n", qPrintable(warning));
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("daytona-themetool"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DAYTONA_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Converts Windows .theme files for the Daytona desktop."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("inspect or apply"));
    parser.addPositionalArgument(QStringLiteral("theme"),
        QStringLiteral("A .theme file, or with --scheme a built-in scheme name."));
    const QCommandLineOption schemeOption(QStringLiteral("scheme"),
        QStringLiteral("Use a built-in scheme (%1).")
            .arg(Daytona::builtInSchemeNames().join(QStringLiteral(", "))),
        QStringLiteral("name"));
    parser.addOption(schemeOption);
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    const QString command = args.value(0);
    if (command != QLatin1String("inspect") && command != QLatin1String("apply")) {
        parser.showHelp(2);
    }
    std::optional<Daytona::ColourScheme> scheme;
    if (parser.isSet(schemeOption)) {
        scheme = Daytona::builtInScheme(parser.value(schemeOption));
        if (!scheme) {
            std::fprintf(stderr, "Unknown scheme '%s'\n", qPrintable(parser.value(schemeOption)));
            return 1;
        }
    } else {
        if (args.size() < 2)
            parser.showHelp(2);
        scheme = Daytona::readThemeFile(args.at(1));
        if (!scheme) {
            std::fprintf(stderr, "Cannot read '%s'\n", qPrintable(args.at(1)));
            return 1;
        }
    }
    if (command == QLatin1String("apply"))
        return apply(*scheme);
    print(*scheme);
    return 0;
}
