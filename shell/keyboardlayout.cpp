// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardlayout.h"

#include "settings.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QLocale>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <cerrno>
#include <csignal>
#include <cstring>

Q_LOGGING_CATEGORY(lcKeyboard, "daytona.keyboard", QtInfoMsg)

namespace Daytona {

namespace {

    QStringList splitList(const QString &text)
    {
        return text.split(QLatin1Char(','));
    }

    // Pairs XKB's comma-separated layout and variant lists.
    QList<InputLocale> localesFromLists(const QString &layouts, const QString &variants)
    {
        QList<InputLocale> locales;
        const QStringList variantList = splitList(variants);
        const QStringList layoutList = splitList(layouts);
        for (int i = 0; i < layoutList.size(); ++i) {
            const InputLocale locale { layoutList.at(i).trimmed(), variantList.value(i).trimmed() };
            if (locale.isValid() && !locales.contains(locale))
                locales << locale;
        }
        return locales;
    }

    // A value from a shell-style KEY="value" line, without its quotes.
    QString unquote(QString value)
    {
        value = value.trimmed();
        if (value.size() >= 2
            && (value.front() == QLatin1Char('"') || value.front() == QLatin1Char('\''))
            && value.back() == value.front())
            value = value.mid(1, value.size() - 2);
        return value.trimmed();
    }

    QHash<QString, QString> readAssignments(const QByteArray &text)
    {
        QHash<QString, QString> values;
        for (const QByteArray &rawLine : text.split('\n')) {
            const QString line = QString::fromUtf8(rawLine).trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            const qsizetype equals = line.indexOf(QLatin1Char('='));
            if (equals <= 0)
                continue;
            QString key = line.left(equals).trimmed();
            if (key.startsWith(QLatin1String("export ")))
                key = key.mid(7).trimmed();
            values.insert(key, unquote(line.mid(equals + 1)));
        }
        return values;
    }

    // XKB names are letters, digits and a few punctuation marks; anything
    // else (spaces, $, quotes) must not reach the environment file.
    bool isXkbName(const QString &name)
    {
        static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9_+:.()-]*$"));
        return valid.match(name).hasMatch();
    }

    QString joined(const QStringList &list)
    {
        QStringList safe;
        for (const QString &item : list)
            safe << (isXkbName(item) ? item : QString());
        return safe.join(QLatin1Char(','));
    }

} // namespace

QString InputLocale::code() const
{
    if (variant.isEmpty())
        return layout;
    return layout + QLatin1Char('(') + variant + QLatin1Char(')');
}

InputLocale InputLocale::fromCode(const QString &text)
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*([A-Za-z0-9_-]+)\\s*(?:\\(\\s*([A-Za-z0-9_+-]+)\\s*\\))?\\s*$"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return {};
    return { match.captured(1), match.captured(2) };
}

LayoutHotkey XkbConfig::hotkey() const
{
    for (const QString &option : options) {
        if (!option.startsWith(QLatin1String("grp:")))
            continue;
        if (option.contains(QLatin1String("alt")) && option.contains(QLatin1String("shift")))
            return LayoutHotkey::LeftAltShift;
        if (option.contains(QLatin1String("ctrl")) && option.contains(QLatin1String("shift")))
            return LayoutHotkey::CtrlShift;
        return LayoutHotkey::None;
    }
    // NT's default, for when a second locale is added.
    return LayoutHotkey::LeftAltShift;
}

QString defaultSystemKeyboardFile()
{
    return QStringLiteral("/etc/default/keyboard");
}

XkbConfig systemKeyboard(const QString &path)
{
    XkbConfig config;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QHash<QString, QString> values = readAssignments(file.read(64 * 1024));
        config.model = values.value(QStringLiteral("XKBMODEL"));
        config.locales = localesFromLists(
            values.value(QStringLiteral("XKBLAYOUT")), values.value(QStringLiteral("XKBVARIANT")));
        for (const QString &option : splitList(values.value(QStringLiteral("XKBOPTIONS")))) {
            if (!option.trimmed().isEmpty())
                config.options << option.trimmed();
        }
    }
    if (config.locales.isEmpty())
        config.locales << InputLocale { QStringLiteral("us"), {} };
    return config;
}

QString hotkeyOption(LayoutHotkey hotkey)
{
    switch (hotkey) {
    case LayoutHotkey::LeftAltShift:
        return QStringLiteral("grp:alt_shift_toggle");
    case LayoutHotkey::CtrlShift:
        return QStringLiteral("grp:ctrl_shift_toggle");
    case LayoutHotkey::None:
        break;
    }
    return {};
}

XkbConfig sessionKeyboard(const Settings *settings, const XkbConfig &system)
{
    if (!settings)
        return system;
    QList<InputLocale> locales;
    for (const QString &code : settings->keyboardLayouts()) {
        const InputLocale locale = InputLocale::fromCode(code);
        if (locale.isValid() && !locales.contains(locale))
            locales << locale;
    }
    if (locales.isEmpty())
        return system;

    XkbConfig config;
    config.model = system.model;
    config.locales = locales;
    // The system's other options (Caps Lock as Ctrl and so on) are kept;
    // layout switching is Keyboard Properties' to choose.
    for (const QString &option : system.options) {
        if (!option.startsWith(QLatin1String("grp:")))
            config.options << option;
    }
    const QString option = hotkeyOption(settings->layoutHotkey());
    if (locales.size() > 1 && !option.isEmpty())
        config.options.prepend(option);
    return config;
}

QByteArray labwcEnvironment(const XkbConfig &config)
{
    QStringList layouts;
    QStringList variants;
    for (const InputLocale &locale : config.locales) {
        layouts << locale.layout;
        variants << locale.variant;
    }
    // Every variable is always written: labwc keeps a variable the file no
    // longer sets when it reloads.
    QByteArray text = "# Written by Daytona from Keyboard Properties; changes here are lost.\n";
    text += "XKB_DEFAULT_MODEL=" + (isXkbName(config.model) ? config.model : QString()).toUtf8()
        + '\n';
    text += "XKB_DEFAULT_LAYOUT=" + joined(layouts).toUtf8() + '\n';
    text += "XKB_DEFAULT_VARIANT=" + joined(variants).toUtf8() + '\n';
    text += "XKB_DEFAULT_OPTIONS=" + joined(config.options).toUtf8() + '\n';
    return text;
}

QList<InputLocale> localesFromEnvironment(const QByteArray &environment)
{
    const QHash<QString, QString> values = readAssignments(environment);
    return localesFromLists(values.value(QStringLiteral("XKB_DEFAULT_LAYOUT")),
        values.value(QStringLiteral("XKB_DEFAULT_VARIANT")));
}

QString labwcConfigDir()
{
    return qEnvironmentVariable("DAYTONA_LABWC_CONFIG");
}

bool applyToLabwc(const XkbConfig &config, const QString &dir, QString *error)
{
    if (dir.isEmpty()) {
        if (error)
            *error = QStringLiteral("not running in a Daytona session");
        return false;
    }
    QSaveFile file(QDir(dir).filePath(QStringLiteral("environment")));
    if (!file.open(QIODevice::WriteOnly) || file.write(labwcEnvironment(config)) < 0
        || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    // labwc --reconfigure does the same: labwc reads the environment file
    // again on SIGHUP and builds each keyboard's keymap from it.
    const int pid = qEnvironmentVariableIntValue("LABWC_PID");
    if (pid <= 0 || ::kill(pid, SIGHUP) != 0) {
        if (error)
            *error = pid <= 0 ? QStringLiteral("LABWC_PID is not set")
                              : QString::fromLocal8Bit(std::strerror(errno));
        return false;
    }
    return true;
}

LayoutRegistry::LayoutRegistry(const QString &path)
{
    QString file = path;
    const QString root = qEnvironmentVariable("XKB_CONFIG_ROOT");
    if (file.isEmpty() && !root.isEmpty())
        file = QDir(root).filePath(QStringLiteral("rules/evdev.xml"));
    if (file.isEmpty()) {
        for (const char *candidate : { "/usr/share/X11/xkb/rules/evdev.xml",
                 "/usr/share/xkeyboard-config-2/rules/evdev.xml" }) {
            if (QFile::exists(QString::fromLatin1(candidate))) {
                file = QString::fromLatin1(candidate);
                break;
            }
        }
    }
    QFile in(file);
    if (file.isEmpty() || !in.open(QIODevice::ReadOnly)) {
        qCWarning(lcKeyboard) << "Cannot read the keyboard layout list" << file;
        return;
    }

    // <layout><configItem>...</configItem><variantList><variant>
    // <configItem>...</configItem></variant>...</variantList></layout>
    QXmlStreamReader xml(&in);
    const auto readConfigItem = [&xml](LayoutInfo &info, QString &name) {
        while (xml.readNextStartElement()) {
            const auto tag = xml.name();
            if (tag == QLatin1String("name")) {
                name = xml.readElementText().trimmed();
            } else if (tag == QLatin1String("shortDescription")) {
                info.shortName = xml.readElementText().trimmed();
            } else if (tag == QLatin1String("description")) {
                info.description = xml.readElementText().trimmed();
            } else if (tag == QLatin1String("languageList")) {
                QStringList languages;
                while (xml.readNextStartElement()) {
                    if (xml.name() == QLatin1String("iso639Id"))
                        languages << xml.readElementText().trimmed();
                    else
                        xml.skipCurrentElement();
                }
                if (!languages.isEmpty())
                    info.languages = languages;
            } else {
                xml.skipCurrentElement();
            }
        }
    };
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QLatin1String("layout"))
            continue;
        LayoutInfo base;
        QList<LayoutInfo> variants;
        while (xml.readNextStartElement()) {
            if (xml.name() == QLatin1String("configItem")) {
                readConfigItem(base, base.locale.layout);
            } else if (xml.name() == QLatin1String("variantList")) {
                while (xml.readNextStartElement()) {
                    if (xml.name() != QLatin1String("variant")) {
                        xml.skipCurrentElement();
                        continue;
                    }
                    // A variant inherits what it does not say from its layout.
                    LayoutInfo variant;
                    while (xml.readNextStartElement()) {
                        if (xml.name() == QLatin1String("configItem"))
                            readConfigItem(variant, variant.locale.variant);
                        else
                            xml.skipCurrentElement();
                    }
                    variants << variant;
                }
            } else {
                xml.skipCurrentElement();
            }
        }
        if (base.locale.layout.isEmpty())
            continue;
        m_layouts << base;
        for (LayoutInfo variant : std::as_const(variants)) {
            if (variant.locale.variant.isEmpty())
                continue;
            variant.locale.layout = base.locale.layout;
            if (variant.shortName.isEmpty())
                variant.shortName = base.shortName;
            if (variant.languages.isEmpty())
                variant.languages = base.languages;
            if (variant.description.isEmpty())
                variant.description = base.description;
            m_layouts << variant;
        }
    }
    if (xml.hasError())
        qCWarning(lcKeyboard) << "Error reading" << file << xml.errorString();
}

LayoutInfo LayoutRegistry::find(const InputLocale &locale) const
{
    for (const LayoutInfo &info : m_layouts) {
        if (info.locale == locale)
            return info;
    }
    LayoutInfo unknown;
    unknown.locale = locale;
    unknown.description = locale.code();
    unknown.shortName = locale.layout.left(2);
    return unknown;
}

QString LayoutRegistry::languageName(const QString &iso639)
{
    const QLocale::Language language = QLocale::codeToLanguage(iso639);
    if (language == QLocale::AnyLanguage || language == QLocale::C)
        return iso639;
    return QLocale::languageToString(language);
}

QString LayoutRegistry::indicatorText(const LayoutInfo &info)
{
    QString text = info.shortName.left(2);
    if (text.isEmpty())
        text = info.locale.layout.left(2);
    if (!text.isEmpty())
        text = text.front().toUpper() + text.mid(1).toLower();
    return text;
}

} // namespace Daytona
