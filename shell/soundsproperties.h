// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "soundtheme.h"

#include <QDialog>
#include <QHash>

class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace Daytona {

class EventSounds;
class Settings;

// NT's Sounds Properties, in the freedesktop terms the rest of the system
// uses: the events (by their Sound Naming Specification names, grouped as
// the specification groups them), the sound chosen for the selected one
// (Name, Browse..., Preview) and the sound theme in place of NT's schemes,
// "No sounds" turning event sounds off. The theme is the desktop-wide one
// (see SoundScheme); as in NT, choosing a theme gives every event that
// theme's sound again.
class SoundsPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    SoundsPropertiesDialog(Settings *settings, EventSounds *sounds, QWidget *parent = nullptr);

    // Where sound themes are looked for (soundThemeDirs() by default).
    void setSoundDirs(const QStringList &dirs) { m_soundDirs = dirs; }

    // Reads the current choices.
    void prepare();

    QTreeWidget *eventList() const { return m_events; }
    QComboBox *nameBox() const { return m_names; }
    QComboBox *themeBox() const { return m_themes; }
    QPushButton *browseButton() const { return m_browse; }
    QToolButton *previewButton() const { return m_preview; }
    QPushButton *applyButton() const { return m_apply; }

    // Selects the event called name in the list.
    void selectEvent(const QString &name);
    // The file the selected event would play, or empty for none.
    QString selectedFile() const;
    // Chooses file for the selected event, as Browse... does.
    void chooseFile(const QString &file);

public Q_SLOTS:
    void accept() override;
    void apply();

private:
    QString selectedEvent() const;
    QString themeFile(const QString &event) const;
    void fillNames();
    void updateEventIcons();
    void nameChosen();
    void themeChosen();
    void browse();
    void changed();

    Settings *m_settings;
    EventSounds *m_sounds;
    QStringList m_soundDirs;
    SoundScheme m_scheme;
    // Settings::eventSound for each event, as chosen here.
    QHash<QString, QString> m_choices;
    QTreeWidget *m_events;
    QLabel *m_description;
    QComboBox *m_names;
    QPushButton *m_browse;
    QToolButton *m_preview;
    QComboBox *m_themes;
    QPushButton *m_apply;
    bool m_updating = false;
};

} // namespace Daytona
