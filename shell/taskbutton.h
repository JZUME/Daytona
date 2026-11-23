// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QToolButton>

namespace Daytona {

struct Toplevel;

// One window's taskbar button: its icon and title, shown pressed and bold
// while the window is active. The compositor decides the state; clicking
// never toggles the button by itself.
class TaskButton : public QToolButton
{
    Q_OBJECT

public:
    explicit TaskButton(quint32 id, QWidget *parent = nullptr);

    quint32 id() const { return m_id; }
    void update(const Toplevel &toplevel);

    // The icon for an app id: the icon theme's icon of that name (or its
    // last dotted part), else a generic application icon.
    static QIcon iconForAppId(const QString &appId);

Q_SIGNALS:
    void windowMenuRequested(const QPoint &globalPos);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void nextCheckState() override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateText();

    quint32 m_id;
    QString m_title;
    QString m_appId;
};

} // namespace Daytona
