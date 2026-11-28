// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;
class QScreen;

namespace Daytona {

// Whose session is locked.
struct LockUser
{
    QString name;
    QString fullName;
};

// The user running this process, with the full name from the password
// database's GECOS field.
LockUser currentLockUser();

// NT 4.0's lock dialogs. A lock surface cannot have windows of its own, so
// the dialog is a widget drawn with an NT dialog frame and title bar inside
// the lock screen.
//
//   Locked    "Workstation Locked": the workstation is locked; press
//             Ctrl+Alt+Delete to unlock it. No buttons.
//   Unlock    "Unlock Workstation": user name and password, OK and Cancel.
//   Checking  Unlock with its controls disabled while PAM checks the
//             password.
//   Failed    The password was wrong; OK returns to Unlock.
class LockPrompt : public QWidget
{
    Q_OBJECT

public:
    enum class Page
    {
        Locked,
        Unlock,
        Checking,
        Failed
    };

    explicit LockPrompt(const LockUser &user, QWidget *parent = nullptr);

    Page page() const { return m_page; }
    void setPage(Page page);
    QString title() const;

    QLineEdit *passwordField() const { return m_password; }
    QPushButton *okButton() const { return m_ok; }
    QPushButton *cancelButton() const { return m_cancel; }
    QPushButton *failedOkButton() const { return m_failedOk; }

Q_SIGNALS:
    void unlockRequested(const QString &password);
    void cancelRequested();
    void failureAcknowledged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    int titleBarHeight() const;
    void updateMargins();

    Page m_page = Page::Locked;
    QWidget *m_lockedPage = nullptr;
    QWidget *m_unlockPage = nullptr;
    QWidget *m_failedPage = nullptr;
    QLineEdit *m_password = nullptr;
    QPushButton *m_ok = nullptr;
    QPushButton *m_cancel = nullptr;
    QPushButton *m_failedOk = nullptr;
};

// The lock screen on every output: the desktop colour with the lock dialog
// on the output that has keyboard focus. A password-protected screen saver
// starts black, with no dialog and no pointer, until the user does
// something. After a minute without input it goes back to how it started.
class LockController : public QObject
{
    Q_OBJECT

public:
    // Checks a password, calling done with the result, possibly later.
    using Authenticate
        = std::function<void(const QString &password, std::function<void(bool)> done)>;

    LockController(const LockUser &user, bool startBlank, Authenticate authenticate,
        QObject *parent = nullptr);
    ~LockController() override;

    // A full-screen widget for screen, not yet shown; it is deleted by
    // removeScreen() or with the controller.
    QWidget *addScreen(QScreen *screen);
    void removeScreen(QScreen *screen);
    QList<QWidget *> screens() const;

    LockPrompt *prompt() const { return m_prompt; }
    bool isBlank() const { return m_blank; }
    void setIdleTimeout(int milliseconds) { m_idle.setInterval(milliseconds); }

    // Whether Workstation Locked asks for Ctrl+Alt+Delete before Unlock
    // Workstation (true, NT's default). Off, as NT's DisableCAD policy, the
    // lock opens on Unlock Workstation, and Cancel, Escape and a minute
    // without input go back to it with the password cleared.
    bool requiresCtrlAltDelete() const { return m_requireCtrlAltDelete; }
    void setRequiresCtrlAltDelete(bool require);

    // The background of a screen while not blank.
    static QColor backgroundColour();

Q_SIGNALS:
    // The password was accepted.
    void unlocked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void wake();
    void reset();
    void rest();
    void updateScreens();
    void placePrompt();
    void checkPassword(const QString &password);

    bool m_startBlank;
    bool m_requireCtrlAltDelete = true;
    bool m_blank;
    Authenticate m_authenticate;
    QPointer<LockPrompt> m_prompt;
    QList<QPointer<QWidget>> m_screens;
    QTimer m_idle;
};

} // namespace Daytona
