// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "sessions.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;

namespace Daytona {

// The log on dialog, as Windows 98's (Windows 2000's Shut Down... button
// added): user name, password, the session to start, OK and Cancel.
class LogonDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogonDialog(QWidget *parent = nullptr);

    QLineEdit *userField() const { return m_user; }
    QLineEdit *passwordField() const { return m_password; }
    QPushButton *okButton() const { return m_ok; }
    QPushButton *cancelButton() const { return m_cancel; }
    QPushButton *sessionButton() const { return m_sessionButton; }
    QPushButton *shutDownButton() const { return m_shutDown; }
    QLabel *sessionLabel() const { return m_sessionName; }

    void setSessionName(const QString &name);
    // Focus in User name, or in Password when a name is filled in.
    void focusFirstEmpty();

Q_SIGNALS:
    void logOnRequested(const QString &user, const QString &password);
    void sessionRequested();
    void shutDownRequested();

public Q_SLOTS:
    // Cancel and Escape clear both fields; the dialog never closes.
    void reject() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLineEdit *m_user;
    QLineEdit *m_password;
    QPushButton *m_ok;
    QPushButton *m_cancel;
    QPushButton *m_sessionButton;
    QPushButton *m_shutDown;
    QLabel *m_sessionName;
};

// Session...: the installed sessions to choose from.
class SessionDialog : public QDialog
{
    Q_OBJECT

public:
    SessionDialog(const QList<DesktopSession> &sessions, int current, QWidget *parent = nullptr);

    QListWidget *list() const { return m_list; }
    int chosen() const;

private:
    QListWidget *m_list;
};

// A further question from PAM: a one-time code, a new password, and so on,
// labelled with PAM's own text.
class PromptDialog : public QDialog
{
    Q_OBJECT

public:
    PromptDialog(const QString &prompt, bool secret, QWidget *parent = nullptr);

    QLabel *label() const { return m_label; }
    QLineEdit *field() const { return m_field; }

private:
    QLabel *m_label;
    QLineEdit *m_field;
};

// What the greeter's Shut Down dialog may offer, from logind.
struct GreeterPower
{
    bool shutDown = false;
    bool restart = false;
    bool standBy = false;
};

enum class PowerAction
{
    ShutDown,
    Restart,
    StandBy
};

// NT's Shut Down dialog, with the choices there are before anyone has
// logged on.
class GreeterShutDownDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GreeterShutDownDialog(const GreeterPower &power, QWidget *parent = nullptr);

    PowerAction chosen() const;
    QRadioButton *option(PowerAction action) const;

private:
    QRadioButton *m_shutDown;
    QRadioButton *m_restart;
    QRadioButton *m_standBy;
};

} // namespace Daytona
