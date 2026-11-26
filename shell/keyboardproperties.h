// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "keyboardlayout.h"
#include "settings.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTreeWidget;

namespace Daytona {

// NT's Add Input Locale: a language, then one of its keyboard layouts.
class AddInputLocaleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddInputLocaleDialog(const LayoutRegistry *registry, QWidget *parent = nullptr);

    QComboBox *languageBox() const { return m_languages; }
    QComboBox *layoutBox() const { return m_layouts; }
    InputLocale selectedLocale() const;

private:
    void updateLayouts();

    const LayoutRegistry *m_registry;
    QComboBox *m_languages;
    QComboBox *m_layouts;
};

// NT's Keyboard Properties with its Input Locales tab: the installed input
// locales (keyboard layouts), the default one and the hot key that switches
// between them. Applied straight away through labwc, and kept for the
// next log on.
class KeyboardPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyboardPropertiesDialog(Settings *settings, QWidget *parent = nullptr);

    // Reads the current settings.
    void prepare();
    // The system's keyboard file, for tests (defaultSystemKeyboardFile()).
    void setSystemKeyboardFile(const QString &path) { m_systemFile = path; }

    QTreeWidget *localeList() const { return m_list; }
    QPushButton *addButton() const { return m_add; }
    QPushButton *removeButton() const { return m_remove; }
    QPushButton *defaultButton() const { return m_setDefault; }
    QPushButton *applyButton() const { return m_apply; }
    QRadioButton *hotkeyButton(LayoutHotkey hotkey) const;
    QLabel *defaultLabel() const { return m_defaultLabel; }

    QList<InputLocale> locales() const { return m_locales; }
    LayoutHotkey hotkey() const;
    const LayoutRegistry &registry() const { return m_registry; }

    // Adds a locale to the end of the list, as Add... does.
    void addLocale(const InputLocale &locale);
    void removeSelected();
    void setSelectedAsDefault();

public Q_SLOTS:
    void accept() override;
    void apply();

Q_SIGNALS:
    // The settings were saved; the session's keyboard should follow them.
    void applied();

private:
    void rebuildList(int select);
    void updateButtons();
    void changed();

    Settings *m_settings;
    QString m_systemFile = defaultSystemKeyboardFile();
    LayoutRegistry m_registry;
    QList<InputLocale> m_locales;
    QTreeWidget *m_list;
    QPushButton *m_add;
    QPushButton *m_remove;
    QPushButton *m_setDefault;
    QLabel *m_defaultLabel;
    QRadioButton *m_leftAltShift;
    QRadioButton *m_ctrlShift;
    QRadioButton *m_noHotkey;
    QPushButton *m_apply;
    bool m_preparing = false;
};

} // namespace Daytona
