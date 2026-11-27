// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "desktopentry.h"
#include "settings.h"
#include "xdgmenu.h"

#include <QElapsedTimer>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <functional>

class QMenu;

namespace Daytona {

class ApplicationIndex;
class ClickCatcher;
class Settings;

// The NT Start menu: a vertical banner down the left side and, top to
// bottom, Programs, Documents, Settings, Find, Run..., Shut Down....
//
// The menu is its own layer-shell surface, placed by the compositor right
// above the taskbar, so it can open from the Start menu key (which arrives
// over D-Bus, without the input event a popup would need). It draws its
// items with the style's menu item drawing so it matches other menus. Its
// submenus are ordinary QMenu popups of this surface.
class StartMenu : public QWidget
{
    Q_OBJECT

public:
    static constexpr int BannerWidth = 21;

    struct Item
    {
        QIcon icon;
        QString text;
        QMenu *submenu = nullptr;
        std::function<void()> action;
        bool enabled = true;
        bool separator = false;
    };

    StartMenu(ApplicationIndex *index, Settings *settings, QWidget *parent = nullptr);
    ~StartMenu() override;

    // Opens the menu directly above the taskbar, aligned with anchor (the
    // Start button). Opened from the keyboard, the first item is
    // highlighted so the arrow keys work at once; opened with the mouse,
    // nothing is.
    void popUpFrom(QWidget *anchor, bool fromKeyboard = false);

    // The taskbar's edge: the menu opens from it, towards the screen.
    void setEdge(TaskbarEdge edge) { m_edge = edge; }

    // True for a short time after the menu closed. A click on the Start
    // button that closed the menu must not reopen it.
    bool justClosed() const;

    const QList<Item> &items() const { return m_items; }
    int currentIndex() const { return m_current; }
    QMenu *programsMenu() const { return m_programs; }
    QMenu *documentsMenu() const { return m_documents; }

    // Where Documents reads recently used files from (for tests).
    void setRecentDocumentsFile(const QString &path) { m_recentFile = path; }
    QAction *controlPanelAction() const { return m_controlPanel; }
    QAction *findAction() const { return m_find; }

    // The command Find > Files or Folders runs: Commands/Find from the
    // settings file, else PCManFM-Qt's Find Files when it is installed,
    // else none (the item is greyed out).
    static QString findCommand(const Settings *settings);

    QSize sizeHint() const override;

Q_SIGNALS:
    void closed();
    void runRequested();
    void shutDownRequested();
    void taskbarPropertiesRequested();
    void controlPanelRequested();
    void displayPropertiesRequested();
    void mousePropertiesRequested();
    void keyboardPropertiesRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void attachToLayerShell();
    void rebuildPrograms();
    void addPrograms(QMenu *menu, const MenuNode &folder);
    void rebuildDocuments();
    void updateCommandItems();
    int iconSize() const;
    int itemHeight(const Item &item) const;
    QRect itemRect(int index) const;
    int itemAt(const QPoint &pos) const;
    void hoverAt(const QPoint &pos);
    void pressAt(const QPoint &pos);
    void releaseAt(const QPoint &pos);
    void setCurrent(int index);
    void moveCurrent(int step);
    void activate(int index, bool fromKeyboard);
    void openSubmenu(int index, bool selectFirst);
    void closeSubmenu();
    void runCommand(const QString &command, const QString &what);
    static void launchEntry(const DesktopEntry &entry);

    ApplicationIndex *m_index;
    Settings *m_settings;
    QList<Item> m_items;
    QMenu *m_programs;
    QMenu *m_documents;
    QMenu *m_settingsMenu;
    QMenu *m_findMenu;
    QAction *m_controlPanel;
    QAction *m_find;
    ClickCatcher *m_catcher;
    QPointer<QMenu> m_openSubmenu;
    bool m_closingSubmenu = false; // we are closing it, not the compositor
    int m_current = -1;
    QTimer m_submenuDelay;
    QString m_recentFile;
    QElapsedTimer m_closedAt;
    bool m_attached = false;
    TaskbarEdge m_edge = TaskbarEdge::Bottom;
    bool m_rebuildPending = false;
};

} // namespace Daytona
