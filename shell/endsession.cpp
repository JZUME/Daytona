// SPDX-License-Identifier: GPL-3.0-or-later

#include "endsession.h"

#include "toplevellist.h"

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QPushButton>
#include <QStyle>

Q_LOGGING_CATEGORY(lcEndSession, "daytona.session", QtInfoMsg)

namespace Daytona {

EndTaskDialog::EndTaskDialog(const QString &program, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("End Task - %1").arg(program));

    auto *icon = new QLabel(this);
    icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning, nullptr, this).pixmap(32));
    icon->setAlignment(Qt::AlignTop);
    // NT's wording, for a program that has been asked to close rather than
    // one that has stopped responding.
    auto *text = new QLabel(
        tr("This program has not closed. It may be busy, or waiting for a response from "
           "you, such as whether to save your work.\n\n"
           "To end the program now, click End Task. You will lose any unsaved information "
           "in this program.\n\n"
           "To give the program more time, click Wait.\n\n"
           "To return to the desktop, click Cancel."),
        this);
    text->setWordWrap(true);
    text->setFixedWidth(text->fontMetrics().averageCharWidth() * 52);

    auto *endTask = new QPushButton(tr("&End Task"), this);
    auto *wait = new QPushButton(tr("&Wait"), this);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    wait->setDefault(true);
    for (QPushButton *b : { endTask, wait, cancel })
        b->setMinimumSize(75, 23);
    connect(endTask, &QPushButton::clicked, this, [this] { done(EndTask); });
    connect(wait, &QPushButton::clicked, this, [this] { done(Wait); });
    connect(cancel, &QPushButton::clicked, this, [this] { done(Cancel); });

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 8, 0, 0);
    buttons->addStretch();
    buttons->addWidget(endTask);
    buttons->addWidget(wait);
    buttons->addWidget(cancel);
    buttons->addStretch();

    auto *grid = new QGridLayout(this);
    grid->addWidget(icon, 0, 0);
    grid->addWidget(text, 0, 1);
    grid->addLayout(buttons, 1, 0, 1, 2);
    grid->setSizeConstraint(QLayout::SetFixedSize);
}

ProgramCloser::ProgramCloser(ToplevelList *list, Close close, QObject *parent)
    : QObject(parent)
    , m_list(list)
    , m_close(std::move(close))
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(DefaultTimeout);
    connect(&m_timer, &QTimer::timeout, this, &ProgramCloser::timedOut);
    connect(m_list, &ToplevelList::removed, this, &ProgramCloser::windowClosed);

    showPrompt = [this](const QString &program) {
        delete m_dialog;
        m_dialog = new EndTaskDialog(program);
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_dialog, &QDialog::finished, this, [this](int result) {
            // Closing the dialog itself is Cancel, as in NT.
            answer(result == EndTaskDialog::EndTask || result == EndTaskDialog::Wait
                    ? EndTaskDialog::Answer(result)
                    : EndTaskDialog::Cancel);
        });
        m_dialog->show();
        m_dialog->raise();
        m_dialog->activateWindow();
    };
    hidePrompt = [this] {
        if (m_dialog) {
            m_dialog->disconnect(this);
            m_dialog->close();
        }
    };
}

ProgramCloser::~ProgramCloser()
{
    delete m_dialog;
}

void ProgramCloser::setTimeout(int milliseconds)
{
    m_timer.setInterval(milliseconds);
}

void ProgramCloser::closeAll(std::function<void()> done)
{
    if (isClosing()) {
        m_done = std::move(done);
        return;
    }
    m_done = std::move(done);
    // Only owner windows: a program's own dialogs go with it, and closing
    // one (a Save As dialog, say) would only cancel it.
    m_waiting = m_list->owners();
    if (m_waiting.isEmpty()) {
        finish();
        return;
    }
    qCInfo(lcEndSession) << "Asking" << m_waiting.size() << "windows to close";
    m_timer.start();
    for (quint32 id : std::as_const(m_waiting))
        m_close(id);
}

void ProgramCloser::windowClosed(quint32 id)
{
    if (!isClosing() || !m_waiting.removeOne(id))
        return;
    m_ended.remove(id);
    if (id == m_asking) {
        m_asking = 0;
        if (hidePrompt)
            hidePrompt();
        // The others have had their time too.
        promptNext();
        return;
    }
    if (m_asking == 0 && QSet<quint32>(m_waiting.cbegin(), m_waiting.cend()) == m_ended)
        finish();
}

void ProgramCloser::timedOut()
{
    if (isClosing() && m_asking == 0)
        promptNext();
}

void ProgramCloser::promptNext()
{
    for (quint32 id : std::as_const(m_waiting)) {
        if (m_ended.contains(id))
            continue;
        m_asking = id;
        QString program;
        if (const Toplevel *window = m_list->find(id))
            program = window->title.isEmpty() ? window->appId : window->title;
        qCInfo(lcEndSession) << "Window" << program << "has not closed";
        if (showPrompt)
            showPrompt(program);
        return;
    }
    finish();
}

void ProgramCloser::answer(EndTaskDialog::Answer answer)
{
    if (!isClosing() || m_asking == 0)
        return;
    const quint32 id = m_asking;
    m_asking = 0;
    switch (answer) {
    case EndTaskDialog::EndTask:
        m_ended.insert(id);
        promptNext();
        break;
    case EndTaskDialog::Wait:
        m_timer.start();
        break;
    case EndTaskDialog::Cancel:
        qCInfo(lcEndSession) << "Ending the session was cancelled";
        reset();
        break;
    }
}

void ProgramCloser::finish()
{
    std::function<void()> done = std::move(m_done);
    reset();
    if (done)
        done();
}

void ProgramCloser::reset()
{
    m_timer.stop();
    m_done = nullptr;
    m_waiting.clear();
    m_ended.clear();
    m_asking = 0;
}

} // namespace Daytona
