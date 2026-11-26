// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayproperties.h"

#include "bevel.h"
#include "colourscheme.h"
#include "daytonastyle.h"
#include "desktop.h"
#include "monitorlayout.h"
#include "outputmanager.h"
#include "systemlogo.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QIntValidator>
#include <QLabel>
#include <QListWidget>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <memory>

Q_LOGGING_CATEGORY(lcDisplay, "daytona.display", QtInfoMsg)

namespace Daytona {

namespace {

    const QStringList ImageFilters
        = { QStringLiteral("*.bmp"), QStringLiteral("*.png"), QStringLiteral("*.jpg"),
              QStringLiteral("*.jpeg"), QStringLiteral("*.gif"), QStringLiteral("*.svg") };

    QColor colourOf(const SchemeColours &colours, const char *key)
    {
        const QString name = QString::fromLatin1(key);
        const QColor colour = colours.value(name);
        return colour.isValid() ? colour : standardColours().value(name);
    }

    QImage loadImage(const QString &path, const QSize &maximum)
    {
        if (path.isEmpty())
            return {};
        QImageReader reader(path);
        // Only a preview is needed: read large images at a reduced size.
        const QSize size = reader.size();
        if (size.isValid() && (size.width() > maximum.width() || size.height() > maximum.height()))
            reader.setScaledSize(size.scaled(maximum, Qt::KeepAspectRatio));
        return reader.read();
    }

    QString findThemeTool()
    {
        const QString here = QCoreApplication::applicationDirPath();
        // Installed next to the shell, or in the build tree's themetool/.
        const QString found = QStandardPaths::findExecutable(QStringLiteral("daytona-themetool"),
            { here, QDir(here).filePath(QStringLiteral("../themetool")) });
        if (!found.isEmpty())
            return found;
        return QStringLiteral("daytona-themetool");
    }

    // A small NT window frame: raised border, title bar and caption text.
    QRect drawWindow(QPainter *painter, const QRect &rect, const QString &title, bool active,
        const SchemeColours &c, const QPalette &pal)
    {
        painter->fillRect(rect, colourOf(c, active ? "ActiveBorder" : "InactiveBorder"));
        Bevel::raised(painter, rect, pal);
        const QRect titleBar(rect.left() + 4, rect.top() + 4, rect.width() - 8, 16);
        painter->fillRect(titleBar, colourOf(c, active ? "ActiveTitle" : "InactiveTitle"));
        QFont bold = painter->font();
        bold.setBold(true);
        painter->save();
        painter->setFont(bold);
        painter->setPen(colourOf(c, active ? "TitleText" : "InactiveTitleText"));
        painter->drawText(titleBar.adjusted(3, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, title);
        painter->restore();
        // The caption buttons.
        for (int i = 0; i < 3; ++i) {
            const QRect button(titleBar.right() - 2 - (3 - i) * 15, titleBar.top() + 2, 13, 12);
            painter->fillRect(button, colourOf(c, "ButtonFace"));
            Bevel::raised(painter, button, pal);
        }
        return { rect.left() + 4, titleBar.bottom() + 2, rect.width() - 8,
            rect.bottom() - titleBar.bottom() - 5 };
    }

} // namespace

QStringList wallpaperCandidates(const QStringList &directories, int limit)
{
    QStringList files;
    for (const QString &directory : directories) {
        const QDir dir(directory);
        const QFileInfoList entries = dir.entryInfoList(
            ImageFilters, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &info : entries) {
            const QString path = info.absoluteFilePath();
            if (!files.contains(path))
                files << path;
            if (files.size() >= limit)
                return files;
        }
    }
    return files;
}

QStringList wallpaperDirectories()
{
    QStringList dirs { QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
            .filePath(QStringLiteral("daytona/wallpaper")) };
    for (const QString &data :
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        dirs << QDir(data).filePath(QStringLiteral("backgrounds"))
             << QDir(data).filePath(QStringLiteral("wallpapers"));
    }
    dirs.removeDuplicates();
    return dirs;
}

DesktopPreview::DesktopPreview(QWidget *parent)
    : QWidget(parent)
{
}

void DesktopPreview::setDesktop(
    const QColor &colour, const QImage &wallpaper, WallpaperMode mode, const QSize &screen)
{
    m_colour = colour;
    m_wallpaper = wallpaper;
    m_mode = mode;
    if (screen.isValid())
        m_screen = screen;
    update();
}

QSize DesktopPreview::sizeHint() const
{
    return { 184, 170 };
}

void DesktopPreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QPalette &pal = palette();
    // NT's monitor: a raised case on a stand, with the screen sunken in it.
    const QRect monitor(12, 4, 160, 128);
    painter.fillRect(monitor, pal.color(QPalette::Button));
    Bevel::raised(&painter, monitor, pal);
    const QRect stand(monitor.center().x() - 30, monitor.bottom() + 1, 60, 10);
    painter.fillRect(stand, pal.color(QPalette::Button));
    Bevel::raised(&painter, stand, pal);
    const QRect base(monitor.center().x() - 50, stand.bottom() + 1, 100, 12);
    painter.fillRect(base, pal.color(QPalette::Button));
    Bevel::raised(&painter, base, pal);
    const QRect bezel = monitor.adjusted(10, 10, -10, -22);
    Bevel::sunken(&painter, bezel, pal);
    const QRect screen = bezel.adjusted(2, 2, -2, -2);
    painter.fillRect(screen, m_colour);
    // The power light.
    painter.fillRect(monitor.right() - 18, monitor.bottom() - 12, 4, 3, QColor(0, 200, 0));

    if (m_wallpaper.isNull() || m_screen.isEmpty())
        return;
    // The wallpaper at the same scale as the screen.
    const qreal scale = qreal(screen.width()) / m_screen.width();
    painter.setClipRect(screen);
    if (m_mode == WallpaperMode::Tile) {
        const QSize scaled = (QSizeF(m_wallpaper.size()) * scale).toSize().expandedTo(QSize(1, 1));
        const QImage small
            = m_wallpaper.scaled(scaled, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        painter.drawTiledPixmap(screen, QPixmap::fromImage(small));
        return;
    }
    // The little screen is not quite the real one's shape, so map each axis
    // on its own: what the desktop shows edge to edge, the preview does too.
    const QRect placed = wallpaperRect(m_screen, m_wallpaper.size(), m_mode);
    const qreal scaleY = qreal(screen.height()) / m_screen.height();
    const QRect target(screen.left() + qRound(placed.x() * scale),
        screen.top() + qRound(placed.y() * scaleY), qMax(1, qRound(placed.width() * scale)),
        qMax(1, qRound(placed.height() * scaleY)));
    painter.drawImage(
        target, m_wallpaper.scaled(target.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation));
}

AppearancePreview::AppearancePreview(QWidget *parent)
    : QWidget(parent)
    , m_colours(standardColours())
{
}

void AppearancePreview::setColours(const SchemeColours &colours)
{
    m_colours = colours;
    update();
}

QSize AppearancePreview::sizeHint() const
{
    return { 330, 170 };
}

void AppearancePreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // Everything is drawn in the candidate scheme, not the current one.
    const QPalette pal = paletteFromColours(m_colours);
    const auto c = [this](const char *key) { return colourOf(m_colours, key); };
    Bevel::sunken(&painter, rect(), palette());
    const QRect inside = rect().adjusted(2, 2, -2, -2);
    painter.fillRect(inside, c("Background"));
    painter.setClipRect(inside);

    // Inactive window behind.
    const QRect inactiveArea
        = drawWindow(&painter, QRect(10, 8, 230, 60), tr("Inactive Window"), false, m_colours, pal);
    painter.fillRect(inactiveArea, c("AppWorkspace"));

    // Active window with a menu bar and window text.
    const QRect active(20, 30, 250, 128);
    const QRect area = drawWindow(&painter, active, tr("Active Window"), true, m_colours, pal);
    const QRect menu(area.left(), area.top(), area.width(), 18);
    painter.fillRect(menu, c("Menu"));
    const QFontMetrics fm(font());
    int x = menu.left() + 6;
    const auto item = [&](const QString &text, const QColor &fg, const QColor &bg) {
        const int w = fm.horizontalAdvance(text) + 12;
        const QRect r(x, menu.top() + 1, w, menu.height() - 2);
        if (bg.isValid())
            painter.fillRect(r, bg);
        painter.setPen(fg);
        painter.drawText(r, Qt::AlignCenter, text);
        x += w;
    };
    item(tr("Normal"), c("MenuText"), QColor());
    item(tr("Disabled"), c("GrayText"), QColor());
    item(tr("Selected"), c("HilightText"), c("Hilight"));
    const QRect window(
        area.left(), menu.bottom() + 2, area.width(), area.bottom() - menu.bottom() - 1);
    painter.fillRect(window, c("Window"));
    Bevel::sunken(&painter, window, pal);
    painter.setPen(c("WindowText"));
    painter.drawText(
        window.adjusted(6, 4, -20, 0), Qt::AlignLeft | Qt::AlignTop, tr("Window Text"));
    // A scroll bar down the right.
    const QRect scroll(window.right() - 17, window.top() + 2, 16, window.height() - 4);
    painter.fillRect(scroll, c("Scrollbar"));
    for (const QRect &arrow : { QRect(scroll.left(), scroll.top(), 16, 16),
             QRect(scroll.left(), scroll.bottom() - 15, 16, 16) }) {
        painter.fillRect(arrow, c("ButtonFace"));
        Bevel::raised(&painter, arrow, pal);
    }

    // A message box in front.
    const QRect box(130, 96, 160, 64);
    const QRect boxArea = drawWindow(&painter, box, tr("Message Box"), true, m_colours, pal);
    painter.fillRect(boxArea, c("ButtonFace"));
    painter.setPen(c("ButtonText"));
    painter.drawText(
        boxArea.adjusted(6, 2, 0, 0), Qt::AlignLeft | Qt::AlignTop, tr("Message Text"));
    const QRect ok(boxArea.center().x() - 25, boxArea.bottom() - 18, 50, 16);
    painter.fillRect(ok, c("ButtonFace"));
    Bevel::raised(&painter, ok, pal);
    painter.drawText(ok, Qt::AlignCenter, tr("OK"));
}

DisplayPropertiesDialog::DisplayPropertiesDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_themeTool(findThemeTool())
{
    setWindowTitle(tr("Display Properties"));
    auto *tabs = new QTabWidget(this);
    tabs->addTab(createBackgroundTab(), tr("Background"));
    tabs->addTab(createScreenSaverTab(), tr("Screen Saver"));
    tabs->addTab(createAppearanceTab(), tr("Appearance"));
    tabs->addTab(createSettingsTab(), tr("Settings"));

    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    m_apply = new QPushButton(tr("&Apply"), this);
    for (QPushButton *b : { ok, cancel, m_apply })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &DisplayPropertiesDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &DisplayPropertiesDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &DisplayPropertiesDialog::apply);
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

QWidget *DisplayPropertiesDialog::createBackgroundTab()
{
    auto *page = new QWidget;
    m_desktopPreview = new DesktopPreview(page);
    auto *group = new QGroupBox(tr("Wallpaper"), page);
    m_wallpapers = new QListWidget(group);
    m_wallpapers->setMinimumHeight(110);
    auto *browse = new QPushButton(tr("&Browse..."), group);
    browse->setMinimumSize(75, 23);
    connect(browse, &QPushButton::clicked, this, &DisplayPropertiesDialog::browse);
    auto *displayLabel = new QLabel(tr("Display:"), group);
    // NT's Tile and Center first, then the later Windows modes (AGENT.md,
    // Design Scope).
    m_displayModes = {
        { WallpaperMode::Tile, new QRadioButton(tr("&Tile"), group) },
        { WallpaperMode::Centre, new QRadioButton(tr("&Center"), group) },
        { WallpaperMode::Stretch, new QRadioButton(tr("&Stretch"), group) },
        { WallpaperMode::Fit, new QRadioButton(tr("F&it"), group) },
        { WallpaperMode::Fill, new QRadioButton(tr("Fi&ll"), group) },
    };
    connect(m_wallpapers, &QListWidget::currentRowChanged, this, [this] {
        updateBackgroundPreview();
        changed();
    });
    for (const auto &[mode, radio] : std::as_const(m_displayModes)) {
        connect(radio, &QRadioButton::toggled, this, [this] {
            updateBackgroundPreview();
            changed();
        });
    }

    auto *groupLayout = new QGridLayout(group);
    const int rows = 3 + int(m_displayModes.size());
    groupLayout->addWidget(m_wallpapers, 0, 0, rows, 1);
    groupLayout->addWidget(browse, 0, 1);
    groupLayout->addWidget(displayLabel, 1, 1);
    for (int i = 0; i < m_displayModes.size(); ++i)
        groupLayout->addWidget(m_displayModes[i].second, 2 + i, 1);
    groupLayout->setRowStretch(rows - 1, 1); // as NT: the options under Browse...

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(m_desktopPreview, 0, Qt::AlignHCenter);
    layout->addWidget(group);
    return page;
}

QWidget *DisplayPropertiesDialog::createScreenSaverTab()
{
    // NT's tab, with its one built-in screen saver that fits Daytona:
    // Blank Screen. It has no settings of its own, so Settings... stays
    // disabled, as NT's did for screen savers without any.
    auto *page = new QWidget;
    m_saverPreview = new DesktopPreview(page);
    auto *group = new QGroupBox(tr("&Screen Saver"), page);
    m_screenSaver = new QComboBox(group);
    m_screenSaver->addItem(tr("(None)"), int(ScreenSaver::None));
    m_screenSaver->addItem(tr("Blank Screen"), int(ScreenSaver::Blank));
    m_saverSettings = new QPushButton(tr("Se&ttings..."), group);
    m_saverSettings->setEnabled(false);
    m_previewSaver = new QPushButton(tr("Pre&view"), group);
    for (QPushButton *b : { m_saverSettings, m_previewSaver })
        b->setMinimumSize(75, 23);
    m_password = new QCheckBox(tr("&Password protected"), group);
    auto *waitLabel = new QLabel(tr("&Wait:"), group);
    m_wait = new QSpinBox(group);
    m_wait->setRange(Settings::MinScreenSaverWait, Settings::MaxScreenSaverWait);
    waitLabel->setBuddy(m_wait);
    auto *minutes = new QLabel(tr("minutes"), group);

    connect(m_screenSaver, &QComboBox::currentIndexChanged, this, [this] {
        updateScreenSaverControls();
        changed();
    });
    connect(m_password, &QCheckBox::toggled, this, &DisplayPropertiesDialog::changed);
    connect(m_wait, &QSpinBox::valueChanged, this, &DisplayPropertiesDialog::changed);
    connect(m_previewSaver, &QPushButton::clicked, this,
        &DisplayPropertiesDialog::screenSaverPreviewRequested);

    auto *top = new QHBoxLayout;
    top->addWidget(m_screenSaver, 1);
    top->addWidget(m_saverSettings);
    top->addWidget(m_previewSaver);
    auto *bottom = new QHBoxLayout;
    bottom->addWidget(m_password);
    bottom->addStretch();
    bottom->addWidget(waitLabel);
    bottom->addWidget(m_wait);
    bottom->addWidget(minutes);
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->addLayout(top);
    groupLayout->addLayout(bottom);

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(m_saverPreview, 0, Qt::AlignHCenter);
    layout->addWidget(group);
    layout->addStretch();
    return page;
}

void DisplayPropertiesDialog::updateScreenSaverControls()
{
    const bool blank = m_screenSaver->currentData().toInt() == int(ScreenSaver::Blank);
    m_previewSaver->setEnabled(blank);
    m_password->setEnabled(blank);
    m_wait->setEnabled(blank);
    // The little monitor shows the screen saver, or the desktop without one.
    const QScreen *screen = QApplication::primaryScreen();
    m_saverPreview->setDesktop(
        blank ? QColor(Qt::black) : Style::schemeColour(QStringLiteral("Background")), QImage(),
        WallpaperMode::Centre, screen ? screen->size() : QSize(1024, 768));
}

QWidget *DisplayPropertiesDialog::createAppearanceTab()
{
    auto *page = new QWidget;
    m_appearancePreview = new AppearancePreview(page);
    auto *label = new QLabel(tr("&Scheme:"), page);
    m_schemes = new QComboBox(page);
    label->setBuddy(m_schemes);
    auto *load = new QPushButton(tr("&Load .theme file..."), page);
    connect(load, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Load .theme file"),
            QDir::homePath(), tr("Theme files (*.theme *.Theme *.THEME)"));
        if (!file.isEmpty())
            loadThemeFile(file);
    });
    connect(m_schemes, &QComboBox::currentIndexChanged, this, [this] {
        updateAppearancePreview();
        changed();
    });

    // Not in NT: the Start button shows the operating system's logo, and
    // this turns it off for Daytona's own (AGENT.md, approved exceptions).
    m_systemLogo = new QCheckBox(tr("Use the operating system's &logo on the Start button"), page);
    connect(m_systemLogo, &QCheckBox::toggled, this, &DisplayPropertiesDialog::changed);

    auto *row = new QHBoxLayout;
    row->addWidget(m_schemes, 1);
    row->addWidget(load);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(m_appearancePreview, 0, Qt::AlignHCenter);
    layout->addWidget(label);
    layout->addLayout(row);
    layout->addWidget(m_systemLogo);
    layout->addStretch();
    return page;
}

void DisplayPropertiesDialog::prepare()
{
    m_preparing = true;
    // Background.
    m_wallpapers->clear();
    m_wallpapers->addItem(tr("(None)"));
    for (const QString &path : wallpaperCandidates(wallpaperDirectories())) {
        auto *item = new QListWidgetItem(QFileInfo(path).completeBaseName(), m_wallpapers);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
    selectWallpaper(m_settings ? m_settings->wallpaperPath() : QString());
    const WallpaperMode mode = m_settings ? m_settings->wallpaperMode() : WallpaperMode::Centre;
    for (const auto &[candidate, radio] : std::as_const(m_displayModes))
        radio->setChecked(candidate == mode);

    // Screen Saver.
    const ScreenSaver saver = m_settings ? m_settings->screenSaver() : ScreenSaver::Blank;
    m_screenSaver->setCurrentIndex(qMax(0, m_screenSaver->findData(int(saver))));
    m_password->setChecked(m_settings && m_settings->screenSaverPassword());
    m_wait->setValue(m_settings ? m_settings->screenSaverWait() : 15);
    updateScreenSaverControls();

    // Appearance: built-in schemes, then loaded .theme files.
    m_schemes->clear();
    for (const QString &name : builtInSchemeNames())
        m_schemes->addItem(name);
    if (m_settings) {
        for (const QString &file : m_settings->themeFiles()) {
            if (QFileInfo::exists(file))
                m_schemes->addItem(QFileInfo(file).completeBaseName(), file);
        }
    }
    const QString current = m_settings ? m_settings->colourScheme() : QString();
    const int index = m_schemes->findText(current);
    m_schemes->setCurrentIndex(index >= 0 ? index : 0);
    m_appliedScheme = m_schemes->currentText();
    m_systemLogo->setChecked(m_settings ? m_settings->systemStartLogo() : true);
    // Nothing to choose between when the system has no logo.
    m_systemLogo->setEnabled(!systemLogo().isNull());

    // Settings: every screen, the dialog's own first shown.
    prepareMonitors();

    m_preparing = false;
    updateBackgroundPreview();
    updateAppearancePreview();
    m_apply->setEnabled(false);
}

void DisplayPropertiesDialog::selectWallpaper(const QString &path)
{
    if (!path.isEmpty()) {
        for (int i = 1; i < m_wallpapers->count(); ++i) {
            if (m_wallpapers->item(i)->data(Qt::UserRole).toString() == path) {
                m_wallpapers->setCurrentRow(i);
                return;
            }
        }
        // A wallpaper chosen elsewhere: list it too.
        auto *item = new QListWidgetItem(QFileInfo(path).completeBaseName(), m_wallpapers);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_wallpapers->setCurrentItem(item);
        return;
    }
    m_wallpapers->setCurrentRow(0);
}

void DisplayPropertiesDialog::browse()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Browse"), QDir::homePath(),
        tr("Images (%1)").arg(ImageFilters.join(QLatin1Char(' '))));
    if (!file.isEmpty())
        selectWallpaper(file);
}

QString DisplayPropertiesDialog::selectedWallpaper() const
{
    const QListWidgetItem *item = m_wallpapers->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

WallpaperMode DisplayPropertiesDialog::selectedMode() const
{
    for (const auto &[mode, radio] : m_displayModes) {
        if (radio->isChecked())
            return mode;
    }
    return WallpaperMode::Centre;
}

SchemeColours DisplayPropertiesDialog::candidateColours() const
{
    const QString file = m_schemes->currentData().toString();
    if (!file.isEmpty()) {
        if (const std::optional<ColourScheme> scheme = readThemeFile(file))
            return scheme->colours;
        return standardColours();
    }
    if (const std::optional<ColourScheme> scheme = builtInScheme(m_schemes->currentText()))
        return scheme->colours;
    return standardColours();
}

void DisplayPropertiesDialog::updateBackgroundPreview()
{
    if (m_preparing)
        return;
    const QScreen *screen = QApplication::primaryScreen();
    const QSize size = screen ? screen->size() : QSize(1024, 768);
    m_desktopPreview->setDesktop(Style::schemeColour(QStringLiteral("Background")),
        loadImage(selectedWallpaper(), size), selectedMode(), size);
}

void DisplayPropertiesDialog::updateAppearancePreview()
{
    if (m_preparing)
        return;
    m_appearancePreview->setColours(candidateColours());
}

void DisplayPropertiesDialog::changed()
{
    if (!m_preparing)
        m_apply->setEnabled(true);
}

void DisplayPropertiesDialog::loadThemeFile(const QString &path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    int index = m_schemes->findData(absolute);
    if (index < 0) {
        m_schemes->addItem(QFileInfo(absolute).completeBaseName(), absolute);
        index = m_schemes->count() - 1;
    }
    m_schemes->setCurrentIndex(index);
}

void DisplayPropertiesDialog::apply()
{
    if (m_settings) {
        // Wallpaper: written directly; the desktop follows the settings.
        m_settings->setWallpaperMode(selectedMode());
        m_settings->setWallpaperPath(selectedWallpaper());
        m_settings->setSystemStartLogo(m_systemLogo->isChecked());
        // The screen saver: the shell's idle monitor follows the settings.
        m_settings->setScreenSaverWait(m_wait->value());
        m_settings->setScreenSaverPassword(m_password->isChecked());
        m_settings->setScreenSaver(ScreenSaver(m_screenSaver->currentData().toInt()));
    }

    // Colour scheme: the theme tool writes colors.ini and the labwc and GTK
    // themes. Run without a shell and without waiting.
    if (m_schemes->currentText() != m_appliedScheme) {
        const QString file = m_schemes->currentData().toString();
        const QStringList arguments = file.isEmpty()
            ? QStringList { QStringLiteral("apply"), QStringLiteral("--scheme"),
                  m_schemes->currentText() }
            : QStringList { QStringLiteral("apply"), file };
        auto *process = new QProcess(this);
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        connect(process, &QProcess::finished, this,
            [this, process, file](int code, QProcess::ExitStatus status) {
                process->deleteLater();
                const bool ok = status == QProcess::NormalExit && code == 0;
                if (ok && !file.isEmpty() && m_settings)
                    m_settings->addThemeFile(file);
                if (!ok)
                    qCWarning(lcDisplay) << "daytona-themetool failed with exit code" << code;
                Q_EMIT schemeApplied(ok);
            });
        connect(
            process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart)
                    return;
                qCWarning(lcDisplay) << "Cannot run" << m_themeTool;
                process->deleteLater();
                Q_EMIT schemeApplied(false);
            });
        process->start(m_themeTool, arguments);
        m_appliedScheme = m_schemes->currentText();
    }
    applyScreen();
    m_apply->setEnabled(false);
}

namespace {

    bool sameScale(double a, double b)
    {
        return std::abs(a - b) < 0.001;
    }

    constexpr int MinPercent = 100;
    constexpr int MaxPercent = 300;

} // namespace

QWidget *DisplayPropertiesDialog::createSettingsTab()
{
    // Laid out as NT's Settings tab: Desktop Area and Refresh Frequency on
    // one side, Font Size on the other. With more than one screen, Windows
    // 2000's monitor diagram and Display list go above, and its primary
    // monitor and Extend check boxes below.
    auto *page = new QWidget;

    m_arrangementBox = new QWidget(page);
    auto *dragHint = new QLabel(
        tr("Drag the monitor icons to match the physical arrangement of your monitors."),
        m_arrangementBox);
    dragHint->setWordWrap(true);
    m_arrangement = new MonitorArrangement(m_arrangementBox);
    connect(m_arrangement, &MonitorArrangement::selectedChanged, this,
        &DisplayPropertiesDialog::selectMonitor);
    connect(m_arrangement, &MonitorArrangement::monitorMoved, this,
        &DisplayPropertiesDialog::monitorMoved);
    m_display = new QComboBox(m_arrangementBox);
    connect(
        m_display, &QComboBox::currentIndexChanged, this, &DisplayPropertiesDialog::selectMonitor);
    auto *displayLabel = new QLabel(tr("&Display:"), m_arrangementBox);
    displayLabel->setBuddy(m_display);
    auto *displayRow = new QHBoxLayout;
    displayRow->addWidget(displayLabel);
    displayRow->addWidget(m_display, 1);
    auto *arrangementLayout = new QVBoxLayout(m_arrangementBox);
    arrangementLayout->setContentsMargins(0, 0, 0, 0);
    arrangementLayout->addWidget(dragHint);
    arrangementLayout->addWidget(m_arrangement, 1);
    arrangementLayout->addLayout(displayRow);

    m_primaryBox = new QCheckBox(tr("&Use this device as the primary monitor."), page);
    connect(m_primaryBox, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && !m_preparing)
            setPrimary();
    });
    m_extend = new QCheckBox(tr("&Extend my desktop onto this monitor."), page);
    connect(m_extend, &QCheckBox::toggled, this, [this](bool checked) {
        if (!m_preparing)
            setExtended(checked);
    });

    auto *area = new QGroupBox(tr("Desktop Area"), page);
    m_desktopArea = new QSlider(Qt::Horizontal, area);
    m_desktopArea->setPageStep(1);
    m_desktopArea->setTickPosition(QSlider::TicksBelow);
    m_desktopArea->setTickInterval(1);
    m_areaLabel = new QLabel(area);
    m_areaLabel->setAlignment(Qt::AlignCenter);
    connect(m_desktopArea, &QSlider::valueChanged, this, [this] {
        updateRefreshList(selectedScreenMode().refresh);
        screenControlChanged();
    });
    auto *sliderRow = new QHBoxLayout;
    sliderRow->addWidget(new QLabel(tr("Less"), area));
    sliderRow->addWidget(m_desktopArea, 1);
    sliderRow->addWidget(new QLabel(tr("More"), area));
    auto *areaLayout = new QVBoxLayout(area);
    areaLayout->addLayout(sliderRow);
    areaLayout->addWidget(m_areaLabel);

    auto *refresh = new QGroupBox(tr("Refresh Frequency"), page);
    m_refresh = new QComboBox(refresh);
    connect(m_refresh, &QComboBox::currentIndexChanged, this,
        &DisplayPropertiesDialog::screenControlChanged);
    auto *refreshLayout = new QVBoxLayout(refresh);
    refreshLayout->addWidget(m_refresh);

    auto *font = new QGroupBox(tr("Font Size"), page);
    m_fontSize = new QComboBox(font);
    m_fontSize->addItem(tr("Small Fonts"), 1.0);
    // NT's Large Fonts was 125% (120 DPI); Daytona's is 150%, the more useful
    // size on today's high-density screens (AGENT.md, approved exceptions).
    m_fontSize->addItem(tr("Large Fonts"), 1.5);
    m_customFontSize = new QPushButton(tr("&Custom..."), font);
    m_customFontSize->setMinimumSize(75, 23);
    connect(m_fontSize, &QComboBox::currentIndexChanged, this,
        &DisplayPropertiesDialog::screenControlChanged);
    connect(m_customFontSize, &QPushButton::clicked, this, [this] {
        CustomFontSizeDialog custom(this);
        custom.setPercent(qRound(selectedScale() * 100));
        if (custom.exec() == QDialog::Accepted)
            selectScale(custom.percent() / 100.0);
    });
    auto *fontLayout = new QVBoxLayout(font);
    fontLayout->addWidget(m_fontSize);
    fontLayout->addWidget(m_customFontSize, 0, Qt::AlignRight);

    m_screenLabel = new QLabel(page);
    m_screenLabel->setWordWrap(true);

    auto *grid = new QGridLayout(page);
    grid->addWidget(m_arrangementBox, 0, 0, 1, 2);
    grid->addWidget(area, 1, 0);
    grid->addWidget(font, 1, 1);
    grid->addWidget(refresh, 2, 0);
    grid->addWidget(m_primaryBox, 3, 0, 1, 2);
    grid->addWidget(m_extend, 4, 0, 1, 2);
    grid->addWidget(m_screenLabel, 5, 0, 1, 2);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(6, 1);
    return page;
}

ScreenMode DisplayPropertiesDialog::selectedScreenMode() const
{
    if (m_sizes.isEmpty())
        return {};
    ScreenMode wanted;
    wanted.size = m_sizes.value(m_desktopArea->value());
    wanted.refresh = m_refresh->currentData().toInt();
    return matchMode(m_modes, wanted);
}

void DisplayPropertiesDialog::selectScreenMode(const ScreenMode &mode)
{
    const bool preparing = m_preparing;
    m_preparing = true;
    const int index = m_sizes.indexOf(mode.size);
    m_desktopArea->setValue(index >= 0 ? index : 0);
    updateRefreshList(mode.refresh);
    m_preparing = preparing;
}

void DisplayPropertiesDialog::updateRefreshList(int keepRefresh)
{
    const QSize size = m_sizes.value(m_desktopArea->value());
    m_areaLabel->setText(
        size.isValid() ? tr("%1 by %2 pixels").arg(size.width()).arg(size.height()) : QString());
    QList<int> rates;
    for (const ScreenMode &mode : std::as_const(m_modes)) {
        if (mode.size == size && !rates.contains(mode.refresh))
            rates.append(mode.refresh);
    }
    std::sort(rates.begin(), rates.end());
    const bool preparing = m_preparing;
    m_preparing = true;
    m_refresh->clear();
    for (int rate : std::as_const(rates)) {
        // Whole hertz as NT showed them, unless two rates would look alike.
        const int whole = qRound(rate / 1000.0);
        bool clash = false;
        for (int other : std::as_const(rates))
            clash |= other != rate && qRound(other / 1000.0) == whole;
        const QString text = rate <= 0 ? tr("Default")
            : clash                    ? tr("%1 Hertz").arg(QString::number(rate / 1000.0, 'f', 2))
                                       : tr("%1 Hertz").arg(whole);
        m_refresh->addItem(text, rate);
    }
    // The same rate if this size has it, else the size's best one.
    ScreenMode wanted;
    wanted.size = size;
    wanted.refresh = keepRefresh;
    const ScreenMode match = matchMode(m_modes, wanted);
    const int index = m_refresh->findData(match.refresh);
    m_refresh->setCurrentIndex(index >= 0 ? index : 0);
    m_preparing = preparing;
}

QString DisplayPropertiesDialog::screenName() const
{
    if (!m_screenName.isEmpty())
        return m_screenName;
    if (QScreen *s = screen())
        return s->name();
    return QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->name() : QString();
}

double DisplayPropertiesDialog::selectedScale() const
{
    return m_fontSize->currentData().toDouble();
}

void DisplayPropertiesDialog::selectScale(double scale)
{
    for (int i = 0; i < m_fontSize->count(); ++i) {
        if (sameScale(m_fontSize->itemData(i).toDouble(), scale)) {
            m_fontSize->setCurrentIndex(i);
            return;
        }
    }
    // A custom size: one entry after Small and Large Fonts.
    const QString text = tr("Other (%1%)").arg(qRound(scale * 100));
    if (m_fontSize->count() > 2) {
        m_fontSize->setItemText(2, text);
        m_fontSize->setItemData(2, scale);
    } else {
        m_fontSize->addItem(text, scale);
    }
    m_fontSize->setCurrentIndex(2);
}

void DisplayPropertiesDialog::prepareMonitors()
{
    const QString own = screenName();
    QStringList names = m_screens ? m_screens->screens() : QStringList();
    if (names.isEmpty())
        names = { own };
    m_monitors.clear();
    for (const QString &name : std::as_const(names)) {
        Monitor monitor;
        monitor.name = name;
        monitor.id = m_screens ? m_screens->identity(name) : name;
        const double reported = m_screens ? m_screens->scale(name) : 0.0;
        monitor.canChange = reported > 0;
        monitor.scale = reported > 0  ? reported
            : name == own && screen() ? screen()->devicePixelRatio()
                                      : 1.0;
        monitor.modes = m_screens ? m_screens->modes(name) : QList<ScreenMode>();
        ScreenMode current = m_screens ? m_screens->currentMode(name) : ScreenMode();
        if (!current.isValid()) {
            for (const ScreenMode &mode : std::as_const(monitor.modes)) {
                if (mode.preferred)
                    current = mode;
            }
        }
        monitor.mode = current;
        monitor.enabled = names.size() == 1 || (m_screens && m_screens->isEnabled(name));
        monitor.position = m_screens ? m_screens->position(name) : QPoint();
        // A screen that is off has no mode yet: the one shown is asked for
        // when it is switched on.
        monitor.appliedMode = monitor.enabled ? monitor.mode : ScreenMode();
        monitor.appliedScale = monitor.scale;
        monitor.appliedEnabled = monitor.enabled;
        monitor.appliedPosition = monitor.position;
        m_monitors.append(monitor);
    }

    // The primary monitor: the one saved, else the one the taskbar is on.
    const auto findId = [this](const QString &id) {
        for (const Monitor &monitor : std::as_const(m_monitors)) {
            if (monitor.enabled && monitor.id == id)
                return true;
        }
        return false;
    };
    m_primary = m_settings ? m_settings->primaryOutput() : QString();
    if (!findId(m_primary)) {
        m_primary.clear();
        const QScreen *primary = QGuiApplication::primaryScreen();
        for (const Monitor &monitor : std::as_const(m_monitors)) {
            if (monitor.enabled && m_primary.isEmpty())
                m_primary = monitor.id;
            if (monitor.enabled && primary && monitor.name == primary->name()) {
                m_primary = monitor.id;
                break;
            }
        }
    }
    m_appliedPrimary = m_primary;

    const bool several = m_monitors.size() > 1;
    m_arrangementBox->setVisible(several);
    m_primaryBox->setVisible(several);
    m_extend->setVisible(several);
    m_display->clear();
    for (int i = 0; i < m_monitors.size(); ++i) {
        const Monitor &monitor = m_monitors.at(i);
        m_display->addItem(tr("%1. %2").arg(i + 1).arg(
            m_screens ? m_screens->description(monitor.name) : monitor.name));
    }
    m_arrangement->setDesktopColour(Style::schemeColour(QStringLiteral("Background")));
    m_monitor = std::max(0, int(names.indexOf(own)));
    showMonitor();
    updateArrangement();
}

void DisplayPropertiesDialog::selectMonitor(int index)
{
    if (index < 0 || index >= m_monitors.size() || index == m_monitor)
        return;
    m_monitor = index;
    showMonitor();
}

void DisplayPropertiesDialog::showMonitor()
{
    const bool preparing = m_preparing;
    m_preparing = true;
    const Monitor &monitor = m_monitors.at(m_monitor);
    m_display->setCurrentIndex(m_monitor);
    m_arrangement->setSelected(m_monitor);

    while (m_fontSize->count() > 2)
        m_fontSize->removeItem(2);
    selectScale(monitor.scale);
    m_modes = monitor.modes;
    m_sizes.clear();
    for (const ScreenMode &mode : std::as_const(m_modes)) {
        if (mode.isValid() && !m_sizes.contains(mode.size))
            m_sizes.append(mode.size);
    }
    // Less to more: by area, then width.
    std::sort(m_sizes.begin(), m_sizes.end(), [](const QSize &a, const QSize &b) {
        const qint64 areaA = qint64(a.width()) * a.height();
        const qint64 areaB = qint64(b.width()) * b.height();
        return areaA != areaB ? areaA < areaB : a.width() < b.width();
    });
    // A screen that is off keeps its settings until it is extended onto.
    const bool canChange = monitor.canChange && monitor.enabled;
    m_fontSize->setEnabled(canChange);
    m_customFontSize->setEnabled(canChange);
    const bool canMode = canChange && !m_sizes.isEmpty();
    m_desktopArea->setEnabled(canMode);
    m_refresh->setEnabled(canMode);
    m_desktopArea->setRange(0, qMax(0, int(m_sizes.size()) - 1));
    selectScreenMode(monitor.mode);
    if (!canMode)
        m_areaLabel->setText(tr("Not available"));

    // NT's taskbar stays on one monitor, which cannot be switched off.
    const bool primary = monitor.id == m_primary;
    m_primaryBox->setChecked(primary);
    m_primaryBox->setEnabled(monitor.canChange && monitor.enabled && !primary);
    m_extend->setChecked(monitor.enabled);
    m_extend->setEnabled(monitor.canChange && !primary);

    const QString applies = m_monitors.size() > 1
        ? tr("These settings apply to monitor %1 (%2) and are remembered for it.")
              .arg(m_monitor + 1)
              .arg(monitor.name)
        : tr("These settings apply to this screen (%1) and are remembered for it.")
              .arg(monitor.name);
    m_screenLabel->setText(monitor.canChange
            ? applies
            : tr("The display settings cannot be changed in this session."));
    m_preparing = preparing;
}

void DisplayPropertiesDialog::screenControlChanged()
{
    if (m_preparing || m_monitor >= m_monitors.size())
        return;
    Monitor &monitor = m_monitors[m_monitor];
    const QSize before = monitorSize(monitor);
    monitor.scale = selectedScale();
    const ScreenMode mode = selectedScreenMode();
    if (mode.isValid())
        monitor.mode = mode;
    const QSize after = monitorSize(monitor);
    if (monitor.enabled && before.isValid() && after.isValid() && before != after) {
        // The screens beyond its right and bottom edges move with them.
        QList<int> indexes;
        QList<QRect> rects;
        for (int i = 0; i < m_monitors.size(); ++i) {
            if (!m_monitors.at(i).enabled)
                continue;
            indexes.append(i);
            rects.append(QRect(m_monitors.at(i).position,
                i == m_monitor ? before : monitorSize(m_monitors.at(i))));
        }
        rects = resizeMonitor(rects, int(indexes.indexOf(m_monitor)), after);
        for (int i = 0; i < indexes.size(); ++i)
            m_monitors[indexes.at(i)].position = rects.at(i).topLeft();
        normaliseMonitors();
    }
    updateArrangement();
    changed();
}

void DisplayPropertiesDialog::setExtended(bool extend)
{
    Monitor &monitor = m_monitors[m_monitor];
    if (monitor.enabled == extend)
        return;
    if (extend) {
        // Beside the desktop, at its top right.
        int right = 0;
        for (const Monitor &other : std::as_const(m_monitors)) {
            if (other.enabled)
                right = std::max(right, other.position.x() + monitorSize(other).width());
        }
        monitor.position = QPoint(right, 0);
    }
    monitor.enabled = extend;
    normaliseMonitors();
    showMonitor();
    updateArrangement();
    changed();
}

void DisplayPropertiesDialog::setPrimary()
{
    m_primary = m_monitors.at(m_monitor).id;
    showMonitor();
    changed();
}

void DisplayPropertiesDialog::monitorMoved(int index, const QPoint &position)
{
    if (index < 0 || index >= m_monitors.size() || !m_monitors.at(index).enabled)
        return;
    m_monitors[index].position = position;
    normaliseMonitors();
    updateArrangement();
    changed();
}

QSize DisplayPropertiesDialog::monitorSize(const Monitor &monitor) const
{
    QSize size
        = m_screens ? m_screens->logicalSize(monitor.name, monitor.mode, monitor.scale) : QSize();
    if (!size.isValid() || size.isEmpty())
        size = QSize(1024, 768);
    return size;
}

void DisplayPropertiesDialog::normaliseMonitors()
{
    QList<int> indexes;
    QList<QRect> rects;
    for (int i = 0; i < m_monitors.size(); ++i) {
        if (!m_monitors.at(i).enabled)
            continue;
        indexes.append(i);
        rects.append(QRect(m_monitors.at(i).position, monitorSize(m_monitors.at(i))));
    }
    rects = Daytona::normaliseMonitors(rects);
    for (int i = 0; i < indexes.size(); ++i)
        m_monitors[indexes.at(i)].position = rects.at(i).topLeft();
}

void DisplayPropertiesDialog::updateArrangement()
{
    QList<MonitorArrangement::Monitor> drawn;
    for (int i = 0; i < m_monitors.size(); ++i) {
        const Monitor &monitor = m_monitors.at(i);
        drawn.append({ QString::number(i + 1), QRect(monitor.position, monitorSize(monitor)),
            monitor.enabled });
    }
    const bool preparing = m_preparing;
    m_preparing = true;
    m_arrangement->setMonitors(drawn);
    m_arrangement->setSelected(m_monitor);
    m_preparing = preparing;
}

void DisplayPropertiesDialog::applyScreen()
{
    if (!m_screens || m_monitors.isEmpty())
        return;
    // The taskbar moves at once; nothing to confirm.
    if (m_settings && m_primary != m_appliedPrimary)
        m_settings->setPrimaryOutput(m_primary);
    m_appliedPrimary = m_primary;

    int enabled = 0;
    for (const Monitor &monitor : std::as_const(m_monitors))
        enabled += monitor.enabled ? 1 : 0;
    QList<ScreenChange> changes;
    QList<ScreenChange> undo;
    for (const Monitor &monitor : std::as_const(m_monitors)) {
        if (!monitor.canChange)
            continue;
        ScreenChange change;
        ScreenChange back;
        change.screen = back.screen = monitor.name;
        bool changed = false;
        if (monitor.enabled != monitor.appliedEnabled) {
            change.enabled = monitor.enabled;
            back.enabled = monitor.appliedEnabled;
            back.position = monitor.appliedPosition;
            changed = true;
        }
        if (monitor.enabled) {
            if (monitor.mode.isValid() && !monitor.mode.sameAs(monitor.appliedMode)) {
                change.mode = monitor.mode;
                back.mode = monitor.appliedMode;
                changed = true;
            }
            if (!sameScale(monitor.scale, monitor.appliedScale)) {
                change.scale = monitor.scale;
                back.scale = monitor.appliedScale;
                changed = true;
            }
            // A screen switched on is always told where to go.
            if (enabled > 1
                && (monitor.position != monitor.appliedPosition || !monitor.appliedEnabled)) {
                change.position = monitor.position;
                back.position = monitor.appliedPosition;
                changed = true;
            }
        }
        if (changed) {
            changes.append(change);
            undo.append(back);
        }
    }
    if (changes.isEmpty())
        return;

    const QList<Monitor> before = m_monitors;
    for (Monitor &monitor : m_monitors) {
        monitor.appliedMode = monitor.mode;
        monitor.appliedScale = monitor.scale;
        monitor.appliedEnabled = monitor.enabled;
        monitor.appliedPosition = monitor.position;
    }
    const QList<Monitor> after = m_monitors;

    QPointer<DisplayPropertiesDialog> self(this);
    // Back to the previous settings: in the compositor and in the tab.
    const auto revert = [self, before, undo](bool tellCompositor) {
        if (!self)
            return;
        if (tellCompositor)
            self->m_screens->change(undo, {});
        for (const Monitor &previous : before) {
            for (Monitor &monitor : self->m_monitors) {
                if (monitor.name != previous.name)
                    continue;
                monitor.mode = monitor.appliedMode = previous.appliedMode;
                monitor.scale = monitor.appliedScale = previous.appliedScale;
                monitor.enabled = monitor.appliedEnabled = previous.appliedEnabled;
                monitor.position = monitor.appliedPosition = previous.appliedPosition;
            }
        }
        // A primary monitor that was switched back off is no longer primary.
        bool primaryOn = false;
        for (const Monitor &monitor : std::as_const(self->m_monitors))
            primaryOn |= monitor.enabled && monitor.id == self->m_primary;
        if (!primaryOn) {
            for (const Monitor &monitor : std::as_const(self->m_monitors)) {
                if (monitor.enabled) {
                    self->m_primary = self->m_appliedPrimary = monitor.id;
                    if (self->m_settings)
                        self->m_settings->setPrimaryOutput(monitor.id);
                    break;
                }
            }
        }
        self->showMonitor();
        self->updateArrangement();
    };
    // What to remember once kept: what changed, and with more than one
    // screen on, where every one of them is.
    const auto save = [self, before, after, enabled] {
        if (!self || !self->m_settings)
            return;
        Settings *settings = self->m_settings;
        for (int i = 0; i < after.size() && i < before.size(); ++i) {
            const Monitor &now = after.at(i);
            const Monitor &was = before.at(i);
            if (!now.canChange)
                continue;
            if (now.enabled != was.appliedEnabled)
                settings->setOutputEnabled(now.id, now.enabled);
            if (!now.enabled)
                continue;
            if (!sameScale(now.scale, was.appliedScale))
                settings->setOutputScale(now.id, now.scale);
            if (now.mode.isValid() && !now.mode.sameAs(was.appliedMode))
                settings->setOutputMode(now.id, now.mode);
            if (enabled > 1)
                settings->setOutputPosition(now.id, now.position);
        }
    };
    m_screens->change(changes, [self, revert, save](bool ok) {
        if (!self)
            return;
        if (!ok) {
            revert(false);
            // Not modal: this runs inside the compositor's event.
            auto *box = new QMessageBox(QMessageBox::Warning, tr("Display Properties"),
                tr("The display settings could not be changed."), QMessageBox::Ok, self);
            box->setAttribute(Qt::WA_DeleteOnClose);
            box->open();
            return;
        }
        self->confirmSettings([revert, save](bool keep) {
            if (keep)
                save();
            else
                revert(true);
        });
    });
}

void DisplayPropertiesDialog::confirmSettings(const std::function<void(bool)> &done)
{
    if (m_confirm) {
        m_confirm(done);
        return;
    }
    // A wrong size or mode can make the screen hard to use, so new settings
    // are kept only when confirmed; otherwise the old ones come back.
    auto *box = new QMessageBox(QMessageBox::Question, tr("Display Properties"), QString(),
        QMessageBox::Yes | QMessageBox::No, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setDefaultButton(QMessageBox::No);
    auto remaining = std::make_shared<int>(15);
    const auto update = [box, remaining] {
        box->setText(tr("Do you want to keep these display settings?\n\n"
                        "The previous settings will be restored in %n second(s).",
            nullptr, *remaining));
    };
    update();
    auto *timer = new QTimer(box);
    connect(timer, &QTimer::timeout, box, [box, remaining, update] {
        if (--*remaining <= 0)
            box->done(QMessageBox::No);
        else
            update();
    });
    connect(
        box, &QMessageBox::finished, box, [done](int result) { done(result == QMessageBox::Yes); });
    timer->start(1000);
    box->open();
}

CustomFontSizeDialog::CustomFontSizeDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Custom Font Size"));
    auto *label = new QLabel(tr("&Scale to"), this);
    m_percent = new QComboBox(this);
    m_percent->setEditable(true);
    for (int percent : { 100, 125, 150, 175, 200, 250, 300 })
        m_percent->addItem(QString::number(percent));
    m_percent->setValidator(new QIntValidator(MinPercent, MaxPercent, m_percent));
    label->setBuddy(m_percent);
    auto *suffix = new QLabel(tr("% of normal size"), this);
    m_sample = new QLabel(this);
    m_sample->setFrameShape(QFrame::Panel);
    m_sample->setFrameShadow(QFrame::Sunken);
    m_sample->setAlignment(Qt::AlignCenter);
    m_sample->setMinimumSize(260, 80);
    m_sample->setAutoFillBackground(true);
    m_sample->setBackgroundRole(QPalette::Base);
    connect(m_percent, &QComboBox::currentTextChanged, this, &CustomFontSizeDialog::updateSample);

    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    for (QPushButton *b : { ok, cancel })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto *row = new QHBoxLayout;
    row->addWidget(label);
    row->addWidget(m_percent);
    row->addWidget(suffix);
    row->addStretch();
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(row);
    layout->addWidget(m_sample);
    layout->addLayout(buttons);
    updateSample();
    setFixedSize(sizeHint());
}

void CustomFontSizeDialog::setPercent(int percent)
{
    m_percent->setCurrentText(QString::number(qBound(MinPercent, percent, MaxPercent)));
}

int CustomFontSizeDialog::percent() const
{
    bool ok = false;
    const int value = m_percent->currentText().toInt(&ok);
    return ok ? qBound(MinPercent, value, MaxPercent) : MinPercent;
}

void CustomFontSizeDialog::updateSample()
{
    // NT showed a line of text at the chosen size.
    QFont sample = font();
    sample.setPointSizeF(sample.pointSizeF() * percent() / 100.0);
    m_sample->setFont(sample);
    m_sample->setText(tr("Sample text at %1%").arg(percent()));
}

void DisplayPropertiesDialog::accept()
{
    apply();
    QDialog::accept();
}

} // namespace Daytona
