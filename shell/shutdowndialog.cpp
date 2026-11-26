// SPDX-License-Identifier: GPL-3.0-or-later

#include "shutdowndialog.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <csignal>
#include <sys/types.h>

Q_LOGGING_CATEGORY(lcSession, "daytona.session", QtInfoMsg)

namespace Daytona {

namespace {

    QDBusMessage logindCall(const QString &method)
    {
        return QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
            QStringLiteral("/org/freedesktop/login1"),
            QStringLiteral("org.freedesktop.login1.Manager"), method);
    }

    bool allowed(const QDBusPendingCall &call)
    {
        QDBusPendingReply<QString> reply = call;
        if (reply.isError())
            return false;
        // "challenge" means allowed after authentication, which logind asks for.
        return reply.value() == QLatin1String("yes") || reply.value() == QLatin1String("challenge");
    }

} // namespace

SessionActions::SessionActions(QObject *parent)
    : QObject(parent)
{
}

void SessionActions::queryAvailability(Availability done)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        done({});
        return;
    }
    const QStringList methods { QStringLiteral("CanPowerOff"), QStringLiteral("CanReboot"),
        QStringLiteral("CanSuspend"), QStringLiteral("CanHibernate") };
    QList<QDBusPendingCallWatcher *> watchers;
    for (const QString &method : methods)
        watchers << new QDBusPendingCallWatcher(bus.asyncCall(logindCall(method), 3000), this);
    auto finished = std::make_shared<int>(0);
    auto report = [=](QDBusPendingCallWatcher *) {
        if (++*finished < watchers.size())
            return;
        PowerAvailability available;
        available.shutDown = allowed(*watchers[0]);
        available.restart = allowed(*watchers[1]);
        available.standBy = allowed(*watchers[2]);
        available.hibernate = allowed(*watchers[3]);
        done(available);
        for (QDBusPendingCallWatcher *watcher : watchers)
            watcher->deleteLater();
    };
    for (QDBusPendingCallWatcher *watcher : watchers)
        connect(watcher, &QDBusPendingCallWatcher::finished, this, report);
}

void SessionActions::perform(SessionAction action, Failure failed)
{
    if (action == SessionAction::Lock) {
        Q_EMIT lockRequested();
        return;
    }
    if (action == SessionAction::LogOff) {
        // End the session as "labwc --exit" does: SIGTERM to the compositor
        // named by LABWC_PID, which labwc sets for the programs it starts,
        // including this shell. Signalling it directly works whichever
        // labwc binary runs the session (Daytona's own build included).
        bool ok = false;
        const qint64 pid = qEnvironmentVariable("LABWC_PID").toLongLong(&ok);
        if (!ok || pid <= 1 || ::kill(pid_t(pid), SIGTERM) != 0)
            failed(tr("Could not end the session."));
        return;
    }
    QString method;
    switch (action) {
    case SessionAction::ShutDown:
        method = QStringLiteral("PowerOff");
        break;
    case SessionAction::Restart:
        method = QStringLiteral("Reboot");
        break;
    case SessionAction::StandBy:
        // The screen is locked before the machine sleeps: swayidle holds
        // logind's sleep delay until daytona-shell --lock has locked it.
        method = QStringLiteral("Suspend");
        break;
    case SessionAction::Hibernate:
        method = QStringLiteral("Hibernate");
        break;
    case SessionAction::LogOff:
    case SessionAction::Lock:
        return; // handled above
    }
    QDBusMessage call = logindCall(method);
    call << true; // interactive: logind may ask for authentication
    auto *watcher
        = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call, 25000), this);
    connect(
        watcher, &QDBusPendingCallWatcher::finished, this, [failed](QDBusPendingCallWatcher *w) {
            const QDBusPendingReply<> reply = *w;
            if (reply.isError()) {
                qCWarning(lcSession) << "logind refused:" << reply.error().message();
                failed(reply.error().message());
            }
            w->deleteLater();
        });
}

ShutDownDialog::ShutDownDialog(SessionActions *actions, QWidget *parent)
    : QDialog(parent)
    , m_actions(actions)
{
    setWindowTitle(tr("Shut Down Daytona"));
    showError = [this](const QString &title, const QString &message) {
        QMessageBox::critical(this, title, message);
    };

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("system-shutdown")).pixmap(32));
    icon->setAlignment(Qt::AlignTop);
    auto *question = new QLabel(tr("Are you sure you want to:"), this);
    m_shutDown = new QRadioButton(tr("&Shut down the computer?"), this);
    m_restart = new QRadioButton(tr("&Restart the computer?"), this);
    // From Windows 98 and 2000, which offered them where the hardware could:
    // hidden unless logind allows them.
    m_standBy = new QRadioButton(tr("Put the computer on s&tand by?"), this);
    m_hibernate = new QRadioButton(tr("&Hibernate the computer?"), this);
    m_standBy->hide();
    m_hibernate->hide();
    m_logOff = new QRadioButton(tr("&Close all programs and log on as a different user?"), this);
    // Not in NT's dialog, where locking was Ctrl+Alt+Del's: Lock Workstation
    // is offered here too, so it can be found from the Start menu.
    m_lock = new QRadioButton(tr("&Lock the computer?"), this);
    m_lock->setEnabled(false);

    // NT's buttons: Yes (default) and No.
    auto *yes = new QPushButton(tr("&Yes"), this);
    yes->setDefault(true);
    auto *no = new QPushButton(tr("&No"), this);
    for (QPushButton *b : { yes, no })
        b->setMinimumSize(75, 23);
    connect(yes, &QPushButton::clicked, this, &ShutDownDialog::accept);
    connect(no, &QPushButton::clicked, this, &ShutDownDialog::reject);

    auto *options = new QVBoxLayout;
    options->addWidget(question);
    options->addWidget(m_shutDown);
    options->addWidget(m_restart);
    options->addWidget(m_standBy);
    options->addWidget(m_hibernate);
    options->addWidget(m_logOff);
    options->addWidget(m_lock);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(yes);
    buttons->addWidget(no);
    buttons->addStretch();

    auto *grid = new QGridLayout(this);
    grid->addWidget(icon, 0, 0);
    grid->addLayout(options, 0, 1);
    grid->addLayout(buttons, 1, 0, 1, 2);
    // Fixed, but following the options shown.
    grid->setSizeConstraint(QLayout::SetFixedSize);
}

QRadioButton *ShutDownDialog::option(SessionAction action) const
{
    switch (action) {
    case SessionAction::ShutDown:
        return m_shutDown;
    case SessionAction::Restart:
        return m_restart;
    case SessionAction::StandBy:
        return m_standBy;
    case SessionAction::Hibernate:
        return m_hibernate;
    case SessionAction::LogOff:
        return m_logOff;
    case SessionAction::Lock:
        return m_lock;
    }
    return m_logOff;
}

SessionAction ShutDownDialog::chosen() const
{
    if (m_shutDown->isChecked())
        return SessionAction::ShutDown;
    if (m_restart->isChecked())
        return SessionAction::Restart;
    if (m_standBy->isChecked())
        return SessionAction::StandBy;
    if (m_hibernate->isChecked())
        return SessionAction::Hibernate;
    if (m_lock->isChecked())
        return SessionAction::Lock;
    return SessionAction::LogOff;
}

void ShutDownDialog::select(SessionAction action)
{
    // Fall back to an enabled option if the remembered one is unavailable.
    for (SessionAction candidate :
        { action, SessionAction::ShutDown, SessionAction::Restart, SessionAction::LogOff }) {
        if (option(candidate)->isEnabled() && !option(candidate)->isHidden()) {
            option(candidate)->setChecked(true);
            return;
        }
    }
}

void ShutDownDialog::setAvailability(const PowerAvailability &available)
{
    m_shutDown->setEnabled(available.shutDown);
    m_restart->setEnabled(available.restart);
    m_standBy->setVisible(available.standBy);
    m_standBy->setEnabled(available.standBy);
    m_hibernate->setVisible(available.hibernate);
    m_hibernate->setEnabled(available.hibernate);
    if (!option(chosen())->isEnabled() || !option(chosen())->isChecked())
        select(m_last);
}

void ShutDownDialog::setLockAvailable(bool available)
{
    m_lock->setEnabled(available);
    if (!available && m_lock->isChecked())
        select(m_last);
}

void ShutDownDialog::prepare()
{
    // Until logind answers, only logging off is certainly possible.
    setAvailability({});
    select(m_last);
    if (m_actions) {
        QPointer<ShutDownDialog> self(this);
        m_actions->queryAvailability([self](const PowerAvailability &available) {
            if (self) {
                self->setAvailability(available);
                self->select(self->m_last);
            }
        });
    }
}

void ShutDownDialog::accept()
{
    const SessionAction action = chosen();
    m_last = action; // remembered for this session only
    QDialog::accept();
    if (m_actions) {
        QPointer<ShutDownDialog> self(this);
        m_actions->perform(action, [self](const QString &message) {
            if (self)
                self->showError(self->windowTitle(), message);
        });
    }
}

} // namespace Daytona
