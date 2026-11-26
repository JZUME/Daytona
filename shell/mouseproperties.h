// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings.h"

#include <QDialog>
#include <QPixmap>

class QComboBox;
class QPushButton;
class QTreeWidget;

namespace Daytona {

// A pointer at a scheme's size, from Daytona's cursor theme, or a null
// pixmap when the theme is not installed. ratio is the screen's device
// pixel ratio.
QPixmap pointerPreview(const QString &cursorName, PointerScheme scheme, qreal ratio);

// NT's Mouse Properties with its Pointers tab: a pointer scheme (Standard,
// Large or Extra Large) and the pointers it has. Applied at the next log
// on, when daytona-session sets the cursor size for everything.
class MousePropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MousePropertiesDialog(Settings *settings, QWidget *parent = nullptr);

    // Reads the current scheme.
    void prepare();

    QComboBox *schemeBox() const { return m_schemes; }
    QTreeWidget *pointerList() const { return m_pointers; }
    QPushButton *applyButton() const { return m_apply; }
    PointerScheme selectedScheme() const;

public Q_SLOTS:
    void accept() override;
    void apply();

private:
    void updatePointers();

    Settings *m_settings;
    QComboBox *m_schemes;
    QTreeWidget *m_pointers;
    QPushButton *m_apply;
    bool m_preparing = false;
};

} // namespace Daytona
