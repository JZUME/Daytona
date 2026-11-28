// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QPoint>
#include <QPointer>
#include <QWidget>

class QScreen;

namespace Daytona {

// NT's Blank Screen screen saver: every screen black, above everything,
// until the mouse moves or a key or button is pressed. Without password
// protection; a protected screen saver is a lock (ScreenLocker) instead.
class BlankScreen : public QObject
{
    Q_OBJECT

public:
    explicit BlankScreen(QObject *parent = nullptr);
    ~BlankScreen() override;

    void start();
    void stop();
    bool isActive() const { return !m_windows.isEmpty(); }
    QList<QWidget *> windows() const;

    // Whether an input event ends the screen saver: any key or button press,
    // or the pointer moving more than a few pixels from where it was first
    // seen. Events in the first moments are ignored, so the click or key
    // that started a preview does not end it at once.
    bool endsScreenSaver(QEvent *event);

Q_SIGNALS:
    void started();
    void stopped();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QList<QPointer<QWidget>> m_windows;
    QElapsedTimer m_shown;
    bool m_seenPointer = false;
    QPoint m_firstPointer;
};

} // namespace Daytona
