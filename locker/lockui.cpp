// SPDX-License-Identifier: GPL-3.0-or-later

#include "lockui.h"

#include "bevel.h"
#include "daytonastyle.h"

#include <QApplication>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>
#include <pwd.h>
#include <unistd.h>

namespace Daytona {

namespace {

    // Text in NT's dialogs wraps at about this width.
    constexpr int TextWidth = 300;
    // NT's dialog margin (7 dialog units at 8 point).
    constexpr int Margin = 11;
    // NT's dialog frame: a raised edge and a pixel of button face.
    constexpr int Frame = 3;
    constexpr int IdleTimeoutMs = 60 * 1000;

    QLabel *iconLabel(const QIcon &icon, QWidget *parent)
    {
        auto *label = new QLabel(parent);
        label->setPixmap(icon.pixmap(32));
        label->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        label->setFixedWidth(32 + Margin);
        return label;
    }

    QLabel *textLabel(const QString &text, QWidget *parent)
    {
        auto *label = new QLabel(text, parent);
        label->setWordWrap(true);
        label->setFixedWidth(TextWidth);
        return label;
    }

    QPushButton *button(const QString &text, QWidget *parent)
    {
        auto *b = new QPushButton(text, parent);
        b->setMinimumSize(75, 23);
        return b;
    }

    QIcon lockIcon()
    {
        // Chicago95's is a computer with a padlock, close to NT's.
        return QIcon::fromTheme(QStringLiteral("system-lock-screen"),
            QIcon::fromTheme(QStringLiteral("changes-prevent")));
    }

    bool isCtrlAltDelete(const QKeyEvent *key)
    {
        const Qt::KeyboardModifiers both = Qt::ControlModifier | Qt::AltModifier;
        return key->key() == Qt::Key_Delete && (key->modifiers() & both) == both;
    }

    bool isEnter(const QKeyEvent *key)
    {
        return key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
    }

} // namespace

LockUser currentLockUser()
{
    LockUser user;
    if (const passwd *entry = ::getpwuid(::getuid())) {
        user.name = QString::fromLocal8Bit(entry->pw_name);
        // GECOS: "Full Name,room,phone,...".
        if (entry->pw_gecos)
            user.fullName = QString::fromLocal8Bit(entry->pw_gecos).section(QLatin1Char(','), 0, 0);
    }
    if (user.name.isEmpty())
        user.name = QString::fromLocal8Bit(qgetenv("USER"));
    return user;
}

LockPrompt::LockPrompt(const LockUser &user, QWidget *parent)
    : QWidget(parent)
{
    // The dialog's face, not the screen's background it sits on.
    QPalette face;
    face.setColor(QPalette::Window, QApplication::palette().color(QPalette::Window));
    setPalette(face);
    setAutoFillBackground(true);

    const QString who = user.fullName.isEmpty() || user.fullName == user.name
        ? user.name
        : tr("%1 (%2)").arg(user.name, user.fullName);
    const QString inUse = tr("This workstation is in use and has been locked.");
    const QString only = tr("Only %1 can unlock this workstation.").arg(who);

    // Workstation Locked.
    m_lockedPage = new QWidget(this);
    {
        auto *text = new QVBoxLayout;
        text->setSpacing(Margin);
        text->addWidget(textLabel(inUse, m_lockedPage));
        text->addWidget(textLabel(only, m_lockedPage));
        text->addWidget(
            textLabel(tr("Press Ctrl+Alt+Delete to unlock this workstation."), m_lockedPage));
        auto *row = new QHBoxLayout(m_lockedPage);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);
        row->addWidget(iconLabel(lockIcon(), m_lockedPage));
        row->addLayout(text);
    }

    // Unlock Workstation.
    m_unlockPage = new QWidget(this);
    {
        auto *text = new QVBoxLayout;
        text->setSpacing(Margin);
        text->addWidget(textLabel(inUse, m_unlockPage));
        text->addWidget(textLabel(only, m_unlockPage));

        auto *userName = new QLineEdit(user.name, m_unlockPage);
        userName->setEnabled(false);
        m_password = new QLineEdit(m_unlockPage);
        m_password->setEchoMode(QLineEdit::Password);
        auto *userLabel = new QLabel(tr("&User name:"), m_unlockPage);
        userLabel->setBuddy(userName);
        auto *passwordLabel = new QLabel(tr("&Password:"), m_unlockPage);
        passwordLabel->setBuddy(m_password);
        auto *fields = new QGridLayout;
        fields->setHorizontalSpacing(Margin);
        fields->setVerticalSpacing(7);
        fields->addWidget(userLabel, 0, 0);
        fields->addWidget(userName, 0, 1);
        fields->addWidget(passwordLabel, 1, 0);
        fields->addWidget(m_password, 1, 1);
        text->addLayout(fields);

        m_ok = button(tr("OK"), m_unlockPage);
        m_ok->setDefault(true);
        m_cancel = button(tr("Cancel"), m_unlockPage);
        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(6);
        buttons->addStretch();
        buttons->addWidget(m_ok);
        buttons->addWidget(m_cancel);

        auto *row = new QHBoxLayout;
        row->setSpacing(0);
        row->addWidget(iconLabel(lockIcon(), m_unlockPage));
        row->addLayout(text);
        auto *column = new QVBoxLayout(m_unlockPage);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(Margin + 3);
        column->addLayout(row);
        column->addLayout(buttons);

        connect(m_password, &QLineEdit::returnPressed, m_ok, &QPushButton::click);
        connect(m_ok, &QPushButton::clicked, this,
            [this] { Q_EMIT unlockRequested(m_password->text()); });
        connect(m_cancel, &QPushButton::clicked, this, &LockPrompt::cancelRequested);
    }

    // The wrong password.
    m_failedPage = new QWidget(this);
    {
        auto *row = new QHBoxLayout;
        row->setSpacing(0);
        row->addWidget(
            iconLabel(style()->standardIcon(QStyle::SP_MessageBoxWarning), m_failedPage));
        row->addWidget(textLabel(tr("The password is incorrect. Type your password again. "
                                    "Letters in passwords must be typed using the correct case. "
                                    "Make sure that Caps Lock is not accidentally on."),
            m_failedPage));
        m_failedOk = button(tr("OK"), m_failedPage);
        auto *buttons = new QHBoxLayout;
        buttons->addStretch();
        buttons->addWidget(m_failedOk);
        buttons->addStretch();
        auto *column = new QVBoxLayout(m_failedPage);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(Margin + 3);
        column->addLayout(row);
        column->addLayout(buttons);
        connect(m_failedOk, &QPushButton::clicked, this, &LockPrompt::failureAcknowledged);
    }

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->addWidget(m_lockedPage);
    layout->addWidget(m_unlockPage);
    layout->addWidget(m_failedPage);
    updateMargins();
    setPage(Page::Locked);
}

void LockPrompt::setPage(Page page)
{
    const bool changed = page != m_page;
    m_page = page;
    m_lockedPage->setVisible(page == Page::Locked);
    m_unlockPage->setVisible(page == Page::Unlock || page == Page::Checking);
    m_failedPage->setVisible(page == Page::Failed);
    m_unlockPage->setEnabled(page == Page::Unlock);
    setCursor(page == Page::Checking ? Qt::WaitCursor : Qt::ArrowCursor);
    if (changed && page != Page::Checking)
        m_password->clear();
    if (page == Page::Unlock)
        m_password->setFocus();
    else if (page == Page::Failed)
        m_failedOk->setFocus();
    adjustSize();
    update();
}

QString LockPrompt::title() const
{
    return m_page == Page::Locked ? tr("Workstation Locked") : tr("Unlock Workstation");
}

int LockPrompt::titleBarHeight() const
{
    QFont bold = font();
    bold.setBold(true);
    // NT's 18 pixel caption at 8 point, growing with the font.
    return qMax(18, QFontMetrics(bold).height() + 5);
}

void LockPrompt::updateMargins()
{
    const int title = titleBarHeight();
    layout()->setContentsMargins(
        Frame + Margin, Frame + title + 1 + Margin, Frame + Margin, Frame + Margin);
}

void LockPrompt::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange && layout())
        updateMargins();
    QWidget::changeEvent(event);
}

void LockPrompt::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    Bevel::raised(&painter, rect(), palette());
    const QRect titleBar(Frame, Frame, width() - 2 * Frame, titleBarHeight());
    painter.fillRect(titleBar, Style::titleBarBrush(titleBar));
    QFont bold = font();
    bold.setBold(true);
    painter.setFont(bold);
    painter.setPen(Style::schemeColour(QStringLiteral("TitleText")));
    painter.drawText(titleBar.adjusted(3, 0, -3, 0), Qt::AlignVCenter | Qt::AlignLeft, title());
}

LockController::LockController(
    const LockUser &user, bool startBlank, Authenticate authenticate, QObject *parent)
    : QObject(parent)
    , m_startBlank(startBlank)
    , m_blank(startBlank)
    , m_authenticate(std::move(authenticate))
    , m_prompt(new LockPrompt(user))
{
    m_prompt->hide();
    connect(m_prompt, &LockPrompt::unlockRequested, this, &LockController::checkPassword);
    connect(m_prompt, &LockPrompt::cancelRequested, this, &LockController::rest);
    connect(m_prompt, &LockPrompt::failureAcknowledged, this,
        [this] { m_prompt->setPage(LockPrompt::Page::Unlock); });

    m_idle.setSingleShot(true);
    m_idle.setInterval(IdleTimeoutMs);
    connect(&m_idle, &QTimer::timeout, this, &LockController::reset);
    m_idle.start();

    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &LockController::placePrompt);
    qApp->installEventFilter(this);
}

LockController::~LockController()
{
    qApp->removeEventFilter(this);
    delete m_prompt;
    for (const QPointer<QWidget> &screen : std::as_const(m_screens))
        delete screen;
}

QColor LockController::backgroundColour()
{
    return Style::schemeColour(QStringLiteral("Background"));
}

QWidget *LockController::addScreen(QScreen *screen)
{
    auto *widget = new QWidget(nullptr, Qt::FramelessWindowHint);
    widget->setAttribute(Qt::WA_DeleteOnClose, false);
    widget->setAutoFillBackground(true);
    if (screen) {
        widget->setScreen(screen);
        widget->resize(screen->size());
    }
    m_screens.append(widget);
    updateScreens();
    placePrompt();
    return widget;
}

void LockController::removeScreen(QScreen *screen)
{
    for (qsizetype i = 0; i < m_screens.size(); ++i) {
        QWidget *widget = m_screens.at(i);
        if (!widget || widget->screen() != screen)
            continue;
        if (m_prompt && m_prompt->parentWidget() == widget) {
            m_prompt->hide();
            m_prompt->setParent(nullptr);
        }
        m_screens.removeAt(i);
        delete widget;
        break;
    }
    placePrompt();
}

QList<QWidget *> LockController::screens() const
{
    QList<QWidget *> list;
    for (const QPointer<QWidget> &screen : m_screens) {
        if (screen)
            list << screen;
    }
    return list;
}

bool LockController::eventFilter(QObject *watched, QEvent *event)
{
    // Input reaches a window before its widgets, once per event.
    if (!watched->isWindowType())
        return false;
    switch (event->type()) {
    case QEvent::KeyPress: {
        m_idle.start();
        if (m_blank) {
            wake();
            return true;
        }
        const auto *key = static_cast<QKeyEvent *>(event);
        switch (m_prompt->page()) {
        case LockPrompt::Page::Locked:
            if (isCtrlAltDelete(key))
                m_prompt->setPage(LockPrompt::Page::Unlock);
            // Nothing else is typed anywhere while locked.
            return true;
        case LockPrompt::Page::Unlock:
            if (key->key() == Qt::Key_Escape) {
                rest();
                return true;
            }
            // Already unlocking: nothing more for Ctrl+Alt+Delete to do.
            return isCtrlAltDelete(key);
        case LockPrompt::Page::Checking:
            return true;
        case LockPrompt::Page::Failed:
            if (isEnter(key) || key->key() == Qt::Key_Escape) {
                m_prompt->setPage(LockPrompt::Page::Unlock);
                return true;
            }
            return false;
        }
        return false;
    }
    case QEvent::KeyRelease:
        return m_blank;
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        m_idle.start();
        if (m_blank) {
            wake();
            return true;
        }
        return false;
    case QEvent::Resize:
        QMetaObject::invokeMethod(this, &LockController::placePrompt, Qt::QueuedConnection);
        return false;
    default:
        return false;
    }
}

void LockController::setRequiresCtrlAltDelete(bool require)
{
    m_requireCtrlAltDelete = require;
    if (m_prompt->page() == LockPrompt::Page::Locked
        || (require && m_prompt->page() == LockPrompt::Page::Unlock))
        rest();
}

void LockController::rest()
{
    if (m_requireCtrlAltDelete) {
        m_prompt->setPage(LockPrompt::Page::Locked);
        return;
    }
    m_prompt->passwordField()->clear();
    m_prompt->setPage(LockPrompt::Page::Unlock);
}

void LockController::wake()
{
    m_blank = false;
    updateScreens();
    placePrompt();
    if (m_prompt->page() == LockPrompt::Page::Unlock)
        m_prompt->passwordField()->setFocus();
}

void LockController::reset()
{
    if (m_prompt->page() == LockPrompt::Page::Checking) {
        m_idle.start();
        return;
    }
    rest();
    m_blank = m_startBlank;
    updateScreens();
    placePrompt();
}

void LockController::updateScreens()
{
    for (const QPointer<QWidget> &screen : std::as_const(m_screens)) {
        if (!screen)
            continue;
        QPalette palette = screen->palette();
        palette.setColor(QPalette::Window, m_blank ? QColor(Qt::black) : backgroundColour());
        screen->setPalette(palette);
        if (m_blank)
            screen->setCursor(Qt::BlankCursor);
        else
            screen->unsetCursor();
    }
}

void LockController::placePrompt()
{
    if (!m_prompt)
        return;
    // The dialog goes where the keyboard is: labwc gives keyboard focus to
    // one lock surface, and moves it when another is clicked.
    QWidget *target = nullptr;
    const QWindow *focus = QGuiApplication::focusWindow();
    for (const QPointer<QWidget> &screen : std::as_const(m_screens)) {
        if (screen && focus && screen->windowHandle() == focus)
            target = screen;
    }
    for (const QPointer<QWidget> &screen : std::as_const(m_screens)) {
        if (!target && screen && screen->screen() == QGuiApplication::primaryScreen())
            target = screen;
    }
    for (const QPointer<QWidget> &screen : std::as_const(m_screens)) {
        if (!target && screen)
            target = screen;
    }
    if (!target)
        return;
    if (m_prompt->parentWidget() != target) {
        m_prompt->setParent(target);
        if (m_prompt->page() == LockPrompt::Page::Unlock)
            m_prompt->passwordField()->setFocus();
    }
    m_prompt->adjustSize();
    m_prompt->move(
        (target->width() - m_prompt->width()) / 2, (target->height() - m_prompt->height()) / 2);
    m_prompt->setVisible(!m_blank);
}

void LockController::checkPassword(const QString &password)
{
    m_prompt->setPage(LockPrompt::Page::Checking);
    QPointer<LockController> self(this);
    m_authenticate(password, [self](bool accepted) {
        if (!self)
            return;
        self->m_idle.start();
        if (accepted)
            Q_EMIT self->unlocked();
        else
            self->m_prompt->setPage(LockPrompt::Page::Failed);
    });
}

} // namespace Daytona
