// SPDX-License-Identifier: GPL-3.0-or-later

#include "controlpanel.h"

#include "applicationindex.h"

#include <QAction>
#include <QActionGroup>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QLoggingCategory>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QStatusBar>
#include <QVBoxLayout>
#include <algorithm>

Q_LOGGING_CATEGORY(lcControlPanel, "daytona.controlpanel", QtInfoMsg)

namespace Daytona {

namespace {

    constexpr int LargeIconSize = 32;
    constexpr int SmallIconSize = 16;
    constexpr int AppletRole = Qt::UserRole;

    QIcon appletIcon(const QString &name)
    {
        const QIcon icon = QIcon::fromTheme(name);
        return icon.isNull() ? QIcon::fromTheme(QStringLiteral("application-x-executable")) : icon;
    }

    bool byName(const ControlPanelApplet &a, const ControlPanelApplet &b)
    {
        return QString::localeAwareCompare(a.name, b.name) < 0;
    }

    // A group heading: its name over a sunken rule, as XP's Classic style
    // drew "Show in Groups" headings.
    QFrame *rule(QWidget *parent)
    {
        auto *line = new QFrame(parent);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        return line;
    }

} // namespace

QList<ControlPanelApplet> controlPanelApplets(const QList<DesktopEntry> &entries)
{
    const auto tr = [](const char *text) {
        return QCoreApplication::translate("Daytona::ControlPanel", text);
    };
    using B = ControlPanelApplet::Builtin;
    QList<ControlPanelApplet> builtins = {
        { tr("Date/Time"), tr("Changes the date, time and time zone."),
            QStringLiteral("preferences-system-time"), B::DateTime, {} },
        { tr("Display"),
            tr("Changes your desktop's colours, wallpaper, screen area and font size."),
            QStringLiteral("preferences-desktop-display"), B::Display, {} },
        { tr("Keyboard"), tr("Changes the keyboard layouts and how to switch between them."),
            QStringLiteral("input-keyboard"), B::Keyboard, {} },
        { tr("Mouse"), tr("Changes the mouse pointer scheme."), QStringLiteral("input-mouse"),
            B::Mouse, {} },
        // NT's Sounds; the mixer (pavucontrol-qt) is listed under
        // Applications by its own name.
        { tr("Sounds"), tr("Changes the sounds played for events and the sound theme."),
            QStringLiteral("preferences-desktop-sound"), B::Sounds, {} },
        { tr("System"),
            tr("Shows information about your computer, such as its processor and memory."),
            QStringLiteral("computer"), B::System, {} },
        { tr("Taskbar"), tr("Changes how the taskbar and Start menu look and behave."),
            QStringLiteral("preferences-system-windows"), B::Taskbar, {} },
    };
    std::sort(builtins.begin(), builtins.end(), byName);

    QList<ControlPanelApplet> applications;
    for (const DesktopEntry &entry : entries) {
        if (!entry.categories.contains(QLatin1String("Settings")))
            continue;
        applications.append({ entry.name, entry.comment, entry.icon, B::None, entry });
    }
    std::sort(applications.begin(), applications.end(), byName);
    return builtins + applications;
}

ControlPanel::ControlPanel(ApplicationIndex *index, QWidget *parent)
    : QMainWindow(parent)
    , m_index(index)
{
    setWindowTitle(tr("Control Panel"));
    setWindowIcon(appletIcon(QStringLiteral("preferences-system")));

    auto *content = new QWidget;
    // A folder window: the applets sit on the window (Base) colour.
    content->setBackgroundRole(QPalette::Base);
    content->setAutoFillBackground(true);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(2);

    QFont headingFont = font();
    headingFont.setBold(true);
    m_builtinHeading = new QLabel(tr("Daytona"), content);
    m_builtinHeading->setFont(headingFont);
    m_builtinView = createView();
    m_applicationHeading = new QLabel(tr("Applications"), content);
    m_applicationHeading->setFont(headingFont);
    m_applicationRule = rule(content);
    m_applicationView = createView();

    layout->addWidget(m_builtinHeading);
    layout->addWidget(rule(content));
    layout->addWidget(m_builtinView);
    layout->addSpacing(8);
    layout->addWidget(m_applicationHeading);
    layout->addWidget(m_applicationRule);
    layout->addWidget(m_applicationView);
    layout->addStretch(1);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(content);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->viewport()->installEventFilter(this);
    setCentralWidget(m_scroll);

    // File
    QMenu *file = menuBar()->addMenu(tr("&File"));
    m_open = file->addAction(tr("&Open"), this, &ControlPanel::openSelected);
    QFont bold = m_open->font();
    bold.setBold(true); // the default action, as in Explorer
    m_open->setFont(bold);
    file->addSeparator();
    file->addAction(tr("&Close"), this, &QWidget::close);

    // View
    QMenu *view = menuBar()->addMenu(tr("&View"));
    QAction *statusBarAction = view->addAction(tr("Status &Bar"));
    statusBarAction->setCheckable(true);
    statusBarAction->setChecked(true);
    view->addSeparator();
    auto *modes = new QActionGroup(this);
    m_largeIcons
        = view->addAction(tr("Lar&ge Icons"), this, [this] { setViewMode(ViewMode::LargeIcons); });
    m_smallIcons
        = view->addAction(tr("S&mall Icons"), this, [this] { setViewMode(ViewMode::SmallIcons); });
    m_list = view->addAction(tr("&List"), this, [this] { setViewMode(ViewMode::List); });
    for (QAction *action : { m_largeIcons, m_smallIcons, m_list }) {
        action->setCheckable(true);
        modes->addAction(action);
    }
    m_largeIcons->setChecked(true);
    view->addSeparator();
    QAction *refreshAction = view->addAction(tr("&Refresh"), this, &ControlPanel::refresh);
    refreshAction->setShortcut(QKeySequence(Qt::Key_F5));
    addAction(refreshAction);

    m_status = new QLabel;
    statusBar()->addWidget(m_status, 1);
    statusBar()->setSizeGripEnabled(true);
    connect(statusBarAction, &QAction::toggled, statusBar(), &QWidget::setVisible);

    if (m_index)
        connect(m_index, &ApplicationIndex::changed, this, &ControlPanel::refresh);

    setViewMode(ViewMode::LargeIcons);
    resize(460, 420);
    prepare();
}

QListWidget *ControlPanel::createView()
{
    auto *view = new QListWidget;
    view->setFrameShape(QFrame::NoFrame);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setMovement(QListView::Static);
    view->setResizeMode(QListView::Adjust);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setTextElideMode(Qt::ElideRight);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(view, &QListWidget::itemActivated, this, &ControlPanel::openItem);
    connect(
        view, &QListWidget::itemSelectionChanged, this, [this, view] { selectionChanged(view); });
    return view;
}

void ControlPanel::prepare()
{
    refresh();
}

void ControlPanel::refresh()
{
    const QString selected
        = selectedApplet() ? selectedApplet()->entry.id + selectedApplet()->name : QString();
    m_applets = controlPanelApplets(m_index ? m_index->entries() : QList<DesktopEntry>());
    m_builtinView->clear();
    m_applicationView->clear();
    for (int i = 0; i < m_applets.size(); ++i) {
        const ControlPanelApplet &applet = m_applets.at(i);
        QListWidget *view = applet.builtin == ControlPanelApplet::Builtin::None ? m_applicationView
                                                                                : m_builtinView;
        auto *item = new QListWidgetItem(appletIcon(applet.iconName), applet.name, view);
        item->setData(AppletRole, i);
        item->setToolTip(applet.comment);
        if (applet.entry.id + applet.name == selected)
            view->setCurrentItem(item);
    }
    const bool haveApplications = m_applicationView->count() > 0;
    m_applicationHeading->setVisible(haveApplications);
    m_applicationRule->setVisible(haveApplications);
    m_applicationView->setVisible(haveApplications);
    updateViewSizes();
    updateStatus();
}

void ControlPanel::setViewMode(ViewMode mode)
{
    m_mode = mode;
    m_largeIcons->setChecked(mode == ViewMode::LargeIcons);
    m_smallIcons->setChecked(mode == ViewMode::SmallIcons);
    m_list->setChecked(mode == ViewMode::List);

    const QFontMetrics metrics(font());
    for (QListWidget *view : { m_builtinView, m_applicationView }) {
        switch (mode) {
        case ViewMode::LargeIcons:
            // Icons in rows, names wrapped beneath, as in NT.
            view->setViewMode(QListView::IconMode);
            view->setFlow(QListView::LeftToRight);
            view->setWrapping(true);
            view->setWordWrap(true);
            view->setIconSize(QSize(LargeIconSize, LargeIconSize));
            view->setGridSize(QSize(std::max(75, metrics.averageCharWidth() * 11),
                LargeIconSize + 10 + 2 * metrics.lineSpacing()));
            break;
        case ViewMode::SmallIcons:
            view->setViewMode(QListView::ListMode);
            view->setFlow(QListView::LeftToRight);
            view->setWrapping(true);
            view->setWordWrap(false);
            view->setIconSize(QSize(SmallIconSize, SmallIconSize));
            view->setGridSize(QSize(
                metrics.averageCharWidth() * 24, std::max(SmallIconSize, metrics.height()) + 4));
            break;
        case ViewMode::List:
            // One column per group: in a grouped window a list that wraps
            // into columns would read across the groups.
            view->setViewMode(QListView::ListMode);
            view->setFlow(QListView::TopToBottom);
            view->setWrapping(false);
            view->setWordWrap(false);
            view->setIconSize(QSize(SmallIconSize, SmallIconSize));
            view->setGridSize(QSize(
                metrics.averageCharWidth() * 40, std::max(SmallIconSize, metrics.height()) + 4));
            break;
        }
    }
    updateViewSizes();
}

void ControlPanel::updateViewSizes()
{
    // Each group is exactly as tall as its icons need at the window's
    // width; the window scrolls as a whole, as XP's grouped views did.
    const QMargins margins = m_scroll->widget()->layout()->contentsMargins();
    const int width = std::max(1, m_scroll->viewport()->width() - margins.left() - margins.right());
    for (QListWidget *view : { m_builtinView, m_applicationView }) {
        const QSize grid = view->gridSize();
        int rows = view->count();
        if (view->isWrapping()) {
            const int columns = std::max(1, width / std::max(1, grid.width()));
            rows = (view->count() + columns - 1) / columns;
        }
        view->setFixedHeight(std::max(1, rows) * grid.height());
    }
}

bool ControlPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scroll->viewport() && event->type() == QEvent::Resize)
        updateViewSizes();
    return QMainWindow::eventFilter(watched, event);
}

const ControlPanelApplet *ControlPanel::selectedApplet() const
{
    for (QListWidget *view : { m_builtinView, m_applicationView }) {
        const QList<QListWidgetItem *> items = view->selectedItems();
        if (!items.isEmpty()) {
            const int i = items.first()->data(AppletRole).toInt();
            if (i >= 0 && i < m_applets.size())
                return &m_applets.at(i);
        }
    }
    return nullptr;
}

void ControlPanel::selectionChanged(QListWidget *view)
{
    // One selection across both groups, as in a single folder.
    if (!view->selectedItems().isEmpty()) {
        QListWidget *other = view == m_builtinView ? m_applicationView : m_builtinView;
        other->clearSelection();
    }
    updateStatus();
}

void ControlPanel::updateStatus()
{
    // NT showed the selected applet's description, else the object count.
    const ControlPanelApplet *applet = selectedApplet();
    m_open->setEnabled(applet != nullptr);
    if (applet && !applet->comment.isEmpty())
        m_status->setText(applet->comment);
    else if (applet)
        m_status->setText(applet->name);
    else
        m_status->setText(tr("%n object(s)", nullptr, int(m_applets.size())));
}

void ControlPanel::openSelected()
{
    if (const ControlPanelApplet *applet = selectedApplet())
        openApplet(*applet);
}

void ControlPanel::openItem(QListWidgetItem *item)
{
    const int i = item ? item->data(AppletRole).toInt() : -1;
    if (i >= 0 && i < m_applets.size())
        openApplet(m_applets.at(i));
}

void ControlPanel::openApplet(const ControlPanelApplet &applet)
{
    using B = ControlPanelApplet::Builtin;
    switch (applet.builtin) {
    case B::DateTime:
        Q_EMIT dateTimePropertiesRequested();
        return;
    case B::Display:
        Q_EMIT displayPropertiesRequested();
        return;
    case B::Keyboard:
        Q_EMIT keyboardPropertiesRequested();
        return;
    case B::Mouse:
        Q_EMIT mousePropertiesRequested();
        return;
    case B::Sounds:
        Q_EMIT soundsPropertiesRequested();
        return;
    case B::System:
        Q_EMIT systemPropertiesRequested();
        return;
    case B::Taskbar:
        Q_EMIT taskbarPropertiesRequested();
        return;
    case B::None:
        break;
    }
    qCDebug(lcControlPanel) << "launching" << applet.entry.id;
    const LaunchResult result = launch(applet.entry);
    if (!result.ok) {
        qCWarning(lcControlPanel) << "Could not start" << applet.entry.id << result.error;
        QMessageBox::critical(this, applet.name,
            tr("Cannot start '%1'.\n\nMake sure the program is installed correctly.")
                .arg(applet.name));
    }
}

} // namespace Daytona
