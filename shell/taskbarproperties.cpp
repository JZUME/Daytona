// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskbarproperties.h"

#include "bevel.h"
#include "settings.h"

#include <QCheckBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QIcon>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace Daytona {

TaskbarOptions TaskbarOptions::fromSettings(const Settings &settings)
{
    TaskbarOptions o;
    o.alwaysOnTop = settings.taskbarAlwaysOnTop();
    o.autoHide = settings.taskbarAutoHide();
    o.smallIcons = settings.smallStartMenuIcons();
    o.showClock = settings.showClock();
    o.locked = settings.taskbarLocked();
    return o;
}

void TaskbarOptions::save(Settings *settings) const
{
    settings->setTaskbarAlwaysOnTop(alwaysOnTop);
    settings->setTaskbarAutoHide(autoHide);
    settings->setSmallStartMenuIcons(smallIcons);
    settings->setShowClock(showClock);
    settings->setTaskbarLocked(locked);
}

TaskbarPreview::TaskbarPreview(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void TaskbarPreview::setOptions(const TaskbarOptions &options)
{
    m_options = options;
    update();
}

QSize TaskbarPreview::sizeHint() const
{
    return { 300, 110 };
}

void TaskbarPreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QPalette &pal = palette();
    // A corner of the desktop, sunken into the dialog.
    painter.fillRect(rect(), pal.color(QPalette::Window));
    const QRect screen = rect().adjusted(2, 2, -2, -2);
    Bevel::sunken(&painter, rect(), pal);
    painter.fillRect(screen, QColor(0, 128, 128));

    // The taskbar along the bottom, or a sliver of it when auto hidden.
    const int barHeight = m_options.autoHide ? 2 : 22;
    const QRect bar(screen.left(), screen.bottom() - barHeight + 1, screen.width(), barHeight);

    // A window behind the Start menu, overlapped by the taskbar unless the
    // taskbar is not always on top.
    const QRect window(screen.left() + 150, screen.top() + 12, 120, screen.height() - 12);
    if (m_options.alwaysOnTop || m_options.autoHide) {
        painter.fillRect(window, pal.color(QPalette::Window));
        Bevel::raised(&painter, window, pal);
        painter.fillRect(
            window.left() + 3, window.top() + 3, window.width() - 6, 10, QColor(0, 0, 128));
    }
    painter.fillRect(bar, pal.color(QPalette::Button));
    painter.fillRect(bar.left(), bar.top(), bar.width(), 1, pal.color(QPalette::Light));
    if (!m_options.alwaysOnTop && !m_options.autoHide) {
        painter.fillRect(window, pal.color(QPalette::Window));
        Bevel::raised(&painter, window, pal);
        painter.fillRect(
            window.left() + 3, window.top() + 3, window.width() - 6, 10, QColor(0, 0, 128));
    }

    if (!m_options.autoHide) {
        // Start button and clock.
        const QRect start(bar.left() + 2, bar.top() + 3, 44, bar.height() - 5);
        Bevel::raised(&painter, start, pal);
        QFont bold = font();
        bold.setBold(true);
        painter.setFont(bold);
        painter.setPen(pal.color(QPalette::ButtonText));
        painter.drawText(start, Qt::AlignCenter, tr("Start"));
        painter.setFont(font());
        if (m_options.showClock) {
            const QString time = QLocale()
                                     .toString(QTime(12, 45), QLocale::ShortFormat)
                                     .remove(QStringLiteral(":00"));
            const int w = painter.fontMetrics().horizontalAdvance(time) + 10;
            const QRect clock(bar.right() - w - 1, bar.top() + 3, w, bar.height() - 5);
            Bevel::sunkenThin(&painter, clock, pal);
            painter.drawText(clock, Qt::AlignCenter, time);
        }
    }

    // A Start menu with large or small icons, above the Start button.
    const int itemHeight = m_options.smallIcons ? 14 : 22;
    const int items = 3;
    const QRect menu(
        screen.left() + 2, bar.top() - items * itemHeight - 6, 100, items * itemHeight + 6);
    painter.fillRect(menu, pal.color(QPalette::Button));
    Bevel::raised(&painter, menu, pal);
    const QRect banner(
        menu.left() + 3, menu.top() + 3, m_options.smallIcons ? 0 : 12, menu.height() - 6);
    painter.fillRect(banner, pal.color(QPalette::Dark));
    const int iconSize = m_options.smallIcons ? 12 : 18;
    const char *icons[] = { "system-file-manager", "preferences-system", "system-run" };
    for (int i = 0; i < items; ++i) {
        const int y = menu.top() + 3 + i * itemHeight + (itemHeight - iconSize) / 2;
        const QIcon icon = QIcon::fromTheme(QLatin1String(icons[i]));
        icon.paint(&painter, QRect(banner.right() + 4, y, iconSize, iconSize));
        painter.fillRect(banner.right() + 8 + iconSize, y + iconSize / 2 - 1, 40, 3,
            pal.color(QPalette::ButtonText));
    }
}

TaskbarPropertiesDialog::TaskbarPropertiesDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Taskbar Properties"));

    auto *tabs = new QTabWidget(this);
    auto *page = new QWidget(tabs);
    tabs->addTab(page, tr("Taskbar Options"));

    m_preview = new TaskbarPreview(page);
    m_onTop = new QCheckBox(tr("&Always on top"), page);
    m_autoHide = new QCheckBox(tr("A&uto hide"), page);
    m_smallIcons = new QCheckBox(tr("Show s&mall icons in Start menu"), page);
    m_showClock = new QCheckBox(tr("Show &Clock"), page);
    m_locked = new QCheckBox(tr("&Lock the taskbar"), page);

    auto *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(8, 8, 8, 8);
    pageLayout->addWidget(m_preview, 0, Qt::AlignHCenter);
    pageLayout->addSpacing(6);
    for (QCheckBox *box : { m_onTop, m_autoHide, m_smallIcons, m_showClock, m_locked }) {
        pageLayout->addWidget(box);
        connect(box, &QCheckBox::toggled, this, &TaskbarPropertiesDialog::optionsChanged);
    }
    pageLayout->addStretch();

    // NT's buttons: OK, Cancel, Apply.
    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    m_apply = new QPushButton(tr("&Apply"), this);
    for (QPushButton *b : { ok, cancel, m_apply })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &TaskbarPropertiesDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &TaskbarPropertiesDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &TaskbarPropertiesDialog::apply);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    buttons->addWidget(m_apply);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(tabs);
    layout->addLayout(buttons);
    prepare();
    setFixedSize(sizeHint());
}

void TaskbarPropertiesDialog::prepare()
{
    const TaskbarOptions o
        = m_settings ? TaskbarOptions::fromSettings(*m_settings) : TaskbarOptions();
    const QSignalBlocker b1(m_onTop), b2(m_autoHide), b3(m_smallIcons), b4(m_showClock),
        b5(m_locked);
    m_onTop->setChecked(o.alwaysOnTop);
    m_autoHide->setChecked(o.autoHide);
    m_smallIcons->setChecked(o.smallIcons);
    m_showClock->setChecked(o.showClock);
    m_locked->setChecked(o.locked);
    m_preview->setOptions(o);
    m_apply->setEnabled(false);
}

TaskbarOptions TaskbarPropertiesDialog::options() const
{
    TaskbarOptions o;
    o.alwaysOnTop = m_onTop->isChecked();
    o.autoHide = m_autoHide->isChecked();
    o.smallIcons = m_smallIcons->isChecked();
    o.showClock = m_showClock->isChecked();
    o.locked = m_locked->isChecked();
    return o;
}

void TaskbarPropertiesDialog::optionsChanged()
{
    const TaskbarOptions o = options();
    m_preview->setOptions(o);
    m_apply->setEnabled(!m_settings || o != TaskbarOptions::fromSettings(*m_settings));
}

void TaskbarPropertiesDialog::apply()
{
    if (m_settings)
        options().save(m_settings);
    m_apply->setEnabled(false);
}

void TaskbarPropertiesDialog::accept()
{
    apply();
    QDialog::accept();
}

} // namespace Daytona
