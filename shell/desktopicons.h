// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "desktopfiles.h"

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QIcon>
#include <QMargins>
#include <QTimer>
#include <QWidget>
#include <optional>

class QAction;
class QLineEdit;
class QMenu;

namespace Daytona {

class Settings;

// Where icons go: NT's grid of columns from the top left of the area they
// may use, CellSize apart. index counts down each column, then across.
QPoint desktopGridCell(const QRect &area, int index);
// An icon's image, size logical pixels square: exactly size * ratio device
// pixels, with that device pixel ratio, whatever size QIcon has.
QPixmap desktopIconImage(const QIcon &icon, int size, qreal ratio);
// The grid cell nearest point (a cell's top left) inside area.
QPoint nearestGridCell(const QRect &area, const QPoint &point);

// NT's desktop icons, drawn over the primary monitor's desktop: My Computer,
// Network Neighborhood, Recycle Bin and the Desktop folder. Opening, the
// trash and the clipboard go through the file manager's standard formats
// (AGENT.md, Desktop).
class DesktopIconView : public QWidget
{
    Q_OBJECT

public:
    // NT's default icon spacing at 96 dpi: 32-pixel icons 43 pixels apart.
    static constexpr int CellSize = 75;
    static constexpr int IconSize = 32;

    // properties is the desktop menu's Properties item.
    DesktopIconView(Settings *settings, QAction *properties, QWidget *parent = nullptr);

    // The edges icons keep clear of (the taskbar), in logical pixels.
    void setMargins(const QMargins &margins);
    QRect iconArea() const;

    // The folder shown; desktopDirectory() unless changed (tests).
    void setDirectory(const QString &directory, const QString &trashFiles);

    // The icons in their order, and where each is.
    QList<DesktopItem> items() const;
    QPoint position(const QString &key) const;
    QRect itemRect(const QString &key) const;
    QStringList selectedKeys() const;

    QMenu *desktopMenu() const { return m_desktopMenu; }
    // The menu for the icon key, or the selection it is in (tests).
    QMenu *itemMenu(const QString &key);

    void arrange(int by); // ArrangeBy
    void lineUp();
    void open(const QString &key);

    enum ArrangeBy
    {
        ByName,
        ByType,
        BySize,
        ByDate
    };

public Q_SLOTS:
    // Reads the folder again (F5, or the file watcher).
    void reload();

Q_SIGNALS:
    // The folder has been read and the icons laid out.
    void loaded();
    // Properties was chosen on My Computer.
    void systemPropertiesRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Icon
    {
        DesktopItem item;
        QIcon icon;
        // The icon drawn at the desktop's device pixel ratio, with its
        // shortcut arrow; made when first painted at that ratio.
        mutable QPixmap image;
        mutable qreal imageRatio = 0;
        QPoint pos; // top left of its cell
        bool selected = false;
    };
    struct Listing
    {
        QList<DesktopItem> files;
        bool trashFull = false;
    };

    void startReload();
    void listingReady();
    void layoutIcons();
    void savePositions();
    int indexOf(const QString &key) const;
    int iconAt(const QPoint &point) const;
    QRect iconRect(const Icon &icon) const;
    QRect labelRect(const Icon &icon, QStringList *lines = nullptr, bool wholeName = false) const;
    bool showsFullLabel(int index) const;
    QPoint clampToArea(const QPoint &pos) const;
    void selectOnly(int index);
    void clearSelection();
    // What a change of selection, focus, drag or rubber band can touch: the
    // selected and focused icons with their names, and the rubber band.
    // Take it before the change and repaint it with the region after
    // (repaintState), not the whole desktop: at 200% on a software-drawn
    // screen each full-screen repaint is millions of pixels.
    QRegion stateRegion() const;
    void repaintState(const QRegion &before);
    void moveCurrent(int key);
    void showMenuFor(int index, const QPoint &globalPos);
    QList<int> selectedFiles() const;

    // Commands
    void openSelected();
    void cut();
    void copy();
    void paste(bool shortcut);
    void createShortcut();
    void deleteSelected();
    void startRename(int index);
    void finishRename(bool commit);
    void newFolder();
    void newTextDocument();
    void emptyRecycleBin();
    void placeNew(const QStringList &names);

    Settings *m_settings;
    QString m_directory;
    QString m_trashFiles;
    QMargins m_margins;
    QList<Icon> m_icons;
    QHash<QString, QPoint> m_positions; // placed by the user, by key
    QStringList m_pending; // placed, but not read from the folder yet
    bool m_autoArrange = false;
    int m_current = -1;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    QFutureWatcher<Listing> m_loader;
    bool m_reloadAgain = false;

    // Mouse
    int m_pressIndex = -1;
    QPoint m_pressPos;
    bool m_dragging = false;
    QPoint m_dragDelta;
    bool m_rubberBand = false;
    QRect m_rubberRect;
    QList<int> m_rubberBase; // selected before a Ctrl+rubber band

    // Where a new or pasted item goes (the desktop menu's position), if any.
    std::optional<QPoint> m_newItemPoint;
    QString m_renameWhenLoaded;
    QLineEdit *m_renameEditor = nullptr;
    QString m_renaming; // key

    QMenu *m_desktopMenu;
    QAction *m_autoArrangeAction;
    QAction *m_pasteAction;
    QAction *m_pasteShortcutAction;
};

} // namespace Daytona
