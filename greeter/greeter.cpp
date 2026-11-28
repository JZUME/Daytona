// SPDX-License-Identifier: GPL-3.0-or-later

#include "greeter.h"

#include "greetd.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <memory>

Q_LOGGING_CATEGORY(lcGreeter, "daytona.greeter", QtInfoMsg)

namespace Daytona {

namespace {

    const QString Logind = QStringLiteral("org.freedesktop.login1");
    const QString LogindPath = QStringLiteral("/org/freedesktop/login1");
    const QString LogindManager = QStringLiteral("org.freedesktop.login1.Manager");

    QDBusMessage logindCall(const QString &method)
    {
        return QDBusMessage::createMethodCall(Logind, LogindPath, LogindManager, method);
    }

    // logind's answers that mean the user may: "yes", or "challenge" (polkit
    // will ask, which needs no agent for the active local seat by default).
    bool allowed(const QString &answer)
    {
        return answer == QLatin1String("yes") || answer == QLatin1String("challenge");
    }

    void wipe(QString &secret)
    {
        secret.fill(QChar(0));
        secret.clear();
    }

} // namespace

PowerActions::PowerActions(QObject *parent)
    : QObject(parent)
{
}

void PowerActions::queryAvailability(Availability done)
{
    struct Pending
    {
        GreeterPower power;
        int left = 3;
        Availability done;
    };
    auto pending = std::make_shared<Pending>();
    pending->done = std::move(done);
    if (!QDBusConnection::systemBus().isConnected()) {
        // No system bus, so no logind: nothing is offered.
        QMetaObject::invokeMethod(
            this, [pending] { pending->done(pending->power); }, Qt::QueuedConnection);
        return;
    }
    const auto ask = [this, pending](const QString &method, bool GreeterPower::*field) {
        auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::systemBus().asyncCall(logindCall(method)), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [pending, field](QDBusPendingCallWatcher *call) {
                const QDBusPendingReply<QString> reply = *call;
                pending->power.*field = !reply.isError() && allowed(reply.value());
                call->deleteLater();
                if (--pending->left == 0)
                    pending->done(pending->power);
            });
    };
    ask(QStringLiteral("CanPowerOff"), &GreeterPower::shutDown);
    ask(QStringLiteral("CanReboot"), &GreeterPower::restart);
    ask(QStringLiteral("CanSuspend"), &GreeterPower::standBy);
}

void PowerActions::perform(PowerAction action, Failure failed)
{
    QString method = QStringLiteral("PowerOff");
    if (action == PowerAction::Restart)
        method = QStringLiteral("Reboot");
    else if (action == PowerAction::StandBy)
        method = QStringLiteral("Suspend");
    if (!QDBusConnection::systemBus().isConnected()) {
        failed(tr("The computer cannot be shut down: systemd-logind is not running."));
        return;
    }
    QDBusMessage call = logindCall(method);
    // Not interactive: there is no polkit agent before anyone logs on.
    call << false;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [failed = std::move(failed)](QDBusPendingCallWatcher *reply) {
            if (reply->isError())
                failed(reply->error().message());
            reply->deleteLater();
        });
}

Greeter::Greeter(GreetdClient *greetd, const QList<DesktopSession> &sessions,
    const QString &statePath, PowerActions *power, QObject *parent)
    : QObject(parent)
    , m_greetd(greetd)
    , m_sessions(sessions)
    , m_statePath(statePath)
    , m_power(power)
    , m_dialog(new LogonDialog)
{
    const QSettings state(m_statePath, QSettings::IniFormat);
    m_dialog->userField()->setText(state.value(QStringLiteral("Last/User")).toString());
    setCurrentSession(
        defaultSession(m_sessions, state.value(QStringLiteral("Last/Session")).toString()));
    m_dialog->focusFirstEmpty();

    connect(m_dialog, &LogonDialog::logOnRequested, this, &Greeter::logOn);
    connect(m_dialog, &LogonDialog::sessionRequested, this, &Greeter::chooseSession);
    connect(m_dialog, &LogonDialog::shutDownRequested, this, &Greeter::shutDown);
    connect(m_greetd, &GreetdClient::authMessage, this, &Greeter::onAuthMessage);
    connect(m_greetd, &GreetdClient::success, this, &Greeter::onSuccess);
    connect(m_greetd, &GreetdClient::error, this, &Greeter::onError);
}

Greeter::~Greeter()
{
    forgetPassword();
    delete m_dialog;
}

QString Greeter::defaultStatePath()
{
    const QString path = qEnvironmentVariable("DAYTONA_GREETER_STATE");
    return path.isEmpty() ? QStringLiteral("/var/lib/daytona-greeter/state") : path;
}

void Greeter::setCurrentSession(int index)
{
    m_session = index >= 0 && index < m_sessions.size() ? index : -1;
    m_dialog->setSessionName(m_session >= 0 ? m_sessions.at(m_session).name : tr("(none)"));
    updateEnabled();
}

void Greeter::logOn(const QString &user, const QString &password)
{
    if (m_state != State::Idle)
        return;
    if (user.isEmpty()) {
        m_dialog->userField()->setFocus();
        return;
    }
    if (m_session < 0) {
        showMessage(tr("No desktop sessions are installed, so there is nothing to log on to."),
            true, [this] { updateEnabled(); });
        return;
    }
    m_user = user;
    m_password = password;
    m_passwordUsed = false;
    m_state = State::Authenticating;
    updateEnabled();
    m_greetd->createSession(user);
}

void Greeter::onAuthMessage(const QString &type, const QString &message)
{
    if (m_state != State::Authenticating) {
        qCWarning(lcGreeter) << "Unexpected PAM message:" << type;
        return;
    }
    const bool secret = type == QLatin1String("secret");
    if (secret && !m_passwordUsed) {
        // The first password PAM asks for is the one in the Password box.
        m_passwordUsed = true;
        m_greetd->postResponse(m_password);
        forgetPassword();
        return;
    }
    if (secret || type == QLatin1String("visible")) {
        showPrompt(message, secret);
    } else if (type == QLatin1String("info") || type == QLatin1String("error")) {
        showMessage(message, type == QLatin1String("error"), [this] {
            if (m_state == State::Authenticating)
                m_greetd->acknowledge();
            updateEnabled();
        });
    } else {
        qCWarning(lcGreeter) << "Unknown PAM message type:" << type;
        cancel();
    }
}

void Greeter::onSuccess()
{
    switch (m_state) {
    case State::Authenticating: {
        const DesktopSession &session = m_sessions.at(m_session);
        qCInfo(lcGreeter) << "Starting" << session.id << "for" << m_user;
        m_state = State::Starting;
        m_greetd->startSession(session.command, session.environment());
        break;
    }
    case State::Starting:
        saveState();
        Q_EMIT finished();
        break;
    case State::Cancelling:
        m_state = State::Idle;
        updateEnabled();
        break;
    case State::Idle:
        break;
    }
}

void Greeter::onError(const QString &errorType, const QString &description)
{
    if (m_state == State::Cancelling || m_state == State::Idle) {
        m_state = State::Idle;
        updateEnabled();
        return;
    }
    qCInfo(lcGreeter) << "Log on failed:" << errorType << description;
    // greetd's descriptions name the PAM step that failed: authentication
    // is the password; the account step is everything else (expired,
    // locked), which PAM has usually explained in a message already.
    if (errorType == QLatin1String("auth_error")
        && description.contains(QLatin1String("NEW_AUTHTOK_REQD"))) {
        // greetd (0.10) does not let PAM change an expired password.
        fail(tr("Your password has expired and must be changed. The log on screen cannot "
                "change it: change it at a text console with the passwd command, then log "
                "on again."));
    } else if (errorType == QLatin1String("auth_error")
        && !description.startsWith(QLatin1String("pam_acct_mgmt"))) {
        fail(tr("The system could not log you on. Make sure your User name is correct, then "
                "type your password again. Letters in passwords must be typed using the "
                "correct case."));
    } else if (errorType == QLatin1String("auth_error")) {
        fail(tr("The system could not log you on. Your account cannot be used at this time."));
    } else if (description.isEmpty()) {
        fail(tr("The system could not log you on."));
    } else {
        fail(tr("The system could not log you on.\n\n%1").arg(description));
    }
}

void Greeter::showPrompt(const QString &prompt, bool secret)
{
    auto *dialog = new PromptDialog(prompt, secret, m_dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        QString answer = dialog->field()->text();
        dialog->field()->clear();
        if (m_state == State::Authenticating)
            m_greetd->postResponse(answer);
        wipe(answer);
        updateEnabled();
    });
    connect(dialog, &QDialog::rejected, this, [this] { cancel(); });
    m_open = dialog;
    updateEnabled();
    dialog->open();
}

void Greeter::showMessage(const QString &message, bool warning, std::function<void()> then)
{
    auto *box = new QMessageBox(warning ? QMessageBox::Warning : QMessageBox::Information,
        tr("Log On Message"), message.trimmed(), QMessageBox::Ok, m_dialog);
    box->setAttribute(Qt::WA_DeleteOnClose);
    connect(box, &QDialog::finished, this, [this, then = std::move(then)] {
        m_open = nullptr;
        then();
    });
    m_open = box;
    updateEnabled();
    box->open();
}

void Greeter::fail(const QString &message)
{
    // greetd needs the failed session cancelled before the next try.
    cancel();
    showMessage(message, true, [this] {
        m_dialog->passwordField()->clear();
        updateEnabled();
        m_dialog->focusFirstEmpty();
    });
}

void Greeter::cancel()
{
    forgetPassword();
    if (m_state == State::Idle || m_state == State::Cancelling) {
        updateEnabled();
        return;
    }
    m_state = State::Cancelling;
    m_greetd->cancelSession();
    updateEnabled();
}

void Greeter::chooseSession()
{
    if (m_sessions.isEmpty())
        return;
    auto *dialog = new SessionDialog(m_sessions, m_session, m_dialog);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(
        dialog, &QDialog::accepted, this, [this, dialog] { setCurrentSession(dialog->chosen()); });
    connect(dialog, &QDialog::finished, this, [this] {
        updateEnabled();
        m_dialog->focusFirstEmpty();
    });
    m_open = dialog;
    updateEnabled();
    dialog->open();
}

void Greeter::shutDown()
{
    QPointer<Greeter> self(this);
    m_power->queryAvailability([self](const GreeterPower &power) {
        if (!self || self->m_open)
            return;
        auto *dialog = new GreeterShutDownDialog(power, self->m_dialog);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        Greeter *greeter = self;
        connect(dialog, &QDialog::accepted, greeter, [greeter, dialog] {
            greeter->m_power->perform(
                dialog->chosen(), [self = QPointer<Greeter>(greeter)](const QString &message) {
                    if (self)
                        self->showMessage(message, true, [] { });
                });
        });
        connect(dialog, &QDialog::finished, greeter, [greeter] { greeter->updateEnabled(); });
        greeter->m_open = dialog;
        greeter->updateEnabled();
        dialog->open();
    });
}

void Greeter::updateEnabled()
{
    const bool idle = m_state == State::Idle;
    for (QWidget *widget : { static_cast<QWidget *>(m_dialog->userField()),
             static_cast<QWidget *>(m_dialog->passwordField()),
             static_cast<QWidget *>(m_dialog->okButton()),
             static_cast<QWidget *>(m_dialog->cancelButton()),
             static_cast<QWidget *>(m_dialog->shutDownButton()) })
        widget->setEnabled(idle);
    m_dialog->sessionButton()->setEnabled(idle && !m_sessions.isEmpty());
    if (idle)
        m_dialog->unsetCursor();
    else
        m_dialog->setCursor(Qt::WaitCursor);
}

void Greeter::forgetPassword()
{
    wipe(m_password);
}

void Greeter::saveState()
{
    QSettings state(m_statePath, QSettings::IniFormat);
    state.setValue(QStringLiteral("Last/User"), m_user);
    if (m_session >= 0)
        state.setValue(QStringLiteral("Last/Session"), m_sessions.at(m_session).id);
    state.sync();
    if (state.status() != QSettings::NoError)
        qCWarning(lcGreeter) << "Cannot remember the user and session in" << m_statePath;
}

} // namespace Daytona
