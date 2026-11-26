// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QListWidget;

namespace Daytona {

// Something a program can be allowed to share, as xdg-desktop-portal-wlr
// lists it to its chooser command: "Monitor: NAME [DESCRIPTION]" or
// "Window: TITLE (ID)" (version 0.8 and later), or just an output name
// (earlier versions).
struct ScreenSource
{
    QString line; // as listed; printed back to choose it
    bool window = false;
    QString name; // the output's name, or the window's title
    QString description; // the output's make and model, if listed
};

ScreenSource parseScreenSource(const QString &line);

// The text the chooser shows for each source: one screen is the "Entire
// screen"; with several, each by its description, else its name.
QStringList screenSourceLabels(const QList<ScreenSource> &sources);

// The dialog shown when a program asks to share the screen (a video call,
// a browser's screen sharing), through xdg-desktop-portal-wlr: which screen
// or window it may share, or Cancel. Run as daytona-shell --choose-screen.
class ScreenChooser : public QDialog
{
    Q_OBJECT

public:
    explicit ScreenChooser(const QList<ScreenSource> &sources, QWidget *parent = nullptr);

    // The chosen source's line, or empty if none.
    QString chosenLine() const;

    QListWidget *list() const { return m_list; }

private:
    QList<ScreenSource> m_sources;
    QListWidget *m_list;
};

// daytona-shell --choose-screen: reads the sources from standard input, one
// per line, asks, and prints the chosen line, or nothing if cancelled.
int runScreenChooser();

} // namespace Daytona
