// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keyboardlayout.h"

#include <QWidget>

class QMenu;

namespace Daytona {

class InputLocales;

// NT's input locale indicator in the notification area: the language of
// the keyboard layout in use ("En", "De") in a small highlighted box.
// Clicking it shows the installed locales to choose from; its right-click
// menu opens Keyboard Properties. It is shown only while more than one
// locale is installed, as in NT.
class InputLocaleIcon : public QWidget
{
    Q_OBJECT

public:
    explicit InputLocaleIcon(InputLocales *locales, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QString text() const { return m_text; }
    // The menu a click shows, rebuilt for the current locales.
    QMenu *localeMenu();

    // Draws the indicator's box with text in rect.
    static void paintIndicator(QPainter *painter, const QRect &rect, const QString &text,
        const QPalette &palette, const QFont &font);

Q_SIGNALS:
    void propertiesRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void stateChanged();
    void showMenu(QMenu *menu);

    InputLocales *m_locales;
    LayoutRegistry m_registry;
    QString m_text;
    QMenu *m_menu = nullptr;
};

} // namespace Daytona
