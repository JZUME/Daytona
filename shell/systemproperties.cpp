// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemproperties.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Daytona {

namespace {

    // The picture of a computer: Chicago95's, as My Computer shows it, at
    // about the size of Windows 98's.
    constexpr int PictureSize = 96;
    // The distribution's logo beside Computer, where Windows 98 showed the
    // computer maker's (OEMLOGO.BMP).
    constexpr int LogoSize = 64;
    // How far a group's lines are indented under its heading.
    constexpr int Indent = 16;

    int percent(qint64 part, qint64 whole)
    {
        return whole > 0 ? int((part * 100 + whole / 2) / whole) : 0;
    }

    QString swapType(const QString &name, const QString &type)
    {
        const auto tr = [](const char *text) {
            return QCoreApplication::translate("Daytona::SystemPropertiesDialog", text);
        };
        // Compressed swap in memory, Debian and Fedora's default on some
        // installs.
        if (name.startsWith(QLatin1String("/dev/zram")))
            return tr("Compressed memory");
        if (type == QLatin1String("partition"))
            return tr("Partition");
        if (type == QLatin1String("file"))
            return tr("File");
        return type;
    }

} // namespace

SystemPropertiesDialog::SystemPropertiesDialog(QWidget *parent)
    : QDialog(parent)
    , m_read([] { return readSystemInfo(); })
{
    setWindowTitle(tr("System Properties"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("computer")));

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createGeneralPage(), tr("General"));
    m_tabs->addTab(createPerformancePage(), tr("Performance"));

    // Nothing here can be changed, but Windows 98 still had both.
    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    for (QPushButton *b : { ok, cancel })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->addWidget(m_tabs);
    main->addLayout(buttons);
    setMinimumWidth(420);
}

QWidget *SystemPropertiesDialog::createGeneralPage()
{
    // A grid, rebuilt by prepare(): the text in the second column, one
    // label a row; the picture beside the first groups and the logo beside
    // Computer, as Windows 98 placed the maker's logo.
    m_details = new QWidget;
    m_picture = new QLabel(m_details);
    m_picture->setObjectName(QStringLiteral("picture"));
    m_picture->setPixmap(
        QIcon::fromTheme(QStringLiteral("computer")).pixmap(PictureSize, PictureSize));
    m_picture->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_picture->setMinimumWidth(PictureSize + 2 * Indent);
    m_logo = new QLabel(m_details);
    m_logo->setObjectName(QStringLiteral("logo"));
    m_logo->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_logo->hide();

    auto *layout = new QGridLayout(m_details);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(0);
    layout->setColumnStretch(1, 1);
    return m_details;
}

QWidget *SystemPropertiesDialog::createPerformancePage()
{
    auto *page = new QWidget;

    auto *status = new QGroupBox(tr("Performance status"), page);
    auto *grid = new QGridLayout(status);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(12);
    const auto addRow = [grid, status](int row, const QString &label, QLabel **value) {
        auto *heading = new QLabel(label, status);
        grid->addWidget(heading, row, 0);
        *value = new QLabel(status);
        (*value)->setTextInteractionFlags(Qt::TextSelectableByMouse);
        grid->addWidget(*value, row, 1);
        return heading;
    };
    addRow(0, tr("Memory:"), &m_memory);
    addRow(1, tr("In use:"), &m_memoryInUse);
    m_memoryBar = new QProgressBar(status);
    m_memoryBar->setRange(0, 100);
    m_memoryBar->setTextVisible(false);
    grid->addWidget(m_memoryBar, 2, 1);
    grid->setRowMinimumHeight(3, 8);
    addRow(4, tr("Virtual memory:"), &m_swap);
    m_swapInUseHeading = addRow(5, tr("In use:"), &m_swapInUse);
    m_swapBar = new QProgressBar(status);
    m_swapBar->setRange(0, 100);
    m_swapBar->setTextVisible(false);
    grid->addWidget(m_swapBar, 6, 1);

    auto *areas = new QGroupBox(tr("Virtual memory"), page);
    m_swapList = new QTreeWidget(areas);
    m_swapList->setRootIsDecorated(false);
    m_swapList->setSelectionMode(QAbstractItemView::NoSelection);
    m_swapList->setFocusPolicy(Qt::NoFocus);
    m_swapList->setHeaderLabels({ tr("Name"), tr("Type"), tr("Size"), tr("In use") });
    m_swapList->header()->setStretchLastSection(false);
    m_swapList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column)
        m_swapList->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_swapList->setMinimumHeight(80);
    m_noSwap = new QLabel(
        tr("Virtual memory is disabled: this computer has no swap partition or file."), areas);
    m_noSwap->setWordWrap(true);
    auto *areasLayout = new QVBoxLayout(areas);
    areasLayout->addWidget(m_swapList);
    areasLayout->addWidget(m_noSwap);
    areasLayout->addStretch();

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(status);
    layout->addWidget(areas, 1);
    return page;
}

void SystemPropertiesDialog::prepare()
{
    const SystemInfo info = m_read();
    const QLocale locale;

    m_logo->setPixmap(info.logo.isNull() ? QPixmap() : info.logo.pixmap(LogoSize, LogoSize));
    m_logo->setVisible(!info.logo.isNull());

    // General: headings with their lines indented beneath, as Windows 98.
    auto *grid = static_cast<QGridLayout *>(m_details->layout());
    while (QLayoutItem *item = grid->takeAt(0)) {
        if (item->widget() != m_picture && item->widget() != m_logo)
            delete item->widget();
        delete item;
    }
    for (int row = 0; row < grid->rowCount(); ++row)
        grid->setRowStretch(row, 0);
    int row = 0;
    // Adds a group, returning its first row.
    const auto addGroup = [this, grid, &row](const QString &heading, const QStringList &lines) {
        const int first = row;
        auto *title = new QLabel(heading, m_details);
        if (first > 0)
            title->setContentsMargins(0, 14, 0, 0);
        grid->addWidget(title, row++, 1);
        for (const QString &line : lines) {
            if (line.isEmpty())
                continue;
            auto *label = new QLabel(line, m_details);
            label->setTextFormat(Qt::PlainText);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            label->setContentsMargins(Indent, 2, 0, 0);
            grid->addWidget(label, row++, 1);
        }
        return first;
    };
    addGroup(tr("System:"),
        { info.operatingSystem, tr("Daytona %1").arg(info.daytonaVersion), info.kernel });
    addGroup(tr("Registered to:"), { info.userName });
    addGroup(tr("Computer name:"), { info.computerName });
    QStringList computer = { info.processor.vendor, info.processor.model };
    if (info.processor.count > 1)
        computer.append(tr("%1 logical processors").arg(info.processor.count));
    computer.append(tr("%1 RAM").arg(memorySize(info.memory.total)));
    const int computerRow = addGroup(tr("Computer:"), computer);
    grid->addWidget(m_picture, 0, 0, computerRow, 1, Qt::AlignHCenter | Qt::AlignTop);
    // The logo's top level with the Computer heading's text.
    m_logo->setContentsMargins(0, 14, 0, 0);
    grid->addWidget(m_logo, computerRow, 0, row - computerRow, 1, Qt::AlignHCenter | Qt::AlignTop);
    grid->setRowStretch(row, 1);

    // Performance.
    const MemoryInfo &memory = info.memory;
    m_memory->setText(tr("%1 of RAM").arg(memorySize(memory.total)));
    m_memoryInUse->setText(
        tr("%1 (%2%)").arg(memorySize(memory.used())).arg(percent(memory.used(), memory.total)));
    m_memoryBar->setValue(percent(memory.used(), memory.total));
    const bool hasSwap = memory.swapTotal > 0;
    m_swap->setText(hasSwap ? memorySize(memory.swapTotal) : tr("Disabled"));
    m_swapInUse->setText(tr("%1 (%2%)")
            .arg(memorySize(memory.swapUsed()))
            .arg(percent(memory.swapUsed(), memory.swapTotal)));
    m_swapBar->setValue(percent(memory.swapUsed(), memory.swapTotal));
    // Without swap, "Disabled" says it all.
    m_swapInUseHeading->setVisible(hasSwap);
    m_swapInUse->setVisible(hasSwap);
    m_swapBar->setVisible(hasSwap);

    m_swapList->clear();
    for (const SwapArea &area : info.swapAreas) {
        auto *item = new QTreeWidgetItem(m_swapList,
            { area.name, swapType(area.name, area.type), memorySize(area.size),
                memorySize(area.used) });
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
    }
    m_swapList->setVisible(!info.swapAreas.isEmpty());
    m_noSwap->setVisible(info.swapAreas.isEmpty() && !hasSwap);
    m_tabs->setCurrentIndex(0);
}

QStringList SystemPropertiesDialog::generalText() const
{
    QStringList lines;
    // In grid order: row by row down the text column.
    auto *grid = static_cast<QGridLayout *>(m_details->layout());
    for (int row = 0; row < grid->rowCount(); ++row) {
        if (QLayoutItem *item = grid->itemAtPosition(row, 1)) {
            if (auto *label = qobject_cast<QLabel *>(item->widget()))
                lines.append(label->text());
        }
    }
    return lines;
}

} // namespace Daytona
