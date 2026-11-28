// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPointer>
#include <QWidget>

namespace Daytona {

class StatusNotifierItem;

// One 16 pixel icon in the notification area.
class TrayIcon : public QWidget
{
    Q_OBJECT

public:
    explicit TrayIcon(StatusNotifierItem *item, QWidget *parent = nullptr);

    StatusNotifierItem *item() const { return m_item; }
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void itemChanged();
    QPoint screenPosition(const QPoint &pos) const;

    QPointer<StatusNotifierItem> m_item;
};

} // namespace Daytona
