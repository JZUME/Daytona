// SPDX-License-Identifier: GPL-3.0-or-later

#include "soundsproperties.h"

#include "eventsounds.h"
#include "settings.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Daytona {

namespace {

    constexpr int NameRole = Qt::UserRole;
    // The theme combo box's "No sounds".
    const QString NoSounds = QStringLiteral("\x01none");

    QIcon speakerIcon()
    {
        return QIcon::fromTheme(QStringLiteral("audio-volume-high"),
            QIcon::fromTheme(QStringLiteral("audio-x-generic")));
    }

    // A blank icon of the same size, so names line up whether or not they
    // have a sound.
    QIcon blankIcon(int size)
    {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        return QIcon(pixmap);
    }

} // namespace

SoundsPropertiesDialog::SoundsPropertiesDialog(
    Settings *settings, EventSounds *sounds, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_sounds(sounds)
    , m_soundDirs(soundThemeDirs())
{
    setWindowTitle(tr("Sounds Properties"));
    auto *tabs = new QTabWidget(this);
    auto *page = new QWidget;

    auto *eventsLabel = new QLabel(tr("&Events:"), page);
    m_events = new QTreeWidget(page);
    m_events->setHeaderHidden(true);
    m_events->setRootIsDecorated(true);
    m_events->setSelectionMode(QAbstractItemView::SingleSelection);
    m_events->setMinimumSize(340, 232);
    m_events->setIconSize(QSize(16, 16));
    eventsLabel->setBuddy(m_events);
    QTreeWidgetItem *alerts = new QTreeWidgetItem(m_events, { tr("Alerts") });
    QTreeWidgetItem *notifications = new QTreeWidgetItem(m_events, { tr("Notifications") });
    for (QTreeWidgetItem *group : { alerts, notifications }) {
        group->setFlags(Qt::ItemIsEnabled);
        group->setIcon(0, QIcon::fromTheme(QStringLiteral("folder")));
    }
    for (const SoundEvent &event : soundEvents()) {
        auto *item = new QTreeWidgetItem(
            event.group == SoundEvent::Group::Alerts ? alerts : notifications, { event.name });
        item->setData(0, NameRole, event.name);
        item->setToolTip(0, event.description);
    }
    m_events->expandAll();
    m_description = new QLabel(page);
    m_description->setWordWrap(true);

    auto *soundGroup = new QGroupBox(tr("Sound"), page);
    auto *nameLabel = new QLabel(tr("&Name:"), soundGroup);
    m_names = new QComboBox(soundGroup);
    m_names->setMinimumWidth(200);
    nameLabel->setBuddy(m_names);
    m_browse = new QPushButton(tr("&Browse..."), soundGroup);
    m_browse->setMinimumSize(75, 23);
    auto *previewLabel = new QLabel(tr("Preview:"), soundGroup);
    m_preview = new QToolButton(soundGroup);
    m_preview->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    m_preview->setToolTip(tr("Play"));
    m_preview->setAccessibleName(tr("Preview"));
    m_preview->setMinimumSize(23, 23);
    auto *soundLayout = new QGridLayout(soundGroup);
    soundLayout->addWidget(nameLabel, 0, 0);
    soundLayout->addWidget(m_names, 1, 0);
    soundLayout->addWidget(m_browse, 1, 1);
    soundLayout->addWidget(previewLabel, 0, 2);
    soundLayout->addWidget(m_preview, 1, 2, Qt::AlignLeft);
    soundLayout->setColumnStretch(0, 1);

    auto *themeGroup = new QGroupBox(tr("Sound &theme"), page);
    m_themes = new QComboBox(themeGroup);
    auto *themeLayout = new QVBoxLayout(themeGroup);
    themeLayout->addWidget(m_themes);

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(eventsLabel);
    layout->addWidget(m_events, 1);
    layout->addWidget(m_description);
    layout->addWidget(soundGroup);
    layout->addWidget(themeGroup);
    tabs->addTab(page, tr("Sounds"));

    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    m_apply = new QPushButton(tr("&Apply"), this);
    for (QPushButton *b : { ok, cancel, m_apply })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &SoundsPropertiesDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &SoundsPropertiesDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &SoundsPropertiesDialog::apply);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    buttons->addWidget(m_apply);

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->addWidget(tabs);
    main->addLayout(buttons);

    connect(m_events, &QTreeWidget::currentItemChanged, this, [this] { fillNames(); });
    connect(m_names, &QComboBox::activated, this, [this] { nameChosen(); });
    connect(m_themes, &QComboBox::activated, this, [this] { themeChosen(); });
    connect(m_browse, &QPushButton::clicked, this, &SoundsPropertiesDialog::browse);
    connect(m_preview, &QToolButton::clicked, this, [this] {
        if (m_sounds && !selectedFile().isEmpty())
            m_sounds->preview(selectedFile());
    });
}

void SoundsPropertiesDialog::prepare()
{
    m_updating = true;
    m_scheme = m_sounds ? m_sounds->scheme() : SoundScheme();
    m_choices.clear();
    for (const SoundEvent &event : soundEvents())
        m_choices.insert(event.name, m_settings ? m_settings->eventSound(event.name) : QString());

    m_themes->clear();
    bool found = false;
    for (const SoundThemeInfo &theme : installedSoundThemes(m_soundDirs)) {
        // With the name other programs' settings use, where it differs.
        m_themes->addItem(theme.name == theme.id
                ? theme.name
                : QStringLiteral("%1 (%2)").arg(theme.name, theme.id),
            theme.id);
        found = found || theme.id == m_scheme.theme;
    }
    // A theme set elsewhere that is not installed (yet) is kept.
    if (!found)
        m_themes->addItem(m_scheme.theme, m_scheme.theme);
    m_themes->addItem(tr("No sounds"), NoSounds);
    m_themes->setCurrentIndex(m_themes->findData(m_scheme.enabled ? m_scheme.theme : NoSounds));

    updateEventIcons();
    if (!m_events->currentItem())
        selectEvent(soundEvents().first().name);
    m_updating = false;
    fillNames();
    m_apply->setEnabled(false);
}

void SoundsPropertiesDialog::selectEvent(const QString &name)
{
    for (QTreeWidgetItemIterator it(m_events); *it; ++it) {
        if ((*it)->data(0, NameRole).toString() == name) {
            m_events->setCurrentItem(*it);
            return;
        }
    }
}

QString SoundsPropertiesDialog::selectedEvent() const
{
    const QTreeWidgetItem *item = m_events->currentItem();
    return item ? item->data(0, NameRole).toString() : QString();
}

QString SoundsPropertiesDialog::themeFile(const QString &name) const
{
    const SoundEvent *event = soundEvent(name);
    if (!event || !m_scheme.enabled)
        return {};
    const QString choice = m_choices.value(name);
    if (choice == Settings::EventSoundNone)
        return {};
    if (!choice.isEmpty())
        return choice;
    return findEventSoundFile(m_soundDirs, m_scheme.theme, *event);
}

QString SoundsPropertiesDialog::selectedFile() const
{
    return themeFile(selectedEvent());
}

void SoundsPropertiesDialog::fillNames()
{
    if (m_updating)
        return;
    m_updating = true;
    const QString event = selectedEvent();
    const QString current = selectedFile();
    m_names->clear();
    m_names->addItem(tr("(None)"), QString());
    if (m_scheme.enabled) {
        QStringList files = soundThemeFiles(m_soundDirs, m_scheme.theme);
        // The theme's sound may come from a theme it inherits, and a sound
        // chosen with Browse... from anywhere.
        const QString themed = event.isEmpty() || !soundEvent(event)
            ? QString()
            : findEventSoundFile(m_soundDirs, m_scheme.theme, *soundEvent(event));
        for (const QString &extra : { themed, current }) {
            if (!extra.isEmpty() && !files.contains(extra))
                files.prepend(extra);
        }
        for (const QString &file : std::as_const(files)) {
            m_names->addItem(QFileInfo(file).completeBaseName(), file);
            m_names->setItemData(m_names->count() - 1, file, Qt::ToolTipRole);
        }
    }
    m_names->setCurrentIndex(qMax(0, m_names->findData(current)));
    const bool enabled = !event.isEmpty() && m_scheme.enabled;
    m_names->setEnabled(enabled);
    m_browse->setEnabled(enabled);
    m_preview->setEnabled(enabled && !current.isEmpty() && m_sounds);
    const SoundEvent *info = soundEvent(event);
    m_description->setText(info ? info->description : QString());
    m_updating = false;
}

void SoundsPropertiesDialog::updateEventIcons()
{
    const QIcon speaker = speakerIcon();
    const QIcon blank = blankIcon(16);
    for (QTreeWidgetItemIterator it(m_events); *it; ++it) {
        const QString name = (*it)->data(0, NameRole).toString();
        if (!name.isEmpty())
            (*it)->setIcon(0, themeFile(name).isEmpty() ? blank : speaker);
    }
}

void SoundsPropertiesDialog::chooseFile(const QString &file)
{
    const QString event = selectedEvent();
    if (event.isEmpty() || !m_scheme.enabled)
        return;
    const SoundEvent *info = soundEvent(event);
    const QString themed
        = info ? findEventSoundFile(m_soundDirs, m_scheme.theme, *info) : QString();
    // The theme's own sound is stored as no choice, so it follows the theme.
    QString choice;
    if (file.isEmpty())
        choice = themed.isEmpty() ? QString() : Settings::EventSoundNone;
    else if (file != themed)
        choice = QFileInfo(file).absoluteFilePath();
    m_choices.insert(event, choice);
    updateEventIcons();
    fillNames();
    changed();
}

void SoundsPropertiesDialog::nameChosen()
{
    if (!m_updating)
        chooseFile(m_names->currentData().toString());
}

void SoundsPropertiesDialog::themeChosen()
{
    if (m_updating)
        return;
    const QString data = m_themes->currentData().toString();
    if (data == NoSounds) {
        m_scheme.enabled = false;
    } else {
        m_scheme.enabled = true;
        m_scheme.theme = data;
    }
    // As NT's schemes did, a theme sets every event's sound.
    for (auto it = m_choices.begin(); it != m_choices.end(); ++it)
        it.value().clear();
    updateEventIcons();
    fillNames();
    changed();
}

void SoundsPropertiesDialog::browse()
{
    QString start = QFileInfo(selectedFile()).absolutePath();
    if (selectedFile().isEmpty())
        start = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    const QString file
        = QFileDialog::getOpenFileName(this, tr("Browse for %1").arg(selectedEvent()), start,
            tr("Sounds (*.oga *.ogg *.wav);;All files (*)"));
    if (!file.isEmpty())
        chooseFile(file);
}

void SoundsPropertiesDialog::changed()
{
    m_apply->setEnabled(true);
}

void SoundsPropertiesDialog::apply()
{
    if (m_settings) {
        for (auto it = m_choices.cbegin(); it != m_choices.cend(); ++it)
            m_settings->setEventSound(it.key(), it.value());
    }
    if (m_sounds && !(m_sounds->scheme() == m_scheme))
        m_sounds->saveScheme(m_scheme);
    m_apply->setEnabled(false);
}

void SoundsPropertiesDialog::accept()
{
    apply();
    QDialog::accept();
}

} // namespace Daytona
