// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktopicons.h"

#include "applicationindex.h"
#include "daytonastyle.h"
#include "desktopentry.h"
#include "settings.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleOptionFocusRect>
#include <QTextLayout>
#include <QtConcurrent>
#include <algorithm>

Q_LOGGING_CATEGORY(lcDesktopIcons, "daytona.desktop.icons", QtInfoMsg)

namespace Daytona {

namespace {

    constexpr int Cell = DesktopIconView::CellSize;
    constexpr int IconPx = DesktopIconView::IconSize;
    // Text in a label wraps at this width: a little wider than the spacing,
    // as today's fonts are wider than MS Sans Serif, so "Network
    // Neighborhood" still fits on two lines. Unselected labels show at most
    // two lines.
    constexpr int LabelTextWidth = Cell + 4;
    constexpr int MaxLabelLines = 2;

    QStringList wrapLabel(
        const QString &text, const QFont &font, QPaintDevice *device, int width, int maxLines)
    {
        QTextLayout layout(text, font, device);
        QTextOption option(Qt::AlignHCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        layout.setTextOption(option);
        const QFontMetrics metrics(font, device);
        QStringList lines;
        layout.beginLayout();
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(width);
            if (maxLines > 0 && lines.size() == maxLines - 1) {
                const QString rest = text.mid(line.textStart());
                lines.append(metrics.elidedText(rest.trimmed(), Qt::ElideRight, width));
                break;
            }
            lines.append(text.mid(line.textStart(), line.textLength()).trimmed());
        }
        layout.endLayout();
        if (lines.isEmpty())
            lines.append(QString());
        return lines;
    }

    // A theme icon, an icon file, or a fallback.
    QIcon iconFor(const DesktopItem &item)
    {
        if (QDir::isAbsolutePath(item.iconName)) {
            if (QFileInfo::exists(item.iconName))
                return QIcon(item.iconName);
        } else if (!item.iconName.isEmpty() && QIcon::hasThemeIcon(item.iconName)) {
            return QIcon::fromTheme(item.iconName);
        }
        return QIcon::fromTheme(
            item.fallbackIconName, QIcon::fromTheme(QStringLiteral("text-x-generic")));
    }

    // Opens one of the file manager's own locations: PCManFM-Qt's
    // computer:///, network:/// and trash:///, else fallback (a folder)
    // with xdg-open.
    bool openLocation(const QString &uri, const QString &fallback)
    {
        const QString fileManager = QStandardPaths::findExecutable(QStringLiteral("pcmanfm-qt"));
        if (!fileManager.isEmpty())
            return QProcess::startDetached(fileManager, { uri });
        if (fallback.isEmpty())
            return false;
        return QProcess::startDetached(QStringLiteral("xdg-open"), { fallback });
    }

    int systemOrder(DesktopItem::Kind kind)
    {
        return static_cast<int>(kind);
    }

} // namespace

QPoint desktopGridCell(const QRect &area, int index)
{
    const int rows = std::max(1, area.height() / Cell);
    return area.topLeft() + QPoint((index / rows) * Cell, (index % rows) * Cell);
}

QPoint nearestGridCell(const QRect &area, const QPoint &point)
{
    const int rows = std::max(1, area.height() / Cell);
    const int columns = std::max(1, area.width() / Cell);
    const QPoint offset = point - area.topLeft();
    const int column = std::clamp((offset.x() + Cell / 2) / Cell, 0, columns - 1);
    const int row = std::clamp((offset.y() + Cell / 2) / Cell, 0, rows - 1);
    return area.topLeft() + QPoint(column * Cell, row * Cell);
}

DesktopIconView::DesktopIconView(Settings *settings, QAction *properties, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_desktopMenu(new QMenu(this))
{
    setObjectName(QStringLiteral("desktop-icons"));
    setFocusPolicy(Qt::StrongFocus);
    if (m_settings) {
        m_positions = m_settings->desktopIconPositions();
        m_autoArrange = m_settings->desktopAutoArrange();
    }

    // NT's desktop menu.
    QMenu *arrangeMenu = m_desktopMenu->addMenu(tr("Arrange &Icons"));
    arrangeMenu->addAction(tr("by &Name"), this, [this] { arrange(ByName); });
    arrangeMenu->addAction(tr("by &Type"), this, [this] { arrange(ByType); });
    arrangeMenu->addAction(tr("by Si&ze"), this, [this] { arrange(BySize); });
    arrangeMenu->addAction(tr("by &Date"), this, [this] { arrange(ByDate); });
    arrangeMenu->addSeparator();
    m_autoArrangeAction = arrangeMenu->addAction(tr("&Auto Arrange"));
    m_autoArrangeAction->setCheckable(true);
    m_autoArrangeAction->setChecked(m_autoArrange);
    connect(m_autoArrangeAction, &QAction::toggled, this, [this](bool on) {
        m_autoArrange = on;
        if (m_settings)
            m_settings->setDesktopAutoArrange(on);
        layoutIcons();
    });
    m_desktopMenu->addAction(tr("Line &up Icons"), this, &DesktopIconView::lineUp);
    m_desktopMenu->addSeparator();
    m_pasteAction = m_desktopMenu->addAction(tr("&Paste"), this, [this] { paste(false); });
    m_pasteShortcutAction
        = m_desktopMenu->addAction(tr("Paste &Shortcut"), this, [this] { paste(true); });
    m_desktopMenu->addSeparator();
    QMenu *newMenu = m_desktopMenu->addMenu(tr("Ne&w"));
    newMenu->addAction(QIcon::fromTheme(QStringLiteral("folder")), tr("&Folder"), this,
        &DesktopIconView::newFolder);
    newMenu->addSeparator();
    newMenu->addAction(QIcon::fromTheme(QStringLiteral("text-x-generic")), tr("&Text Document"),
        this, &DesktopIconView::newTextDocument);
    m_desktopMenu->addSeparator();
    if (properties)
        m_desktopMenu->addAction(properties);
    connect(m_desktopMenu, &QMenu::aboutToShow, this, [this] {
        const bool canPaste
            = !readFileClipboard(QGuiApplication::clipboard()->mimeData()).urls.isEmpty();
        m_pasteAction->setEnabled(canPaste);
        m_pasteShortcutAction->setEnabled(canPaste);
    });
    connect(m_desktopMenu, &QMenu::aboutToHide, this, [this] {
        // The chosen action runs after this; forget the point only later.
        QTimer::singleShot(0, this, [this] { m_newItemPoint.reset(); });
    });

    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(100);
    connect(&m_reloadTimer, &QTimer::timeout, this, &DesktopIconView::startReload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reloadTimer,
        qOverload<>(&QTimer::start));
    connect(&m_loader, &QFutureWatcher<Listing>::finished, this, &DesktopIconView::listingReady);

    setDirectory(desktopDirectory(), trashFilesDirectory());
}

void DesktopIconView::setDirectory(const QString &directory, const QString &trashFiles)
{
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_directory = directory;
    m_trashFiles = trashFiles;
    // NT always had a Desktop folder; so do freedesktop desktops. The trash
    // folder is made by whatever first trashes something, so make it now to
    // be able to watch it.
    QDir().mkpath(m_directory);
    QDir().mkpath(m_trashFiles);
    m_watcher.addPath(m_directory);
    m_watcher.addPath(m_trashFiles);
    reload();
}

void DesktopIconView::setMargins(const QMargins &margins)
{
    if (margins == m_margins)
        return;
    m_margins = margins;
    layoutIcons();
}

QRect DesktopIconView::iconArea() const
{
    return rect().marginsRemoved(m_margins);
}

QList<DesktopItem> DesktopIconView::items() const
{
    QList<DesktopItem> items;
    for (const Icon &icon : m_icons)
        items.append(icon.item);
    return items;
}

QPoint DesktopIconView::position(const QString &key) const
{
    const int index = indexOf(key);
    return index < 0 ? QPoint() : m_icons.at(index).pos;
}

QRect DesktopIconView::itemRect(const QString &key) const
{
    const int index = indexOf(key);
    if (index < 0)
        return {};
    return iconRect(m_icons.at(index)).united(labelRect(m_icons.at(index)));
}

QStringList DesktopIconView::selectedKeys() const
{
    QStringList keys;
    for (const Icon &icon : m_icons) {
        if (icon.selected)
            keys.append(icon.item.key());
    }
    return keys;
}

void DesktopIconView::reload()
{
    m_reloadTimer.stop();
    startReload();
}

void DesktopIconView::startReload()
{
    if (m_loader.isRunning()) {
        m_reloadAgain = true;
        return;
    }
    const QString directory = m_directory;
    const QString trashFiles = m_trashFiles;
    const QString locale = messagesLocale();
    m_loader.setFuture(QtConcurrent::run([directory, trashFiles, locale] {
        Listing listing;
        listing.files = readDesktopFolder(directory, locale);
        listing.trashFull = trashIsFull(trashFiles);
        return listing;
    }));
}

void DesktopIconView::listingReady()
{
    if (m_reloadAgain) {
        m_reloadAgain = false;
        startReload();
        return;
    }
    const Listing listing = m_loader.result();
    const QString currentKey = m_current >= 0 && m_current < m_icons.size()
        ? m_icons.at(m_current).item.key()
        : QString();
    QStringList previousOrder;
    for (const Icon &icon : std::as_const(m_icons))
        previousOrder.append(icon.item.key());
    const QStringList selected = selectedKeys();

    QList<DesktopItem> items;
    for (DesktopItem::Kind kind : { DesktopItem::Kind::Computer, DesktopItem::Kind::Network,
             DesktopItem::Kind::RecycleBin }) {
        DesktopItem item = DesktopItem::system(kind);
        if (kind == DesktopItem::Kind::RecycleBin && listing.trashFull)
            item.iconName = QStringLiteral("user-trash-full");
        items.append(item);
    }
    // Icons already shown keep their order (for Auto Arrange); new ones go
    // after them, by name.
    QList<DesktopItem> files = listing.files;
    std::stable_sort(
        files.begin(), files.end(), [&previousOrder](const DesktopItem &a, const DesktopItem &b) {
            const qsizetype ia = previousOrder.indexOf(a.key());
            const qsizetype ib = previousOrder.indexOf(b.key());
            if (ia < 0 || ib < 0)
                return ia >= 0 && ib < 0;
            return ia < ib;
        });
    items.append(files);

    m_icons.clear();
    m_current = -1;
    for (const DesktopItem &item : std::as_const(items)) {
        Icon icon;
        icon.item = item;
        icon.icon = iconFor(item);
        icon.selected = selected.contains(item.key());
        if (item.key() == currentKey)
            m_current = int(m_icons.size());
        m_icons.append(icon);
    }
    layoutIcons();
    // Newly made icons that have now appeared keep the place they were given.
    const qsizetype pending = m_pending.size();
    m_pending.removeIf([this](const QString &key) { return indexOf(key) >= 0; });
    if (m_pending.size() != pending)
        savePositions();

    if (!m_renameWhenLoaded.isEmpty()) {
        const int index = indexOf(m_renameWhenLoaded);
        m_renameWhenLoaded.clear();
        if (index >= 0) {
            selectOnly(index);
            startRename(index);
        }
    }
    Q_EMIT loaded();
}

void DesktopIconView::layoutIcons()
{
    const QRect area = iconArea();
    QList<QRect> used;
    QList<int> unplaced;
    for (int i = 0; i < m_icons.size(); ++i) {
        Icon &icon = m_icons[i];
        const auto saved = m_positions.constFind(icon.item.key());
        if (m_autoArrange || saved == m_positions.cend()) {
            unplaced.append(i);
            continue;
        }
        icon.pos = clampToArea(*saved);
        used.append(QRect(icon.pos, QSize(Cell, Cell)));
    }
    // The rest fill free grid cells, down each column from the top left.
    int cell = 0;
    for (int i : std::as_const(unplaced)) {
        QPoint pos;
        for (;; ++cell) {
            pos = desktopGridCell(area, cell);
            const QRect candidate(pos + QPoint(1, 1), QSize(Cell - 2, Cell - 2));
            const bool free = std::none_of(used.cbegin(), used.cend(),
                [&candidate](const QRect &r) { return r.intersects(candidate); });
            // Past the right edge, stack in the last cell rather than lose it.
            if (free || pos.x() + Cell > area.right() + 1)
                break;
        }
        m_icons[i].pos = clampToArea(pos);
        ++cell;
    }
    update();

    // An icon stays where it first appeared, as in NT, so it is not moved by
    // others coming and going. Only once the desktop has its real size, so
    // the grid is the final one.
    if (m_autoArrange || !screen() || size() != screen()->size())
        return;
    bool added = false;
    for (int i : std::as_const(unplaced)) {
        if (!m_positions.contains(m_icons.at(i).item.key())) {
            m_positions.insert(m_icons.at(i).item.key(), m_icons.at(i).pos);
            added = true;
        }
    }
    if (added)
        savePositions();
}

void DesktopIconView::savePositions()
{
    // Only icons that are still there; a file renamed or removed elsewhere
    // takes its place with it.
    QHash<QString, QPoint> kept;
    for (const Icon &icon : std::as_const(m_icons)) {
        const QString key = icon.item.key();
        if (m_positions.contains(key))
            kept.insert(key, m_positions.value(key));
    }
    for (const QString &key : std::as_const(m_pending)) {
        if (m_positions.contains(key))
            kept.insert(key, m_positions.value(key));
    }
    m_positions = kept;
    if (m_settings)
        m_settings->setDesktopIconPositions(m_positions);
}

int DesktopIconView::indexOf(const QString &key) const
{
    for (int i = 0; i < m_icons.size(); ++i) {
        if (m_icons.at(i).item.key() == key)
            return i;
    }
    return -1;
}

QPoint DesktopIconView::clampToArea(const QPoint &pos) const
{
    const QRect area = iconArea();
    return { std::clamp(pos.x(), area.left(), std::max(area.left(), area.right() + 1 - Cell)),
        std::clamp(pos.y(), area.top(), std::max(area.top(), area.bottom() + 1 - Cell)) };
}

QRect DesktopIconView::iconRect(const Icon &icon) const
{
    QPoint pos = icon.pos;
    if (m_dragging && icon.selected)
        pos = clampToArea(pos + m_dragDelta);
    return { pos + QPoint((Cell - IconPx) / 2, 2), QSize(IconPx, IconPx) };
}

bool DesktopIconView::showsFullLabel(int index) const
{
    // NT showed the whole name of the focused icon.
    return index == m_current && m_icons.at(index).selected && hasFocus();
}

QRect DesktopIconView::labelRect(const Icon &icon, QStringList *lines) const
{
    const int index = int(&icon - m_icons.constData());
    const QStringList text = wrapLabel(icon.item.name, font(), const_cast<DesktopIconView *>(this),
        LabelTextWidth, showsFullLabel(index) ? 0 : MaxLabelLines);
    const QFontMetrics metrics = fontMetrics();
    int width = 0;
    for (const QString &line : text)
        width = std::max(width, metrics.horizontalAdvance(line));
    if (lines)
        *lines = text;
    const QRect iconBox = iconRect(icon);
    const QSize size(width + 4, int(text.size()) * metrics.height() + 2);
    return { QPoint(iconBox.center().x() + 1 - size.width() / 2, iconBox.bottom() + 4), size };
}

int DesktopIconView::iconAt(const QPoint &point) const
{
    // Topmost (last drawn) first.
    if (m_current >= 0 && m_current < m_icons.size()
        && (iconRect(m_icons.at(m_current)).contains(point)
            || labelRect(m_icons.at(m_current)).contains(point))) {
        return m_current;
    }
    for (int i = int(m_icons.size()) - 1; i >= 0; --i) {
        const Icon &icon = m_icons.at(i);
        if (iconRect(icon).contains(point) || labelRect(icon).contains(point))
            return i;
    }
    return -1;
}

void DesktopIconView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    const QPalette pal = palette();
    const QColor desktop = Style::schemeColour(QStringLiteral("Background"));
    // NT drew labels on the desktop colour, in white or black, whichever
    // reads better.
    const QColor text = qGray(desktop.rgb()) < 128 ? Qt::white : Qt::black;
    const qreal ratio = devicePixelRatioF();
    const int lineHeight = fontMetrics().height();
    // Icons and label backgrounds first, then the names, so a long name
    // reaching into the next column is not covered by that column's label.
    const auto draw = [&](int i, bool names) {
        const Icon &icon = m_icons.at(i);
        QStringList lines;
        const QRect label = labelRect(icon, &lines);
        const QRect iconBox = iconRect(icon);
        if (!event->rect().intersects(label.united(iconBox)))
            return;
        if (names) {
            painter.setPen(icon.selected ? pal.color(QPalette::HighlightedText) : text);
            for (int l = 0; l < lines.size(); ++l) {
                painter.drawText(QRect(label.left(), label.top() + 1 + l * lineHeight,
                                     label.width(), lineHeight),
                    Qt::AlignHCenter | Qt::AlignVCenter, lines.at(l));
            }
            if (i == m_current && hasFocus()) {
                QStyleOptionFocusRect focus;
                focus.initFrom(this);
                focus.rect = label;
                focus.backgroundColor = icon.selected ? pal.color(QPalette::Highlight) : desktop;
                style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
            }
            return;
        }
        QPixmap pixmap = icon.icon.pixmap(QSize(IconPx, IconPx), ratio);
        if (icon.item.isLink) {
            // NT's shortcut arrow, in the bottom left corner.
            const QPixmap arrow = QIcon::fromTheme(QStringLiteral("emblem-symbolic-link"))
                                      .pixmap(QSize(IconPx / 2, IconPx / 2), ratio);
            if (!arrow.isNull()) {
                QPainter overlay(&pixmap);
                overlay.drawPixmap(QPoint(0, IconPx / 2), arrow);
            }
        }
        if (icon.selected) {
            // Selected icons are dithered with the highlight colour, as NT.
            QPainter tint(&pixmap);
            tint.setCompositionMode(QPainter::CompositionMode_SourceAtop);
            tint.fillRect(QRect(QPoint(0, 0), pixmap.deviceIndependentSize().toSize()),
                QBrush(pal.color(QPalette::Highlight), Qt::Dense4Pattern));
        }
        painter.drawPixmap(iconBox.topLeft(), pixmap);
        painter.fillRect(label, icon.selected ? pal.color(QPalette::Highlight) : desktop);
    };
    for (bool names : { false, true }) {
        for (int i = 0; i < m_icons.size(); ++i) {
            if (i != m_current)
                draw(i, names);
        }
    }
    // The focused icon last: its whole name may cover the icons below it.
    if (m_current >= 0 && m_current < m_icons.size()) {
        draw(m_current, false);
        draw(m_current, true);
    }
    if (m_rubberBand && !m_rubberRect.isEmpty()) {
        // NT's dotted selection rectangle.
        QStyleOptionFocusRect band;
        band.initFrom(this);
        band.rect = m_rubberRect;
        band.backgroundColor = desktop;
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &band, &painter, this);
    }
}

void DesktopIconView::selectOnly(int index)
{
    for (int i = 0; i < m_icons.size(); ++i)
        m_icons[i].selected = i == index;
    m_current = index;
    update();
}

void DesktopIconView::clearSelection()
{
    for (Icon &icon : m_icons)
        icon.selected = false;
    update();
}

void DesktopIconView::mousePressEvent(QMouseEvent *event)
{
    if (m_renameEditor)
        finishRename(true);
    const QPoint pos = event->position().toPoint();
    const int index = iconAt(pos);
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    if (event->button() == Qt::RightButton) {
        if (index >= 0 && !m_icons.at(index).selected)
            selectOnly(index);
        else if (index >= 0)
            m_current = index;
        else
            clearSelection();
        if (index < 0)
            m_newItemPoint = pos;
        showMenuFor(index, event->globalPosition().toPoint());
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }
    if (index >= 0) {
        if (ctrl)
            m_icons[index].selected = !m_icons.at(index).selected;
        else if (!m_icons.at(index).selected)
            selectOnly(index);
        m_current = index;
        m_pressIndex = index;
        m_pressPos = pos;
    } else {
        m_rubberBase.clear();
        if (ctrl) {
            for (int i = 0; i < m_icons.size(); ++i) {
                if (m_icons.at(i).selected)
                    m_rubberBase.append(i);
            }
        } else {
            clearSelection();
        }
        m_rubberBand = true;
        m_pressPos = pos;
        m_rubberRect = QRect();
    }
    update();
    event->accept();
}

void DesktopIconView::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (m_pressIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragging
            && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragging = m_icons.at(m_pressIndex).selected;
        }
        if (m_dragging) {
            m_dragDelta = pos - m_pressPos;
            update();
        }
    } else if (m_rubberBand) {
        m_rubberRect = QRect(m_pressPos, pos).normalized();
        for (int i = 0; i < m_icons.size(); ++i) {
            const Icon &icon = m_icons.at(i);
            const bool inBand = iconRect(icon).intersects(m_rubberRect)
                || labelRect(icon).intersects(m_rubberRect);
            m_icons[i].selected = inBand != m_rubberBase.contains(i);
            if (inBand)
                m_current = i;
        }
        update();
    }
    event->accept();
}

void DesktopIconView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        event->accept();
        return;
    }
    if (m_dragging) {
        // Each selected icon goes where it was dropped.
        QList<int> moved;
        for (int i = 0; i < m_icons.size(); ++i) {
            if (!m_icons.at(i).selected)
                continue;
            m_icons[i].pos = clampToArea(m_icons.at(i).pos + m_dragDelta);
            m_positions.insert(m_icons.at(i).item.key(), m_icons.at(i).pos);
            moved.append(i);
        }
        m_dragging = false;
        m_dragDelta = QPoint();
        if (m_autoArrange) {
            // Auto Arrange keeps the grid; the drop sets the new order.
            const QRect area = iconArea();
            const int rows = std::max(1, area.height() / Cell);
            const auto slot = [&area, rows](const QPoint &p) {
                const QPoint cell = nearestGridCell(area, p) - area.topLeft();
                return (cell.x() / Cell) * rows + cell.y() / Cell;
            };
            QList<Icon> icons = m_icons;
            std::stable_sort(icons.begin(), icons.end(),
                [&](const Icon &a, const Icon &b) { return slot(a.pos) < slot(b.pos); });
            const QString currentKey
                = m_current >= 0 ? m_icons.at(m_current).item.key() : QString();
            m_icons = icons;
            m_current = indexOf(currentKey);
            for (const Icon &icon : std::as_const(m_icons))
                m_positions.remove(icon.item.key());
            layoutIcons();
        }
        savePositions();
    } else if (m_pressIndex >= 0 && !(event->modifiers() & Qt::ControlModifier)) {
        selectOnly(m_pressIndex);
    }
    m_pressIndex = -1;
    m_rubberBand = false;
    m_rubberRect = QRect();
    update();
    event->accept();
}

void DesktopIconView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const int index = iconAt(event->position().toPoint());
    if (index >= 0) {
        selectOnly(index);
        open(m_icons.at(index).item.key());
    }
    event->accept();
}

void DesktopIconView::moveCurrent(int key)
{
    if (m_icons.isEmpty())
        return;
    if (m_current < 0 || m_current >= m_icons.size()) {
        selectOnly(0);
        return;
    }
    if (key == Qt::Key_Home || key == Qt::Key_End) {
        selectOnly(key == Qt::Key_Home ? 0 : int(m_icons.size()) - 1);
        return;
    }
    // The nearest icon in that direction, favouring ones in line.
    const QPoint from = m_icons.at(m_current).pos;
    int best = -1;
    qint64 bestScore = 0;
    for (int i = 0; i < m_icons.size(); ++i) {
        const QPoint d = m_icons.at(i).pos - from;
        int along = 0;
        int across = 0;
        switch (key) {
        case Qt::Key_Left:
            along = -d.x();
            across = d.y();
            break;
        case Qt::Key_Right:
            along = d.x();
            across = d.y();
            break;
        case Qt::Key_Up:
            along = -d.y();
            across = d.x();
            break;
        case Qt::Key_Down:
            along = d.y();
            across = d.x();
            break;
        default:
            return;
        }
        if (along <= 0)
            continue;
        const qint64 score = qint64(along) * along + 4 * qint64(across) * across;
        if (best < 0 || score < bestScore) {
            best = i;
            bestScore = score;
        }
    }
    if (best >= 0)
        selectOnly(best);
}

void DesktopIconView::keyPressEvent(QKeyEvent *event)
{
    const bool ctrl = event->modifiers() & Qt::ControlModifier;
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Home:
    case Qt::Key_End:
        moveCurrent(event->key());
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        openSelected();
        return;
    case Qt::Key_Delete:
        deleteSelected();
        return;
    case Qt::Key_F2:
        if (m_current >= 0)
            startRename(m_current);
        return;
    case Qt::Key_F5:
        reload();
        return;
    case Qt::Key_Menu:
        showMenuFor(m_current >= 0 && m_icons.at(m_current).selected ? m_current : -1,
            mapToGlobal(m_current >= 0 ? iconRect(m_icons.at(m_current)).center() : QPoint()));
        return;
    case Qt::Key_F10:
        if (event->modifiers() & Qt::ShiftModifier) {
            showMenuFor(m_current >= 0 && m_icons.at(m_current).selected ? m_current : -1,
                mapToGlobal(m_current >= 0 ? iconRect(m_icons.at(m_current)).center() : QPoint()));
            return;
        }
        break;
    default:
        break;
    }
    if (ctrl) {
        switch (event->key()) {
        case Qt::Key_A:
            for (Icon &icon : m_icons)
                icon.selected = true;
            update();
            return;
        case Qt::Key_C:
            copy();
            return;
        case Qt::Key_X:
            cut();
            return;
        case Qt::Key_V:
            paste(false);
            return;
        default:
            break;
        }
    }
    // Typing a letter selects the next icon whose name starts with it.
    const QString typed = event->text();
    if (!ctrl && typed.size() == 1 && typed.at(0).isPrint() && !m_icons.isEmpty()) {
        const int count = int(m_icons.size());
        for (int step = 1; step <= count; ++step) {
            const int i = (std::max(m_current, -1) + step + count) % count;
            if (m_icons.at(i).item.name.startsWith(typed, Qt::CaseInsensitive)) {
                selectOnly(i);
                return;
            }
        }
        return;
    }
    QWidget::keyPressEvent(event);
}

void DesktopIconView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update();
}

void DesktopIconView::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

void DesktopIconView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutIcons();
}

bool DesktopIconView::eventFilter(QObject *watched, QEvent *event)
{
    // Enter finishes a rename and Escape cancels it. Neither goes on to the
    // desktop (QLineEdit passes Enter on), where Enter would open the icon.
    if (watched == m_renameEditor && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter) {
            finishRename(key != Qt::Key_Escape);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QList<int> DesktopIconView::selectedFiles() const
{
    QList<int> files;
    for (int i = 0; i < m_icons.size(); ++i) {
        if (m_icons.at(i).selected && m_icons.at(i).item.isFile())
            files.append(i);
    }
    return files;
}

QMenu *DesktopIconView::itemMenu(const QString &key)
{
    const int index = indexOf(key);
    if (index < 0)
        return nullptr;
    const DesktopItem &item = m_icons.at(index).item;
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *openAction = menu->addAction(tr("&Open"), this, &DesktopIconView::openSelected);
    openAction->setObjectName(QStringLiteral("open"));
    QFont bold = openAction->font();
    bold.setBold(true);
    openAction->setFont(bold);
    menu->setDefaultAction(openAction);
    if (item.kind == DesktopItem::Kind::RecycleBin) {
        menu->addSeparator();
        QAction *empty
            = menu->addAction(tr("Empty Recycle &Bin"), this, &DesktopIconView::emptyRecycleBin);
        empty->setObjectName(QStringLiteral("empty"));
        empty->setEnabled(item.iconName == QLatin1String("user-trash-full"));
    }
    if (item.isFile()) {
        const bool one = selectedFiles().size() == 1;
        menu->addSeparator();
        menu->addAction(tr("Cu&t"), this, &DesktopIconView::cut)
            ->setObjectName(QStringLiteral("cut"));
        menu->addAction(tr("&Copy"), this, &DesktopIconView::copy)
            ->setObjectName(QStringLiteral("copy"));
        menu->addSeparator();
        menu->addAction(tr("Create &Shortcut"), this, &DesktopIconView::createShortcut)
            ->setObjectName(QStringLiteral("shortcut"));
        menu->addAction(tr("&Delete"), this, &DesktopIconView::deleteSelected)
            ->setObjectName(QStringLiteral("delete"));
        QAction *rename
            = menu->addAction(tr("Rena&me"), this, [this, index] { startRename(index); });
        rename->setObjectName(QStringLiteral("rename"));
        // A .desktop file's label is its Name, not its file name.
        rename->setEnabled(one && !item.isDesktopEntry);
    }
    return menu;
}

void DesktopIconView::showMenuFor(int index, const QPoint &globalPos)
{
    if (index < 0) {
        m_desktopMenu->popup(globalPos);
        return;
    }
    if (QMenu *menu = itemMenu(m_icons.at(index).item.key()))
        menu->popup(globalPos);
}

void DesktopIconView::open(const QString &key)
{
    const int index = indexOf(key);
    if (index < 0)
        return;
    const DesktopItem &item = m_icons.at(index).item;
    bool ok = true;
    switch (item.kind) {
    case DesktopItem::Kind::Computer:
        ok = openLocation(QStringLiteral("computer:///"), QDir::homePath());
        break;
    case DesktopItem::Kind::Network:
        ok = openLocation(QStringLiteral("network:///"), QString());
        break;
    case DesktopItem::Kind::RecycleBin:
        ok = openLocation(QStringLiteral("trash:///"), m_trashFiles);
        break;
    case DesktopItem::Kind::File:
        if (item.isDesktopEntry) {
            const std::optional<DesktopEntry> entry
                = DesktopEntry::parse(item.path, QFileInfo(item.path).fileName(), messagesLocale());
            if (entry && entry->type == QLatin1String("Application")) {
                ok = launch(*entry).ok;
                break;
            }
            if (entry && entry->type == QLatin1String("Link")) {
                // QSettings splits values at commas; join them back.
                QSettings file(item.path, QSettings::IniFormat);
                const QString url = file.value(QStringLiteral("Desktop Entry/URL"))
                                        .toStringList()
                                        .join(QLatin1Char(','));
                ok = !url.isEmpty() && QProcess::startDetached(QStringLiteral("xdg-open"), { url });
                break;
            }
        }
        ok = QProcess::startDetached(QStringLiteral("xdg-open"), { item.path });
        break;
    }
    if (!ok) {
        qCWarning(lcDesktopIcons) << "Cannot open" << item.name;
        QMessageBox::critical(nullptr, item.name,
            tr("Cannot open '%1'.\n\nMake sure the file manager (PCManFM-Qt) is installed.")
                .arg(item.name));
    }
}

void DesktopIconView::openSelected()
{
    for (const Icon &icon : std::as_const(m_icons)) {
        if (icon.selected)
            open(icon.item.key());
    }
}

void DesktopIconView::cut()
{
    QList<QUrl> urls;
    for (int i : selectedFiles())
        urls.append(QUrl::fromLocalFile(m_icons.at(i).item.path));
    if (!urls.isEmpty())
        QGuiApplication::clipboard()->setMimeData(fileClipboardData(urls, true));
}

void DesktopIconView::copy()
{
    QList<QUrl> urls;
    for (int i : selectedFiles())
        urls.append(QUrl::fromLocalFile(m_icons.at(i).item.path));
    if (!urls.isEmpty())
        QGuiApplication::clipboard()->setMimeData(fileClipboardData(urls, false));
}

void DesktopIconView::paste(bool shortcut)
{
    const ClipboardFiles files = readFileClipboard(QGuiApplication::clipboard()->mimeData());
    if (files.urls.isEmpty())
        return;
    if (shortcut) {
        QStringList paths;
        for (const QUrl &url : files.urls)
            paths.append(url.toLocalFile());
        placeNew(createShortcuts(paths, m_directory));
        return;
    }
    // Copying can take a while; do it off the UI thread. Progress, and
    // anything larger, belongs to the file manager.
    auto *watcher = new QFutureWatcher<PasteResult>(this);
    const std::optional<QPoint> point = m_newItemPoint;
    connect(watcher, &QFutureWatcher<PasteResult>::finished, this, [this, watcher, files, point] {
        const PasteResult result = watcher->result();
        watcher->deleteLater();
        m_newItemPoint = point;
        placeNew(result.created);
        m_newItemPoint.reset();
        // Cut items have moved; the clipboard no longer points at them.
        if (files.cut && result.errors.isEmpty())
            QGuiApplication::clipboard()->clear();
        if (!result.errors.isEmpty())
            QMessageBox::warning(nullptr, tr("Error Copying File"), result.errors.join(u'\n'));
    });
    const QString directory = m_directory;
    watcher->setFuture(QtConcurrent::run(
        [files, directory] { return pasteFiles(files.urls, directory, files.cut); }));
}

void DesktopIconView::placeNew(const QStringList &names)
{
    // Pasted at the menu's position, one under another, as NT did; from the
    // keyboard, in the next free places.
    if (m_newItemPoint) {
        QPoint pos = *m_newItemPoint - QPoint(Cell / 2, IconPx / 2);
        for (const QString &name : names) {
            m_positions.insert(name, clampToArea(pos));
            m_pending.append(name);
            pos.ry() += Cell;
        }
    }
    reload();
}

void DesktopIconView::createShortcut()
{
    QStringList paths;
    for (int i : selectedFiles())
        paths.append(m_icons.at(i).item.path);
    placeNew(createShortcuts(paths, m_directory));
}

void DesktopIconView::deleteSelected()
{
    const QList<int> files = selectedFiles();
    if (files.isEmpty())
        return;
    const bool one = files.size() == 1;
    const QString question = one
        ? tr("Are you sure you want to send '%1' to the Recycle Bin?")
              .arg(m_icons.at(files.first()).item.name)
        : tr("Are you sure you want to send these %1 items to the Recycle Bin?").arg(files.size());
    if (QMessageBox::question(nullptr,
            one ? tr("Confirm File Delete") : tr("Confirm Multiple File Delete"), question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
        != QMessageBox::Yes) {
        return;
    }
    QStringList failed;
    for (int i : files) {
        const DesktopItem &item = m_icons.at(i).item;
        if (QFile::moveToTrash(item.path))
            m_positions.remove(item.key());
        else
            failed.append(item.name);
    }
    if (!failed.isEmpty()) {
        QMessageBox::warning(nullptr, tr("Error Deleting File"),
            tr("Cannot delete %1: it could not be moved to the Recycle Bin.")
                .arg(failed.join(QStringLiteral(", "))));
    }
    reload();
}

void DesktopIconView::startRename(int index)
{
    if (index < 0 || index >= m_icons.size())
        return;
    const DesktopItem &item = m_icons.at(index).item;
    if (!item.isFile() || item.isDesktopEntry)
        return;
    finishRename(false);
    m_renaming = item.key();
    m_renameEditor = new QLineEdit(this);
    m_renameEditor->setObjectName(QStringLiteral("rename"));
    m_renameEditor->setText(QFileInfo(item.path).fileName());
    // The name without its extension is selected, as from Windows 2000.
    const int dot = item.isDir ? -1 : int(m_renameEditor->text().lastIndexOf(QLatin1Char('.')));
    m_renameEditor->setSelection(0, dot > 0 ? dot : int(m_renameEditor->text().size()));
    const QRect label = labelRect(m_icons.at(index));
    const int width = std::max(Cell, label.width() + 8);
    m_renameEditor->setGeometry(label.center().x() - width / 2, label.top() - 2, width,
        m_renameEditor->sizeHint().height());
    connect(m_renameEditor, &QLineEdit::editingFinished, this, [this] { finishRename(true); });
    m_renameEditor->installEventFilter(this);
    m_renameEditor->show();
    m_renameEditor->setFocus();
}

void DesktopIconView::finishRename(bool commit)
{
    if (!m_renameEditor)
        return;
    QLineEdit *editor = m_renameEditor;
    m_renameEditor = nullptr;
    const QString key = m_renaming;
    m_renaming.clear();
    const QString newName = editor->text().trimmed();
    editor->hide();
    editor->deleteLater();
    setFocus();
    const int index = indexOf(key);
    if (!commit || index < 0 || newName.isEmpty() || newName == key)
        return;
    if (newName.contains(QLatin1Char('/')) || newName == QLatin1String(".")
        || newName == QLatin1String("..")) {
        QMessageBox::warning(
            nullptr, tr("Rename"), tr("A filename cannot contain the character /, or be . or .."));
        return;
    }
    QDir dir(m_directory);
    if (QFileInfo::exists(dir.filePath(newName)) || QFileInfo(dir.filePath(newName)).isSymLink()) {
        QMessageBox::warning(nullptr, tr("Error Renaming File"),
            tr("Cannot rename %1: A file with the name you specified already exists. Specify a "
               "different filename.")
                .arg(key));
        return;
    }
    if (!dir.rename(key, newName)) {
        QMessageBox::warning(nullptr, tr("Error Renaming File"), tr("Cannot rename %1.").arg(key));
        return;
    }
    // The renamed icon stays where it is.
    m_positions.remove(key);
    m_positions.insert(newName, m_icons.at(index).pos);
    m_pending.append(newName);
    m_icons[index].item.path = dir.filePath(newName);
    m_icons[index].item.name = newName;
    savePositions();
    reload();
}

void DesktopIconView::newFolder()
{
    const QString name = uniqueName(
        m_directory, tr("New Folder"), QStringLiteral("%1 (2)"), QStringLiteral("%1 (%2)"));
    if (!QDir(m_directory).mkdir(name)) {
        QMessageBox::warning(
            nullptr, tr("Error Creating Folder"), tr("Cannot create the folder '%1'.").arg(name));
        return;
    }
    m_renameWhenLoaded = name;
    placeNew({ name });
}

void DesktopIconView::newTextDocument()
{
    const QString name = uniqueName(m_directory, tr("New Text Document.txt"),
        tr("New Text Document (2).txt"), tr("New Text Document (%2).txt"));
    QFile file(QDir(m_directory).filePath(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        QMessageBox::warning(
            nullptr, tr("Error Creating File"), tr("Cannot create the file '%1'.").arg(name));
        return;
    }
    file.close();
    m_renameWhenLoaded = name;
    placeNew({ name });
}

void DesktopIconView::emptyRecycleBin()
{
    if (QMessageBox::question(nullptr, tr("Confirm Multiple File Delete"),
            tr("Are you sure you want to delete all of the items in the Recycle Bin?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
        != QMessageBox::Yes) {
        return;
    }
    auto *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher] {
        const QStringList failed = watcher->result();
        watcher->deleteLater();
        if (!failed.isEmpty()) {
            QMessageBox::warning(nullptr, tr("Error Deleting File"),
                tr("Cannot delete %1.").arg(failed.join(QStringLiteral(", "))));
        }
        reload();
    });
    const QString trash = m_trashFiles;
    watcher->setFuture(QtConcurrent::run([trash] { return emptyTrash(trash); }));
}

void DesktopIconView::arrange(int by)
{
    // The system icons stay first, in NT's order; then the rest sorted.
    QList<Icon> system;
    QList<Icon> files;
    for (const Icon &icon : std::as_const(m_icons))
        (icon.item.isFile() ? files : system).append(icon);
    std::stable_sort(system.begin(), system.end(), [](const Icon &a, const Icon &b) {
        return systemOrder(a.item.kind) < systemOrder(b.item.kind);
    });
    const auto byName = [](const Icon &a, const Icon &b) {
        if (a.item.isDir != b.item.isDir)
            return a.item.isDir;
        return QString::localeAwareCompare(a.item.name, b.item.name) < 0;
    };
    std::stable_sort(files.begin(), files.end(), byName);
    switch (by) {
    case ByType:
        std::stable_sort(files.begin(), files.end(), [](const Icon &a, const Icon &b) {
            if (a.item.isDir != b.item.isDir)
                return a.item.isDir;
            return QString::localeAwareCompare(a.item.typeName, b.item.typeName) < 0;
        });
        break;
    case BySize:
        std::stable_sort(files.begin(), files.end(), [](const Icon &a, const Icon &b) {
            if (a.item.isDir != b.item.isDir)
                return a.item.isDir;
            return a.item.size < b.item.size;
        });
        break;
    case ByDate:
        std::stable_sort(files.begin(), files.end(),
            [](const Icon &a, const Icon &b) { return a.item.modified < b.item.modified; });
        break;
    default:
        break;
    }
    const QString currentKey = m_current >= 0 ? m_icons.at(m_current).item.key() : QString();
    m_icons = system + files;
    m_current = indexOf(currentKey);
    // Arranging places every icon afresh in the grid.
    m_positions.clear();
    layoutIcons();
    if (!m_autoArrange) {
        for (const Icon &icon : std::as_const(m_icons))
            m_positions.insert(icon.item.key(), icon.pos);
    }
    savePositions();
}

void DesktopIconView::lineUp()
{
    // Each icon moves to the nearest free grid cell, top left first.
    const QRect area = iconArea();
    QList<int> order;
    for (int i = 0; i < m_icons.size(); ++i)
        order.append(i);
    std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
        const QPoint pa = m_icons.at(a).pos;
        const QPoint pb = m_icons.at(b).pos;
        return pa.x() != pb.x() ? pa.x() < pb.x() : pa.y() < pb.y();
    });
    QList<QPoint> taken;
    const int rows = std::max(1, area.height() / Cell);
    const int columns = std::max(1, area.width() / Cell);
    for (int i : std::as_const(order)) {
        const QPoint want = m_icons.at(i).pos;
        QPoint best = nearestGridCell(area, want);
        if (taken.contains(best)) {
            qint64 bestDistance = -1;
            for (int c = 0; c < rows * columns; ++c) {
                const QPoint cell = desktopGridCell(area, c);
                if (taken.contains(cell))
                    continue;
                const QPoint d = cell - want;
                const qint64 distance = qint64(d.x()) * d.x() + qint64(d.y()) * d.y();
                if (bestDistance < 0 || distance < bestDistance) {
                    best = cell;
                    bestDistance = distance;
                }
            }
        }
        taken.append(best);
        m_icons[i].pos = best;
        m_positions.insert(m_icons.at(i).item.key(), best);
    }
    update();
    savePositions();
}

} // namespace Daytona
