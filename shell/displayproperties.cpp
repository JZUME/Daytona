// SPDX-License-Identifier: GPL-3.0-or-later

#include "displayproperties.h"

#include "bevel.h"
#include "colourscheme.h"
#include "daytonastyle.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QLoggingCategory>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

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
    const QSize scaled = (QSizeF(m_wallpaper.size()) * scale).toSize().expandedTo(QSize(1, 1));
    const QImage small = m_wallpaper.scaled(scaled, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    painter.setClipRect(screen);
    if (m_mode == WallpaperMode::Tile) {
        painter.drawTiledPixmap(screen, QPixmap::fromImage(small));
    } else {
        painter.drawImage(screen.left() + (screen.width() - small.width()) / 2,
            screen.top() + (screen.height() - small.height()) / 2, small);
    }
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
    tabs->addTab(createAppearanceTab(), tr("Appearance"));

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
    m_tile = new QRadioButton(tr("&Tile"), group);
    m_centre = new QRadioButton(tr("&Center"), group);
    connect(m_wallpapers, &QListWidget::currentRowChanged, this, [this] {
        updateBackgroundPreview();
        changed();
    });
    for (QRadioButton *radio : { m_tile, m_centre }) {
        connect(radio, &QRadioButton::toggled, this, [this] {
            updateBackgroundPreview();
            changed();
        });
    }

    auto *groupLayout = new QGridLayout(group);
    groupLayout->addWidget(m_wallpapers, 0, 0, 5, 1);
    groupLayout->addWidget(browse, 0, 1);
    groupLayout->addWidget(displayLabel, 1, 1);
    groupLayout->addWidget(m_tile, 2, 1);
    groupLayout->addWidget(m_centre, 3, 1);
    groupLayout->setRowStretch(4, 1); // as NT: the options under Browse...

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(m_desktopPreview, 0, Qt::AlignHCenter);
    layout->addWidget(group);
    return page;
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

    auto *row = new QHBoxLayout;
    row->addWidget(m_schemes, 1);
    row->addWidget(load);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(m_appearancePreview, 0, Qt::AlignHCenter);
    layout->addWidget(label);
    layout->addLayout(row);
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
    (mode == WallpaperMode::Tile ? m_tile : m_centre)->setChecked(true);

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
    return m_tile->isChecked() ? WallpaperMode::Tile : WallpaperMode::Centre;
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
    m_apply->setEnabled(false);
}

void DisplayPropertiesDialog::accept()
{
    apply();
    QDialog::accept();
}

} // namespace Daytona
