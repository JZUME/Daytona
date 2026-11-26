// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "colours.h"

#include <QByteArray>
#include <QString>

namespace Daytona {

// The labwc window theme for a colour scheme: Daytona's own theme (shipped
// in labwc/themes/Daytona) with its colours replaced by the scheme's.

// The theme's name. A generated copy goes in the user's data directory under
// the same name, where labwc finds it before the system-wide one.
inline constexpr const char *LabwcThemeName = "Daytona";

// themerc text for the scheme, from Daytona's own themerc.
QByteArray labwcThemerc(const SchemeColours &colours);

// A caption button image recoloured for the scheme. buttonName is one of
// "close", "iconify", "max" or "max_toggled", optionally followed by
// "_pressed" for its sunken image.
QByteArray labwcButtonSvg(const QString &buttonName, const SchemeColours &colours);

// $XDG_DATA_HOME/themes/Daytona/openbox-3
QString labwcUserThemeDir();

// Writes the theme to dir (themerc and every button image labwc 0.8 looks
// for), replacing a theme Daytona generated earlier there.
bool writeLabwcTheme(const QString &dir, const SchemeColours &colours, QString *error = nullptr);

// Removes a theme Daytona generated in dir, so labwc uses the shipped one
// again. Leaves the directory alone if Daytona did not generate it.
// Returns false only if a generated theme could not be removed.
bool removeLabwcTheme(const QString &dir, QString *error = nullptr);

// Asks a running labwc (found from LABWC_PID) to reload its theme.
// Returns false if no labwc is running for this session.
bool reconfigureLabwc();

} // namespace Daytona
