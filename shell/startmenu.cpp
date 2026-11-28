// SPDX-License-Identifier: GPL-3.0-or-later

#include "startmenu.h"

#include "applicationindex.h"
#include "bevel.h"
#include "clickcatcher.h"
#include "daytonastyle.h"
#include "recentdocuments.h"
#include "settings.h"
#include "systemlogo.h"

#include <LayerShellQt/Window>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMenu>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleOptionMenuItem>
#include <QWindow>

Q_LOGGING_CATEGORY(lcStartMenu, "daytona.startmenu", QtInfoMsg)

namespace Daytona {

namespace {

    constexpr int LargeIconSize = 32;
    constexpr int SmallIconSize = 16;
    constexpr int ReopenGuardMs = 250;

    // xdg_positioner constraint adjustments.
    constexpr int SlideY = 2;
    constexpr int FlipX = 4;

    QIcon themeIcon(const char *name)
    {
        const QIcon icon = QIcon::fromTheme(QString::fromLatin1(name));
        return icon.isNull() ? QIcon::fromTheme(QStringLiteral("application-x-executable")) : icon;
    }

    // Menu text uses '&' for mnemonics; application names must not create
    // mnemonics of their own.
    QString menuText(const QString &name)
    {
        QString text = name;
        text.replace(QLatin1Char('&'), QStringLiteral("&&"));
        return text;
    }

    QChar mnemonic(const QString &text)
    {
        for (int i = 0; i + 1 < text.size(); ++i) {
            if (text[i] == QLatin1Char('&')) {
                if (text[i + 1] != QLatin1Char('&'))
                    return text[i + 1].toLower();
                ++i;
            }
        }
        return {};
    }

    void selectFirstAction(QMenu *menu)
    {
        // Down in a menu with nothing highlighted highlights its first item, the
        // same as the keyboard would. QMenu::setActiveAction() would also open
        // that item's submenu straight away, which NT does not do.
        QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QCoreApplication::sendEvent(menu, &down);
    }

} // namespace

StartMenu::StartMenu(ApplicationIndex *index, Settings *settings, QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_index(index)
    , m_settings(settings)
    , m_bannerText(systemName())
{
    setAccessibleName(tr("Start menu"));
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_catcher = new ClickCatcher();
    connect(m_catcher, &ClickCatcher::clicked, this, &QWidget::hide);

    m_programs = new QMenu(this);
    m_recentFile = recentDocumentsFile();
    m_documents = new QMenu(this);
    // Read when shown, so the list is current without watching the file.
    connect(m_documents, &QMenu::aboutToShow, this, &StartMenu::rebuildDocuments);
    rebuildDocuments();

    m_settingsMenu = new QMenu(this);
    m_controlPanel = m_settingsMenu->addAction(themeIcon("preferences-system"),
        tr("&Control Panel"), this, &StartMenu::controlPanelRequested);
    m_settingsMenu->addAction(themeIcon("preferences-system-windows"), tr("&Taskbar..."), this,
        &StartMenu::taskbarPropertiesRequested);
    m_settingsMenu->addAction(themeIcon("preferences-desktop-display"), tr("&Display..."), this,
        &StartMenu::displayPropertiesRequested);
    m_settingsMenu->addAction(themeIcon("input-keyboard"), tr("&Keyboard..."), this,
        &StartMenu::keyboardPropertiesRequested);
    m_settingsMenu->addAction(
        themeIcon("input-mouse"), tr("&Mouse..."), this, &StartMenu::mousePropertiesRequested);

    m_findMenu = new QMenu(this);
    m_find = m_findMenu->addAction(themeIcon("folder-open"), tr("&Files or Folders..."), this,
        [this] { runCommand(findCommand(m_settings), tr("Find")); });

    m_items = {
        { themeIcon("applications-other"), tr("&Programs"), m_programs, {}, true, false },
        { themeIcon("folder-documents"), tr("&Documents"), m_documents, {}, true, false },
        { themeIcon("preferences-system"), tr("&Settings"), m_settingsMenu, {}, true, false },
        { themeIcon("system-search"), tr("&Find"), m_findMenu, {}, true, false },
        { themeIcon("system-run"), tr("&Run..."), nullptr, [this] { Q_EMIT runRequested(); }, true,
            false },
        { {}, {}, nullptr, {}, false, true },
        { themeIcon("system-shutdown"), tr("Sh&ut Down..."), nullptr,
            [this] { Q_EMIT shutDownRequested(); }, true, false },
    };

    for (QMenu *menu : { m_programs, m_documents, m_settingsMenu, m_findMenu }) {
        menu->installEventFilter(this);
        // Choosing anything, at any depth, closes the whole Start menu.
        connect(menu, &QMenu::triggered, this, &QWidget::hide);
        connect(menu, &QMenu::aboutToHide, this, [this, menu] {
            if (m_openSubmenu != menu)
                return;
            m_openSubmenu = nullptr;
            // Hidden by the compositor (a click outside every menu) rather
            // than by us or the keyboard: close the Start menu too, as one
            // click outside closes all of NT's menus.
            if (!m_closingSubmenu && isVisible())
                QTimer::singleShot(0, this, &QWidget::hide);
        });
    }

    m_submenuDelay.setSingleShot(true);
    m_submenuDelay.setInterval(
        style()->styleHint(QStyle::SH_Menu_SubMenuPopupDelay, nullptr, this));
    connect(&m_submenuDelay, &QTimer::timeout, this, [this] {
        if (m_current >= 0 && m_items[m_current].submenu)
            openSubmenu(m_current, false);
    });

    if (m_index)
        connect(m_index, &ApplicationIndex::changed, this, &StartMenu::rebuildPrograms);
    if (m_settings) {
        connect(m_settings, &Settings::changed, this, [this](const QString &key) {
            if (key.isEmpty() || key == Settings::FlatProgramsKey)
                rebuildPrograms();
        });
    }
    rebuildPrograms();
    updateCommandItems();
}

StartMenu::~StartMenu()
{
    delete m_catcher;
}

int StartMenu::iconSize() const
{
    return m_settings && m_settings->smallStartMenuIcons() ? SmallIconSize : LargeIconSize;
}

int StartMenu::itemHeight(const Item &item) const
{
    QStyleOptionMenuItem opt;
    opt.initFrom(this);
    opt.menuItemType
        = item.separator ? QStyleOptionMenuItem::Separator : QStyleOptionMenuItem::Normal;
    opt.maxIconWidth = iconSize();
    opt.text = item.text;
    return style()
        ->sizeFromContents(QStyle::CT_MenuItem, &opt, QSize(0, fontMetrics().height()), this)
        .height();
}

QSize StartMenu::sizeHint() const
{
    const int frame = style()->pixelMetric(QStyle::PM_MenuPanelWidth, nullptr, this);
    const int vmargin = style()->pixelMetric(QStyle::PM_MenuVMargin, nullptr, this);
    int width = 0;
    int height = 0;
    for (const Item &item : m_items) {
        QStyleOptionMenuItem opt;
        opt.initFrom(this);
        opt.menuItemType = item.submenu ? QStyleOptionMenuItem::SubMenu
            : item.separator            ? QStyleOptionMenuItem::Separator
                                        : QStyleOptionMenuItem::Normal;
        opt.maxIconWidth = iconSize();
        opt.text = item.text;
        const QSize contents(
            fontMetrics().horizontalAdvance(QString(item.text).remove(QLatin1Char('&'))),
            fontMetrics().height());
        const QSize size = style()->sizeFromContents(QStyle::CT_MenuItem, &opt, contents, this);
        width = std::max(width, size.width());
        height += size.height();
    }
    return { 2 * frame + BannerWidth + width + 8, 2 * (frame + vmargin) + height };
}

QRect StartMenu::itemRect(int index) const
{
    const int frame = style()->pixelMetric(QStyle::PM_MenuPanelWidth, nullptr, this);
    const int vmargin = style()->pixelMetric(QStyle::PM_MenuVMargin, nullptr, this);
    int y = frame + vmargin;
    for (int i = 0; i < index; ++i)
        y += itemHeight(m_items[i]);
    return { frame + BannerWidth, y, width() - 2 * frame - BannerWidth,
        itemHeight(m_items[index]) };
}

int StartMenu::itemAt(const QPoint &pos) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (!m_items[i].separator && itemRect(i).contains(pos))
            return i;
    }
    return -1;
}

void StartMenu::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QPalette &pal = palette();
    painter.fillRect(rect(), pal.window());
    Bevel::raised(&painter, rect(), pal);

    for (int i = 0; i < m_items.size(); ++i) {
        const Item &item = m_items[i];
        QStyleOptionMenuItem opt;
        opt.initFrom(this);
        opt.rect = itemRect(i);
        opt.text = item.text;
        opt.icon = item.icon;
        opt.maxIconWidth = iconSize();
        opt.font = font();
        opt.menuItemType = item.separator ? QStyleOptionMenuItem::Separator
            : item.submenu                ? QStyleOptionMenuItem::SubMenu
                                          : QStyleOptionMenuItem::Normal;
        opt.state = item.enabled ? QStyle::State_Enabled : QStyle::State_None;
        if (i == m_current && !item.separator)
            opt.state |= QStyle::State_Selected;
        style()->drawControl(QStyle::CE_MenuItem, &opt, &painter, this);
    }

    // NT's banner: a dark strip inside the frame with the product name
    // ("Windows NT") reading upwards from the bottom; here the operating
    // system's, cut short with "..." if the menu is too short for it.
    const int frame = style()->pixelMetric(QStyle::PM_MenuPanelWidth, nullptr, this);
    const QRect banner(frame, frame, BannerWidth, height() - 2 * frame);
    painter.fillRect(banner, pal.color(QPalette::Dark));
    QFont bannerFont = font();
    bannerFont.setBold(true);
    bannerFont.setPointSizeF(bannerFont.pointSizeF() * 1.5);
    painter.setFont(bannerFont);
    painter.setPen(pal.color(QPalette::Light));
    painter.translate(banner.left(), banner.bottom());
    painter.rotate(-90);
    const QRect text(4, 0, banner.height() - 8, banner.width());
    painter.drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
        painter.fontMetrics().elidedText(m_bannerText, Qt::ElideRight, text.width()));
}

void StartMenu::setCurrent(int index)
{
    if (index == m_current)
        return;
    m_current = index;
    update();
}

void StartMenu::moveCurrent(int step)
{
    if (m_items.isEmpty())
        return;
    int index = m_current;
    for (int n = 0; n < m_items.size(); ++n) {
        index = (index + step + int(m_items.size())) % int(m_items.size());
        if (index < 0)
            index = 0;
        if (!m_items[index].separator)
            break;
    }
    setCurrent(index);
}

void StartMenu::mouseMoveEvent(QMouseEvent *event)
{
    hoverAt(event->position().toPoint());
}

void StartMenu::hoverAt(const QPoint &pos)
{
    const int index = itemAt(pos);
    if (index < 0 || index == m_current)
        return;
    setCurrent(index);
    if (m_openSubmenu && m_openSubmenu != m_items[index].submenu)
        closeSubmenu();
    if (m_items[index].submenu && m_items[index].enabled)
        m_submenuDelay.start();
}

void StartMenu::mousePressEvent(QMouseEvent *event)
{
    pressAt(event->position().toPoint());
}

void StartMenu::mouseReleaseEvent(QMouseEvent *event)
{
    releaseAt(event->position().toPoint());
}

void StartMenu::pressAt(const QPoint &pos)
{
    const int index = itemAt(pos);
    if (index >= 0 && m_items[index].submenu) {
        setCurrent(index);
        activate(index, false);
    }
}

void StartMenu::releaseAt(const QPoint &pos)
{
    const int index = itemAt(pos);
    if (index >= 0 && !m_items[index].submenu)
        activate(index, false);
}

void StartMenu::leaveEvent(QEvent *)
{
    // Left the menu before a submenu opened: it no longer opens.
    m_submenuDelay.stop();
    if (!m_openSubmenu)
        setCurrent(-1);
}

void StartMenu::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        moveCurrent(-1);
        return;
    case Qt::Key_Down:
        moveCurrent(1);
        return;
    case Qt::Key_Right:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        if (m_current >= 0)
            activate(m_current, true);
        return;
    case Qt::Key_Escape:
        hide();
        return;
    default:
        break;
    }
    // Mnemonics: the underlined letter chooses its item.
    const QString typed = event->text().toLower();
    if (typed.size() == 1) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (!m_items[i].separator && mnemonic(m_items[i].text) == typed[0]) {
                setCurrent(i);
                activate(i, true);
                return;
            }
        }
    }
    QWidget::keyPressEvent(event);
}

void StartMenu::activate(int index, bool fromKeyboard)
{
    if (index < 0 || index >= m_items.size() || !m_items[index].enabled)
        return;
    const Item &item = m_items[index];
    if (item.submenu) {
        openSubmenu(index, fromKeyboard);
        return;
    }
    const std::function<void()> action = item.action;
    hide();
    if (action)
        action();
}

void StartMenu::openSubmenu(int index, bool selectFirst)
{
    QMenu *menu = m_items[index].submenu;
    if (!menu)
        return;
    m_submenuDelay.stop();
    if (m_openSubmenu != menu) {
        closeSubmenu();
        // Let the compositor place the submenu against its item: to the
        // right, flipped left or slid up if it would leave the screen. Qt's
        // own idea of global positions is not reliable for layer surfaces.
        menu->winId();
        const QRect anchor = itemRect(index);
        QWindow *window = menu->windowHandle();
        window->setProperty("_q_waylandPopupAnchorRect", anchor);
        window->setProperty(
            "_q_waylandPopupAnchor", QVariant::fromValue(Qt::Edges(Qt::TopEdge | Qt::RightEdge)));
        window->setProperty("_q_waylandPopupGravity",
            QVariant::fromValue(Qt::Edges(Qt::BottomEdge | Qt::RightEdge)));
        window->setProperty("_q_waylandPopupConstraintAdjustment", SlideY | FlipX);
        m_openSubmenu = menu;
        menu->popup(mapToGlobal(anchor.topRight()));
    }
    if (selectFirst && !menu->activeAction())
        selectFirstAction(menu);
}

void StartMenu::closeSubmenu()
{
    if (!m_openSubmenu)
        return;
    m_closingSubmenu = true;
    QMenu *menu = m_openSubmenu;
    m_openSubmenu = nullptr;
    menu->hide();
    m_closingSubmenu = false;
}

bool StartMenu::eventFilter(QObject *watched, QEvent *event)
{
    // While a submenu is open Qt gives it every mouse event, including those
    // over the Start menu (submenus pass on what is outside them to the menu
    // that opened them, ending at a first-level submenu). Take back the ones
    // over the Start menu, so pointing at another item there highlights it
    // and opens its submenu, and clicking there works, as in NT.
    if ((event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease)
        && m_openSubmenu && isVisible()) {
        auto *menu = static_cast<QWidget *>(watched);
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (!menu->rect().contains(mouse->position().toPoint())) {
            const QPoint pos = mapFromGlobal(mouse->globalPosition()).toPoint();
            if (rect().contains(pos)) {
                if (event->type() == QEvent::MouseMove)
                    hoverAt(pos);
                else if (event->type() == QEvent::MouseButtonPress)
                    pressAt(pos);
                else
                    releaseAt(pos);
                return true;
            }
        }
    }
    // Escape or Left in a first-level submenu returns to the Start menu.
    if (event->type() == QEvent::KeyPress && watched == m_openSubmenu) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape || key == Qt::Key_Left) {
            closeSubmenu();
            setFocus();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void StartMenu::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_submenuDelay.stop();
    closeSubmenu();
    m_catcher->hide();
    m_current = -1;
    m_closedAt.start();
    if (m_rebuildPending)
        rebuildPrograms();
    Q_EMIT closed();
}

void StartMenu::updateCommandItems()
{
    // Find runs a command from the settings file, else the file manager's
    // search, and is greyed out when there is neither.
    m_find->setEnabled(!findCommand(m_settings).isEmpty());
}

QString StartMenu::findCommand(const Settings *settings)
{
    const QString configured = settings ? settings->findCommand() : QString();
    if (!configured.isEmpty())
        return configured;
    // PCManFM-Qt's Find Files, looking in the home folder (NT's Find looked
    // in the local drives).
    if (!QStandardPaths::findExecutable(QStringLiteral("pcmanfm-qt")).isEmpty())
        return QStringLiteral("pcmanfm-qt --find-files \"%1\"").arg(QDir::homePath());
    return {};
}

void StartMenu::rebuildPrograms()
{
    if (isVisible()) {
        // Never rebuild a menu the user is looking at.
        m_rebuildPending = true;
        return;
    }
    m_rebuildPending = false;
    m_programs->clear();
    const MenuNode programs = m_index ? m_index->programs() : MenuNode();
    if (m_settings && m_settings->flatProgramsMenu()) {
        // Every program in one list, without folders.
        MenuNode flat;
        for (const DesktopEntry &entry : flattenMenu(programs)) {
            MenuNode item;
            item.kind = MenuNode::Kind::Entry;
            item.entry = entry;
            flat.children.append(item);
        }
        addPrograms(m_programs, flat);
    } else {
        addPrograms(m_programs, programs);
    }
    if (m_programs->isEmpty()) {
        QAction *none = m_programs->addAction(tr("(Empty)"));
        none->setEnabled(false);
    }
}

void StartMenu::addPrograms(QMenu *menu, const MenuNode &folder)
{
    for (const MenuNode &item : folder.children) {
        switch (item.kind) {
        case MenuNode::Kind::Folder:
            // Every folder has NT's program-group icon, whatever its
            // .directory file says.
            addPrograms(menu->addMenu(themeIcon("applications-other"), menuText(item.name)), item);
            break;
        case MenuNode::Kind::Entry: {
            const DesktopEntry entry = item.entry;
            QIcon icon = QIcon::fromTheme(entry.icon);
            if (icon.isNull())
                icon = themeIcon("application-x-executable");
            menu->addAction(icon, menuText(entry.name), this, [entry] { launchEntry(entry); });
            break;
        }
        case MenuNode::Kind::Separator:
            menu->addSeparator();
            break;
        }
    }
}

void StartMenu::rebuildDocuments()
{
    m_documents->clear();
    static const QMimeDatabase mimeDatabase;
    const QList<RecentDocument> documents = readRecentDocuments(m_recentFile);
    for (const RecentDocument &document : documents) {
        const QMimeType type = mimeDatabase.mimeTypeForFile(document.path);
        QIcon icon = QIcon::fromTheme(type.iconName());
        if (icon.isNull())
            icon = QIcon::fromTheme(
                type.genericIconName(), QIcon::fromTheme(QStringLiteral("text-x-generic")));
        const QString path = document.path;
        m_documents->addAction(icon, menuText(QFileInfo(path).fileName()), this, [path] {
            if (!QProcess::startDetached(QStringLiteral("xdg-open"), { path }))
                qCWarning(lcStartMenu) << "Could not open" << path;
        });
    }
    if (documents.isEmpty()) {
        QAction *empty = m_documents->addAction(tr("(Empty)"));
        empty->setEnabled(false);
    }
}

void StartMenu::launchEntry(const DesktopEntry &entry)
{
    qCDebug(lcStartMenu) << "launching" << entry.id;
    const LaunchResult result = launch(entry);
    if (!result.ok) {
        qCWarning(lcStartMenu) << "Could not start" << entry.id << result.error;
        QMessageBox::critical(nullptr, entry.name,
            tr("Cannot start '%1'.\n\nMake sure the program is installed correctly.")
                .arg(entry.name));
    }
}

void StartMenu::runCommand(const QString &command, const QString &what)
{
    // A configured command line, split into arguments; never run through a
    // shell.
    QStringList args = QProcess::splitCommand(command);
    if (args.isEmpty())
        return;
    const QString program = args.takeFirst();
    if (!QProcess::startDetached(program, args)) {
        qCWarning(lcStartMenu) << "Could not start" << what << "command" << program;
        QMessageBox::critical(nullptr, what, tr("Cannot start '%1'.").arg(program));
    }
}

void StartMenu::attachToLayerShell()
{
    create();
    auto *layer = LayerShellQt::Window::get(windowHandle());
    layer->setScope(QStringLiteral("daytona-start-menu"));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    // Opened from the Start button's corner of the taskbar, towards the screen:
    // up from a bottom taskbar, down from a top one, sideways from a vertical
    // one (from the top, where the Start button is).
    using A = LayerShellQt::Window;
    switch (m_edge) {
    case TaskbarEdge::Bottom:
        layer->setAnchors(A::Anchors(A::AnchorBottom | A::AnchorLeft));
        break;
    case TaskbarEdge::Top:
    case TaskbarEdge::Left:
        layer->setAnchors(A::Anchors(A::AnchorTop | A::AnchorLeft));
        break;
    case TaskbarEdge::Right:
        layer->setAnchors(A::Anchors(A::AnchorTop | A::AnchorRight));
        break;
    }
    // Zero: sit inside the usable area, i.e. right next to the taskbar.
    layer->setExclusiveZone(0);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    m_attached = true;
}

void StartMenu::popUpFrom(QWidget *anchor, bool fromKeyboard)
{
    // LayerShellQt (6.3) does not reliably keep a window a layer surface
    // once it has been hidden, so start from a fresh native window each time
    // the menu opens. It is cheap: the menu opens rarely.
    if (m_attached) {
        destroy(true, false);
        m_attached = false;
    }
    updateCommandItems();
    resize(sizeHint());
    create();
    // On the taskbar's output.
    if (anchor->screen())
        windowHandle()->setScreen(anchor->screen());
    attachToLayerShell();
    auto *layer = LayerShellQt::Window::get(windowHandle());
    // Line up with the Start button along the taskbar.
    const QPoint along = anchor->mapTo(anchor->window(), QPoint(0, 0));
    if (m_edge == TaskbarEdge::Left || m_edge == TaskbarEdge::Right)
        layer->setMargins(QMargins(0, along.y(), 0, 0));
    else
        layer->setMargins(QMargins(along.x(), 0, 0, 0));
    m_catcher->cover(anchor);
    m_current = fromKeyboard ? 0 : -1;
    // Exclusive keyboard interactivity gives the surface the keyboard; no
    // activation request is needed.
    show();
    setFocus();
}

bool StartMenu::justClosed() const
{
    return m_closedAt.isValid() && m_closedAt.elapsed() < ReopenGuardMs;
}

} // namespace Daytona
