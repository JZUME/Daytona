// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "logonui.h"
#include "sessions.h"

#include <QObject>
#include <QPointer>
#include <functional>

class QMessageBox;

namespace Daytona {

class GreetdClient;

// Shut down, restart and stand by through systemd-logind. Asynchronous, so
// the dialog never waits on D-Bus.
class PowerActions : public QObject
{
    Q_OBJECT

public:
    using Availability = std::function<void(const GreeterPower &power)>;
    using Failure = std::function<void(const QString &message)>;

    explicit PowerActions(QObject *parent = nullptr);
    virtual void queryAvailability(Availability done);
    virtual void perform(PowerAction action, Failure failed);
};

// Logs a user on through greetd: answers PAM's conversation from the log on
// dialog (the password from the Password box, further prompts and messages
// in dialogs of their own), then starts the chosen session.
class Greeter : public QObject
{
    Q_OBJECT

public:
    Greeter(GreetdClient *greetd, const QList<DesktopSession> &sessions, const QString &statePath,
        PowerActions *power, QObject *parent = nullptr);
    ~Greeter() override;

    LogonDialog *dialog() const { return m_dialog; }
    int currentSession() const { return m_session; }
    void setCurrentSession(int index);

    // Whatever is shown above the log on dialog: a prompt, a message, the
    // session list or the Shut Down dialog. For tests.
    QDialog *openDialog() const { return m_open; }

    // /var/lib/daytona-greeter/state, unless DAYTONA_GREETER_STATE is set.
    static QString defaultStatePath();

Q_SIGNALS:
    // The session will start once the greeter exits.
    void finished();

private:
    enum class State
    {
        Idle,
        Authenticating,
        Starting,
        Cancelling
    };

    void logOn(const QString &user, const QString &password);
    void onAuthMessage(const QString &type, const QString &message);
    void onSuccess();
    void onError(const QString &errorType, const QString &description);
    void onDisconnected();
    void showPrompt(const QString &prompt, bool secret);
    void showMessage(const QString &message, bool warning, std::function<void()> then);
    void fail(const QString &message);
    void cancel();
    void chooseSession();
    void shutDown();
    void updateEnabled();
    void forgetPassword();
    void saveState();

    GreetdClient *m_greetd;
    QList<DesktopSession> m_sessions;
    QString m_statePath;
    PowerActions *m_power;
    LogonDialog *m_dialog;
    QPointer<QDialog> m_open;
    State m_state = State::Idle;
    int m_session = -1;
    QString m_user;
    QString m_password;
    bool m_passwordUsed = false;
};

} // namespace Daytona
