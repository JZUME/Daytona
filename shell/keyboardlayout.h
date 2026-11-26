// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace Daytona {

class Settings;

// One installed input locale: an XKB layout and optional variant, such as
// gb or de(nodeadkeys).
struct InputLocale
{
    QString layout;
    QString variant;

    // "layout" or "layout(variant)", as Daytona's INI file stores it.
    QString code() const;
    // Parses code(); an empty layout when text is not a valid code.
    static InputLocale fromCode(const QString &text);

    bool isValid() const { return !layout.isEmpty(); }
    friend bool operator==(const InputLocale &a, const InputLocale &b)
    {
        return a.layout == b.layout && a.variant == b.variant;
    }
};

// NT's "Switch locales" hot keys, done by XKB's own group switching.
enum class LayoutHotkey
{
    LeftAltShift,
    CtrlShift,
    None
};

// What the compositor is given: XKB's model, layouts and options.
struct XkbConfig
{
    QString model;
    QList<InputLocale> locales; // the first is the default
    QStringList options;

    LayoutHotkey hotkey() const;
};

QString defaultSystemKeyboardFile();

// The system's keyboard, from Debian's /etc/default/keyboard (XKBMODEL,
// XKBLAYOUT, XKBVARIANT, XKBOPTIONS), as the installer set it. US English
// when the file is missing or names no layout.
XkbConfig systemKeyboard(const QString &path = defaultSystemKeyboardFile());

// The keyboard for the session: the locales and hot key chosen in Keyboard
// Properties, keeping the system's model and its options other than layout
// switching, or the system's keyboard unchanged when nothing was chosen.
XkbConfig sessionKeyboard(const Settings *settings, const XkbConfig &system);

// The XKB option for a hot key, or an empty string for None.
QString hotkeyOption(LayoutHotkey hotkey);

// labwc's environment file (XKB_DEFAULT_MODEL, _LAYOUT, _VARIANT and
// _OPTIONS) for config.
QByteArray labwcEnvironment(const XkbConfig &config);
// The locales an environment file sets, in order; empty if it sets none.
QList<InputLocale> localesFromEnvironment(const QByteArray &environment);

// The labwc configuration directory daytona-session made for this session
// ($DAYTONA_LABWC_CONFIG), or an empty string outside a Daytona session.
QString labwcConfigDir();

// Writes config to labwc's environment file in dir and asks labwc
// ($LABWC_PID) to reload it, which switches every keyboard at once.
bool applyToLabwc(const XkbConfig &config, const QString &dir, QString *error = nullptr);

// One keyboard layout or variant in xkeyboard-config's list (evdev.xml).
struct LayoutInfo
{
    InputLocale locale;
    QString description; // "English (UK)", "German (no dead keys)"
    QString shortName; // the indicator's text, e.g. "en"
    QStringList languages; // ISO 639-2 codes, e.g. "eng"
};

// xkeyboard-config's layouts and their variants, read from evdev.xml.
class LayoutRegistry
{
public:
    // Reads path, or when it is empty the system's evdev.xml (under
    // $XKB_CONFIG_ROOT when set, as for xkbcommon).
    explicit LayoutRegistry(const QString &path = QString());

    const QList<LayoutInfo> &layouts() const { return m_layouts; }
    // The layout or variant, or one made up from the code when unknown.
    LayoutInfo find(const InputLocale &locale) const;

    // A language's name for an ISO 639-2 code ("eng": "English"), or the
    // code itself when Qt does not know it.
    static QString languageName(const QString &iso639);
    // The indicator's two letters: "En", "De".
    static QString indicatorText(const LayoutInfo &info);

private:
    QList<LayoutInfo> m_layouts;
};

} // namespace Daytona
