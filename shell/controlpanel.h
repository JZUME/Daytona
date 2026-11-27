// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "desktopentry.h"

#include <QMainWindow>

class QAction;
class QActionGroup;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QScrollArea;

namespace Daytona {

class ApplicationIndex;

// One icon in the Control Panel: one of the shell's own property dialogs,
// or an installed settings application.
struct ControlPanelApplet
{
    enum class Builtin
    {
        None, // an application, from entry
        DateTime,
        Display,
        Keyboard,
        Mouse,
        Taskbar
    };

    QString name;
    QString comment; // shown in the status bar when selected
    QString iconName;
    Builtin builtin = Builtin::None;
    DesktopEntry entry;
};

// The applets to show: the shell's own dialogs, then every application
// whose .desktop file lists the freedesktop Settings category. entries are
// the applications the Start menu shows (already filtered by NoDisplay,
// OnlyShowIn and so on). Each of the two groups is sorted by name, as NT
// sorted its applets.
QList<ControlPanelApplet> controlPanelApplets(const QList<DesktopEntry> &entries);

// NT's Control Panel: a folder window of applets. Daytona's own dialogs
// open in the shell; applications are launched. The two are shown under
// separate headings, drawn like Windows XP's "Show in Groups".
//
// Applications register an applet the freedesktop way, with
// Categories=...;Settings; in their .desktop file. The list follows changes
// to installed applications.
class ControlPanel : public QMainWindow
{
    Q_OBJECT

public:
    enum class ViewMode
    {
        LargeIcons,
        SmallIcons,
        List
    };

    explicit ControlPanel(ApplicationIndex *index, QWidget *parent = nullptr);

    // Rebuilds the list from the current applications.
    void prepare();

    // The Daytona group and the Applications group.
    QListWidget *builtinView() const { return m_builtinView; }
    QListWidget *applicationView() const { return m_applicationView; }
    QLabel *applicationHeading() const { return m_applicationHeading; }
    QLabel *statusText() const { return m_status; }
    QAction *openAction() const { return m_open; }
    ViewMode viewMode() const { return m_mode; }
    void setViewMode(ViewMode mode);
    const QList<ControlPanelApplet> &applets() const { return m_applets; }
    // The selected applet, or nullptr.
    const ControlPanelApplet *selectedApplet() const;

public Q_SLOTS:
    // Opens the selected applet, as double-clicking it does.
    void openSelected();
    void refresh();

Q_SIGNALS:
    void dateTimePropertiesRequested();
    void displayPropertiesRequested();
    void keyboardPropertiesRequested();
    void mousePropertiesRequested();
    void taskbarPropertiesRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QListWidget *createView();
    void openItem(QListWidgetItem *item);
    void openApplet(const ControlPanelApplet &applet);
    void selectionChanged(QListWidget *view);
    void updateStatus();
    void updateViewSizes();

    ApplicationIndex *m_index;
    QList<ControlPanelApplet> m_applets;
    QScrollArea *m_scroll;
    QLabel *m_builtinHeading;
    QListWidget *m_builtinView;
    QLabel *m_applicationHeading;
    QWidget *m_applicationRule;
    QListWidget *m_applicationView;
    QLabel *m_status;
    QAction *m_open;
    QAction *m_largeIcons;
    QAction *m_smallIcons;
    QAction *m_list;
    ViewMode m_mode = ViewMode::LargeIcons;
};

} // namespace Daytona
