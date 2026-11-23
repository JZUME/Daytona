// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>

class QCheckBox;
class QPushButton;

namespace Daytona {

class Settings;

// The options a Taskbar Properties dialog edits.
struct TaskbarOptions
{
    bool alwaysOnTop = true;
    bool autoHide = false;
    bool smallIcons = false;
    bool showClock = true;
    bool locked = true;

    bool operator==(const TaskbarOptions &o) const
    {
        return alwaysOnTop == o.alwaysOnTop && autoHide == o.autoHide && smallIcons == o.smallIcons
            && showClock == o.showClock && locked == o.locked;
    }
    bool operator!=(const TaskbarOptions &o) const { return !(*this == o); }

    static TaskbarOptions fromSettings(const Settings &settings);
    void save(Settings *settings) const;
};

// NT's picture at the top of the Taskbar Options tab: a corner of the
// screen with a Start menu and the taskbar, showing the chosen options.
class TaskbarPreview : public QWidget
{
    Q_OBJECT

public:
    explicit TaskbarPreview(QWidget *parent = nullptr);
    void setOptions(const TaskbarOptions &options);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    TaskbarOptions m_options;
};

// NT's Taskbar Properties, Taskbar Options tab only, with "Lock the
// taskbar" added (an approved exception from later Windows).
class TaskbarPropertiesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TaskbarPropertiesDialog(Settings *settings, QWidget *parent = nullptr);

    // Reads the current settings into the controls.
    void prepare();
    TaskbarOptions options() const;

    QPushButton *applyButton() const { return m_apply; }

public Q_SLOTS:
    void accept() override;
    void apply();

private:
    void optionsChanged();

    Settings *m_settings;
    TaskbarPreview *m_preview;
    QCheckBox *m_onTop;
    QCheckBox *m_autoHide;
    QCheckBox *m_smallIcons;
    QCheckBox *m_showClock;
    QCheckBox *m_locked;
    QPushButton *m_apply;
};

} // namespace Daytona
