// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <functional>

namespace Daytona {

class ToplevelList;

// NT's End Task prompt, shown when a program has not closed while logging
// off or shutting down: End Task, Wait or Cancel.
class EndTaskDialog : public QDialog
{
    Q_OBJECT

public:
    enum Answer
    {
        EndTask = QDialog::Accepted + 1,
        Wait,
        Cancel
    };

    explicit EndTaskDialog(const QString &program, QWidget *parent = nullptr);
};

// Closes programs before the session ends. NT asked every program to close
// before logging off or shutting down, so that each could save its work or
// ask the user to; Daytona asks every window to close through the
// compositor (as the window menu's Close does) and waits until they have.
//
// A program that has not closed within the timeout may be busy or waiting
// for the user (for example asking whether to save), so the End Task prompt
// names it: End Task stops waiting for it (it ends with the session), Wait
// gives it more time and Cancel abandons ending the session. The prompt
// goes away by itself if the program closes meanwhile.
class ProgramCloser : public QObject
{
    Q_OBJECT

public:
    using Close = std::function<void(quint32 id)>;

    // close asks the compositor to close one window of list.
    ProgramCloser(ToplevelList *list, Close close, QObject *parent = nullptr);
    ~ProgramCloser() override;

    // How long programs have to close before the End Task prompt. NT's
    // default (WaitToKillAppTimeout) was 20 seconds.
    static constexpr int DefaultTimeout = 20000;
    void setTimeout(int milliseconds);

    // Asks every open window to close and calls done once all have closed
    // or the user chose End Task for those that remain. done is not called
    // if the user cancels. A second call while closing replaces done.
    void closeAll(std::function<void()> done);
    bool isClosing() const { return bool(m_done); }

    // The End Task prompt, replaceable for tests: showPrompt names the
    // program, and the answer comes back through answer(). hidePrompt is
    // called when the program closes before the user has answered.
    std::function<void(const QString &program)> showPrompt;
    std::function<void()> hidePrompt;
    void answer(EndTaskDialog::Answer answer);

    // The window the End Task prompt is about, or 0.
    quint32 asking() const { return m_asking; }

private:
    void windowClosed(quint32 id);
    void timedOut();
    void promptNext();
    void finish();
    void reset();

    ToplevelList *m_list;
    Close m_close;
    QTimer m_timer;
    std::function<void()> m_done;
    QList<quint32> m_waiting; // asked to close and still open, in launch order
    QSet<quint32> m_ended; // the user chose End Task for these
    quint32 m_asking = 0;
    QPointer<EndTaskDialog> m_dialog;
};

} // namespace Daytona
