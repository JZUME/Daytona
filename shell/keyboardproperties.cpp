// SPDX-License-Identifier: GPL-3.0-or-later

#include "keyboardproperties.h"

#include "inputlocaleicon.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Daytona {

namespace {

    QString languageOf(const LayoutInfo &info)
    {
        return info.languages.isEmpty() ? info.description
                                        : LayoutRegistry::languageName(info.languages.first());
    }

    QPushButton *dialogButton(const QString &text, QWidget *parent)
    {
        auto *button = new QPushButton(text, parent);
        button->setMinimumSize(75, 23);
        return button;
    }

} // namespace

AddInputLocaleDialog::AddInputLocaleDialog(const LayoutRegistry *registry, QWidget *parent)
    : QDialog(parent)
    , m_registry(registry)
{
    setWindowTitle(tr("Add Input Locale"));
    m_languages = new QComboBox(this);
    m_layouts = new QComboBox(this);

    // Every language some layout is for, by name.
    QMap<QString, QString> languages;
    for (const LayoutInfo &info : registry->layouts()) {
        for (const QString &code : info.languages)
            languages.insert(LayoutRegistry::languageName(code), code);
    }
    for (auto it = languages.cbegin(); it != languages.cend(); ++it)
        m_languages->addItem(it.key(), it.value());
    // The user's language to start with, else English.
    int own = m_languages->findText(QLocale::languageToString(QLocale().language()));
    if (own < 0)
        own = m_languages->findData(QStringLiteral("eng"));
    m_languages->setCurrentIndex(qMax(0, own));
    connect(
        m_languages, &QComboBox::currentIndexChanged, this, &AddInputLocaleDialog::updateLayouts);
    updateLayouts();

    auto *form = new QFormLayout;
    form->addRow(tr("&Input locale:"), m_languages);
    form->addRow(tr("&Keyboard layout:"), m_layouts);

    auto *ok = dialogButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = dialogButton(tr("Cancel"), this);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);

    m_languages->setMinimumWidth(220);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
}

void AddInputLocaleDialog::updateLayouts()
{
    const QString language = m_languages->currentData().toString();
    QMap<QString, QString> layouts; // description: code
    QString main; // the language's own layout, such as German for German
    for (const LayoutInfo &info : m_registry->layouts()) {
        if (!info.languages.contains(language))
            continue;
        layouts.insert(info.description, info.locale.code());
        if (main.isEmpty() && info.locale.variant.isEmpty() && info.languages.first() == language)
            main = info.locale.code();
    }
    m_layouts->clear();
    for (auto it = layouts.cbegin(); it != layouts.cend(); ++it)
        m_layouts->addItem(it.key(), it.value());
    const int exact = m_layouts->findText(m_languages->currentText());
    m_layouts->setCurrentIndex(exact >= 0 ? exact : qMax(0, m_layouts->findData(main)));
}

InputLocale AddInputLocaleDialog::selectedLocale() const
{
    return InputLocale::fromCode(m_layouts->currentData().toString());
}

KeyboardPropertiesDialog::KeyboardPropertiesDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Keyboard Properties"));
    auto *tabs = new QTabWidget(this);
    auto *page = new QWidget;

    auto *installed = new QGroupBox(tr("Installed input locales"), page);
    m_list = new QTreeWidget(installed);
    m_list->setColumnCount(2);
    m_list->setHeaderLabels({ tr("Language"), tr("Layout") });
    m_list->setRootIsDecorated(false);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->setMinimumSize(340, 120);
    connect(
        m_list, &QTreeWidget::currentItemChanged, this, &KeyboardPropertiesDialog::updateButtons);

    m_add = dialogButton(tr("A&dd..."), installed);
    m_remove = dialogButton(tr("R&emove"), installed);
    connect(m_add, &QPushButton::clicked, this, [this] {
        AddInputLocaleDialog dialog(&m_registry, this);
        if (dialog.exec() == QDialog::Accepted)
            addLocale(dialog.selectedLocale());
    });
    connect(m_remove, &QPushButton::clicked, this, &KeyboardPropertiesDialog::removeSelected);
    auto *listButtons = new QHBoxLayout;
    listButtons->addWidget(m_add);
    listButtons->addWidget(m_remove);
    listButtons->addStretch();

    m_defaultLabel = new QLabel(installed);
    m_defaultLabel->setWordWrap(true);
    m_setDefault = dialogButton(tr("Set as De&fault"), installed);
    connect(
        m_setDefault, &QPushButton::clicked, this, &KeyboardPropertiesDialog::setSelectedAsDefault);
    auto *defaultRow = new QHBoxLayout;
    defaultRow->addWidget(m_defaultLabel, 1);
    defaultRow->addWidget(m_setDefault);

    auto *installedLayout = new QVBoxLayout(installed);
    installedLayout->addWidget(m_list, 1);
    installedLayout->addLayout(listButtons);
    installedLayout->addLayout(defaultRow);

    auto *switching = new QGroupBox(tr("Switch locales"), page);
    m_leftAltShift = new QRadioButton(tr("Left &Alt+Shift"), switching);
    m_ctrlShift = new QRadioButton(tr("&Ctrl+Shift"), switching);
    m_noHotkey = new QRadioButton(tr("&None"), switching);
    auto *switchingLayout = new QHBoxLayout(switching);
    for (QRadioButton *button : { m_leftAltShift, m_ctrlShift, m_noHotkey }) {
        switchingLayout->addWidget(button);
        connect(button, &QRadioButton::toggled, this, [this](bool on) {
            if (on)
                changed();
        });
    }
    switchingLayout->addStretch();

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->addWidget(installed, 1);
    pageLayout->addWidget(switching);
    tabs->addTab(page, tr("Input Locales"));

    auto *ok = dialogButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = dialogButton(tr("Cancel"), this);
    m_apply = dialogButton(tr("&Apply"), this);
    connect(ok, &QPushButton::clicked, this, &KeyboardPropertiesDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &KeyboardPropertiesDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &KeyboardPropertiesDialog::apply);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    buttons->addWidget(m_apply);

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->addWidget(tabs);
    main->addLayout(buttons);
    prepare();
}

QRadioButton *KeyboardPropertiesDialog::hotkeyButton(LayoutHotkey hotkey) const
{
    switch (hotkey) {
    case LayoutHotkey::LeftAltShift:
        return m_leftAltShift;
    case LayoutHotkey::CtrlShift:
        return m_ctrlShift;
    case LayoutHotkey::None:
        break;
    }
    return m_noHotkey;
}

LayoutHotkey KeyboardPropertiesDialog::hotkey() const
{
    if (m_ctrlShift->isChecked())
        return LayoutHotkey::CtrlShift;
    if (m_noHotkey->isChecked())
        return LayoutHotkey::None;
    return LayoutHotkey::LeftAltShift;
}

void KeyboardPropertiesDialog::prepare()
{
    m_preparing = true;
    const XkbConfig system = systemKeyboard(m_systemFile);
    const XkbConfig config = sessionKeyboard(m_settings, system);
    m_locales = config.locales;
    // Unchosen, the hot key is the system's (or NT's default).
    const bool chosen = m_settings && !m_settings->keyboardLayouts().isEmpty();
    hotkeyButton(chosen ? m_settings->layoutHotkey() : system.hotkey())->setChecked(true);
    rebuildList(0);
    m_apply->setEnabled(false);
    m_preparing = false;
}

void KeyboardPropertiesDialog::rebuildList(int select)
{
    m_list->clear();
    const qreal ratio = devicePixelRatioF();
    for (const InputLocale &locale : std::as_const(m_locales)) {
        const LayoutInfo info = m_registry.find(locale);
        QPixmap pixmap(QSize(16, 16) * ratio);
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        InputLocaleIcon::paintIndicator(
            &painter, QRect(0, 0, 16, 16), LayoutRegistry::indicatorText(info), palette(), font());
        painter.end();
        auto *item = new QTreeWidgetItem(m_list, { languageOf(info), info.description });
        item->setIcon(0, QIcon(pixmap));
    }
    m_list->setCurrentItem(
        m_list->topLevelItem(qBound(0, select, m_list->topLevelItemCount() - 1)));
    const LayoutInfo first = m_registry.find(m_locales.value(0));
    m_defaultLabel->setText(
        tr("Default input locale: %1 - %2").arg(languageOf(first), first.description));
    updateButtons();
}

void KeyboardPropertiesDialog::updateButtons()
{
    const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
    m_remove->setEnabled(row >= 0 && m_locales.size() > 1);
    m_setDefault->setEnabled(row > 0);
    // The hot key matters only with more than one locale, as in NT.
    for (QRadioButton *button : { m_leftAltShift, m_ctrlShift, m_noHotkey })
        button->setEnabled(m_locales.size() > 1);
}

void KeyboardPropertiesDialog::changed()
{
    if (!m_preparing)
        m_apply->setEnabled(true);
}

void KeyboardPropertiesDialog::addLocale(const InputLocale &locale)
{
    if (!locale.isValid())
        return;
    int row = m_locales.indexOf(locale);
    if (row < 0) {
        m_locales << locale;
        row = m_locales.size() - 1;
        changed();
    }
    rebuildList(row);
}

void KeyboardPropertiesDialog::removeSelected()
{
    const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
    if (row < 0 || m_locales.size() < 2)
        return;
    m_locales.removeAt(row);
    changed();
    rebuildList(row);
}

void KeyboardPropertiesDialog::setSelectedAsDefault()
{
    const int row = m_list->indexOfTopLevelItem(m_list->currentItem());
    if (row <= 0)
        return;
    m_locales.move(row, 0);
    changed();
    rebuildList(0);
}

void KeyboardPropertiesDialog::apply()
{
    if (m_settings && m_apply->isEnabled()) {
        QStringList codes;
        for (const InputLocale &locale : std::as_const(m_locales))
            codes << locale.code();
        m_settings->setKeyboardLayouts(codes);
        m_settings->setLayoutHotkey(hotkey());
        Q_EMIT applied();
    }
    m_apply->setEnabled(false);
}

void KeyboardPropertiesDialog::accept()
{
    apply();
    QDialog::accept();
}

} // namespace Daytona
