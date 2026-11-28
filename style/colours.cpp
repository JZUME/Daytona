// SPDX-License-Identifier: GPL-3.0-or-later

#include "colours.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLinearGradient>
#include <QRect>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace Daytona {

namespace {

    struct Rgb
    {
        const char *key;
        int r, g, b;
    };

    // Daytona Classic (NT 4.0's default colours), in the order Windows lists the keys.
    const Rgb StandardColours[] = {
        { "Scrollbar", 192, 192, 192 },
        { "Background", 0, 128, 128 },
        { "ActiveTitle", 0, 0, 128 },
        { "InactiveTitle", 128, 128, 128 },
        { "Menu", 192, 192, 192 },
        { "Window", 255, 255, 255 },
        { "WindowFrame", 0, 0, 0 },
        { "MenuText", 0, 0, 0 },
        { "WindowText", 0, 0, 0 },
        { "TitleText", 255, 255, 255 },
        { "ActiveBorder", 192, 192, 192 },
        { "InactiveBorder", 192, 192, 192 },
        { "AppWorkspace", 128, 128, 128 },
        { "Hilight", 0, 0, 128 },
        { "HilightText", 255, 255, 255 },
        { "ButtonFace", 192, 192, 192 },
        { "ButtonShadow", 128, 128, 128 },
        { "GrayText", 128, 128, 128 },
        { "ButtonText", 0, 0, 0 },
        { "InactiveTitleText", 192, 192, 192 },
        { "ButtonHilight", 255, 255, 255 },
        { "ButtonDkShadow", 0, 0, 0 },
        { "ButtonLight", 192, 192, 192 },
        { "InfoText", 0, 0, 0 },
        { "InfoWindow", 255, 255, 225 },
        { "ButtonAlternateFace", 180, 180, 180 },
        { "HotTrackingColor", 0, 0, 255 },
        { "GradientActiveTitle", 0, 0, 128 },
        { "GradientInactiveTitle", 128, 128, 128 },
    };

    // Colour files are a few hundred bytes; refuse anything unreasonable.
    constexpr qint64 MaxColourFileSize = 64 * 1024;

    const QString SchemeGroup = QStringLiteral("Scheme");
    const QString ColoursGroup = QStringLiteral("Colors");

} // namespace

const QStringList &colourKeys()
{
    static const QStringList keys = [] {
        QStringList list;
        for (const Rgb &c : StandardColours)
            list << QString::fromLatin1(c.key);
        return list;
    }();
    return keys;
}

const SchemeColours &standardColours()
{
    static const SchemeColours colours = [] {
        SchemeColours map;
        for (const Rgb &c : StandardColours)
            map.insert(QString::fromLatin1(c.key), QColor(c.r, c.g, c.b));
        return map;
    }();
    return colours;
}

std::optional<QColor> parseRgb(const QString &value)
{
    const QStringList parts = value.simplified().split(QLatin1Char(' '));
    if (parts.size() != 3)
        return std::nullopt;
    int rgb[3];
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        rgb[i] = parts[i].toInt(&ok);
        if (!ok || rgb[i] < 0 || rgb[i] > 255)
            return std::nullopt;
    }
    return QColor(rgb[0], rgb[1], rgb[2]);
}

QPalette paletteFromColours(const SchemeColours &colours)
{
    const auto c = [&colours](const char *key) {
        const QString name = QString::fromLatin1(key);
        const QColor colour = colours.value(name);
        return colour.isValid() ? colour : standardColours().value(name);
    };
    const QColor face = c("ButtonFace");
    const QColor gray = c("GrayText");

    QPalette palette(face);
    // Dialogs and other windows are drawn in the 3D face colour, and so is
    // their text: Windows' "Window" and "WindowText" are for edit fields and
    // lists, which Qt calls Base and Text.
    palette.setColor(QPalette::Window, face);
    palette.setColor(QPalette::WindowText, c("ButtonText"));
    palette.setColor(QPalette::Button, face);
    palette.setColor(QPalette::ButtonText, c("ButtonText"));
    palette.setColor(QPalette::Light, c("ButtonHilight"));
    palette.setColor(QPalette::Midlight, c("ButtonLight"));
    palette.setColor(QPalette::Mid, c("ButtonShadow"));
    palette.setColor(QPalette::Dark, c("ButtonShadow"));
    palette.setColor(QPalette::Shadow, c("ButtonDkShadow"));
    palette.setColor(QPalette::Base, c("Window"));
    palette.setColor(QPalette::AlternateBase, face);
    palette.setColor(QPalette::Text, c("WindowText"));
    palette.setColor(QPalette::BrightText, c("ButtonHilight"));
    palette.setColor(QPalette::Highlight, c("Hilight"));
    palette.setColor(QPalette::HighlightedText, c("HilightText"));
    palette.setColor(QPalette::ToolTipBase, c("InfoWindow"));
    palette.setColor(QPalette::ToolTipText, c("InfoText"));
    palette.setColor(QPalette::Link, c("HotTrackingColor"));
    palette.setColor(QPalette::PlaceholderText, gray);

    // NT greys out disabled text in GrayText; the style adds the emboss.
    for (QPalette::ColorRole role :
        { QPalette::WindowText, QPalette::ButtonText, QPalette::Text, QPalette::HighlightedText }) {
        palette.setColor(QPalette::Disabled, role, gray);
    }
    palette.setColor(QPalette::Disabled, QPalette::Base, face);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, c("ButtonShadow"));
    return palette;
}

namespace {

    QColor colourOf(const SchemeColours &colours, const char *key)
    {
        const QString name = QString::fromLatin1(key);
        const QColor colour = colours.value(name);
        return colour.isValid() ? colour : standardColours().value(name);
    }

    QColor titleColour(const SchemeColours &colours, bool active)
    {
        return colourOf(colours, active ? "ActiveTitle" : "InactiveTitle");
    }

    QColor gradientColour(const SchemeColours &colours, bool active)
    {
        return colourOf(colours, active ? "GradientActiveTitle" : "GradientInactiveTitle");
    }

} // namespace

bool titleBarHasGradient(const SchemeColours &colours, bool active)
{
    return titleColour(colours, active) != gradientColour(colours, active);
}

QBrush titleBarBrush(const SchemeColours &colours, bool active, const QRect &rect)
{
    if (!titleBarHasGradient(colours, active))
        return titleColour(colours, active);
    // As labwc draws it: the first and last pixels are the two colours.
    QLinearGradient gradient(rect.left() + 0.5, 0, rect.right() + 0.5, 0);
    gradient.setColorAt(0, titleColour(colours, active));
    gradient.setColorAt(1, gradientColour(colours, active));
    return gradient;
}

QString colourFilePath()
{
    QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config");
    return QDir(base).filePath(QStringLiteral("daytona/colors.ini"));
}

std::optional<ColourFile> readColourFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxColourFileSize)
        return std::nullopt;
    file.close();

    QSettings ini(path, QSettings::IniFormat);
    if (ini.status() != QSettings::NoError)
        return std::nullopt;
    ColourFile result;
    result.name = ini.value(SchemeGroup + QStringLiteral("/Name")).toString().left(256);
    result.colours = standardColours();
    ini.beginGroup(ColoursGroup);
    for (const QString &key : colourKeys()) {
        // QSettings splits "192 192 192" no further, but a value with commas
        // would come back as a list; accept only a plain string.
        const QVariant value = ini.value(key);
        if (value.typeId() != QMetaType::QString)
            continue;
        if (std::optional<QColor> colour = parseRgb(value.toString()))
            result.colours.insert(key, *colour);
    }
    return result;
}

bool writeColourFile(const QString &path, const ColourFile &file, QString *error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QByteArray text;
    text += "; Daytona colour scheme, written by daytona-themetool.\n"
            "; Each colour is a Windows colour key and \"R G B\" (0-255).\n"
            "; See docs/colour-schemes.md.\n";
    // Written by hand rather than with QSettings so the keys keep their
    // Windows order and the write can be atomic.
    // The name is quoted, as QSettings reads it back, so any text survives.
    QString name = file.name.left(256);
    name.remove(QLatin1Char('\n')).remove(QLatin1Char('\r'));
    name.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    name.replace(QLatin1Char('"'), QLatin1String("\\\""));
    text += "[Scheme]\nName=\"" + name.toUtf8() + "\"\n\n[Colors]\n";
    for (const QString &key : colourKeys()) {
        QColor colour = file.colours.value(key);
        if (!colour.isValid())
            colour = standardColours().value(key);
        text += key.toLatin1() + '=' + QByteArray::number(colour.red()) + ' '
            + QByteArray::number(colour.green()) + ' ' + QByteArray::number(colour.blue()) + '\n';
    }

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(text) != text.size() || !out.commit()) {
        if (error)
            *error = out.errorString();
        return false;
    }
    return true;
}

} // namespace Daytona
