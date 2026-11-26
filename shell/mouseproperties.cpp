// SPDX-License-Identifier: GPL-3.0-or-later

#include "mouseproperties.h"

#include "xcursor.h"

#include <QComboBox>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace Daytona {

namespace {

    // NT's list of pointers, with the cursor names that show them.
    struct PointerRole
    {
        const char *text;
        const char *cursor;
    };
    const PointerRole Roles[] = {
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Normal Select"), "left_ptr" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Help Select"), "question_arrow" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Working In Background"),
            "left_ptr_watch" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Busy"), "watch" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Precision Select"), "crosshair" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Text Select"), "xterm" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Handwriting"), "pencil" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Unavailable"), "not-allowed" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Vertical Resize"),
            "sb_v_double_arrow" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Horizontal Resize"),
            "sb_h_double_arrow" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Diagonal Resize 1"), "nwse-resize" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Diagonal Resize 2"), "nesw-resize" },
        { QT_TRANSLATE_NOOP("Daytona::MousePropertiesDialog", "Move"), "fleur" },
    };

} // namespace

QPixmap pointerPreview(const QString &cursorName, PointerScheme scheme, qreal ratio)
{
    const QString file = findCursorFile(QString::fromLatin1(CursorThemeName), cursorName);
    QFile in(file);
    if (file.isEmpty() || !in.open(QIODevice::ReadOnly))
        return {};
    const int size = pointerSchemeSize(scheme);
    const QList<XcursorImage> images
        = imagesForSize(readXcursor(in.read(16 * 1024 * 1024)), qRound(size * ratio));
    if (images.isEmpty())
        return {};
    // The image is drawn at the scheme's size, however many pixels it has.
    QPixmap pixmap = QPixmap::fromImage(images.first().image);
    pixmap.setDevicePixelRatio(qreal(images.first().nominal) / size);
    return pixmap;
}

MousePropertiesDialog::MousePropertiesDialog(Settings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Mouse Properties"));
    auto *tabs = new QTabWidget(this);

    auto *page = new QWidget;
    auto *group = new QGroupBox(tr("Scheme"), page);
    m_schemes = new QComboBox(group);
    m_schemes->addItem(tr("Standard"), int(PointerScheme::Standard));
    m_schemes->addItem(tr("Standard (large)"), int(PointerScheme::Large));
    m_schemes->addItem(tr("Standard (extra large)"), int(PointerScheme::ExtraLarge));
    connect(m_schemes, &QComboBox::currentIndexChanged, this, [this] {
        updatePointers();
        if (!m_preparing)
            m_apply->setEnabled(true);
    });
    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(m_schemes);

    m_pointers = new QTreeWidget(page);
    m_pointers->setColumnCount(2);
    m_pointers->setHeaderHidden(true);
    m_pointers->setRootIsDecorated(false);
    m_pointers->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pointers->header()->setStretchLastSection(false);
    m_pointers->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_pointers->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    for (const PointerRole &role : Roles) {
        auto *item = new QTreeWidgetItem(m_pointers, { tr(role.text) });
        item->setData(1, Qt::UserRole, QString::fromLatin1(role.cursor));
    }
    m_pointers->setMinimumSize(320, 260);

    auto *note = new QLabel(tr("A new scheme is used from the next time you log on."), page);
    note->setWordWrap(true);

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(group);
    layout->addWidget(m_pointers, 1);
    layout->addWidget(note);
    tabs->addTab(page, tr("Pointers"));

    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    m_apply = new QPushButton(tr("&Apply"), this);
    for (QPushButton *b : { ok, cancel, m_apply })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &MousePropertiesDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &MousePropertiesDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &MousePropertiesDialog::apply);
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

PointerScheme MousePropertiesDialog::selectedScheme() const
{
    return PointerScheme(m_schemes->currentData().toInt());
}

void MousePropertiesDialog::prepare()
{
    m_preparing = true;
    const PointerScheme scheme = m_settings ? m_settings->pointerScheme() : PointerScheme::Standard;
    m_schemes->setCurrentIndex(m_schemes->findData(int(scheme)));
    updatePointers();
    m_apply->setEnabled(false);
    m_preparing = false;
}

void MousePropertiesDialog::updatePointers()
{
    // Each pointer as the selected scheme draws it.
    const PointerScheme scheme = selectedScheme();
    const int size = pointerSchemeSize(scheme);
    m_pointers->setIconSize(QSize(size, size));
    for (int i = 0; i < m_pointers->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_pointers->topLevelItem(i);
        item->setIcon(1,
            QIcon(pointerPreview(
                item->data(1, Qt::UserRole).toString(), scheme, devicePixelRatioF())));
    }
}

void MousePropertiesDialog::apply()
{
    if (m_settings && selectedScheme() != m_settings->pointerScheme()) {
        m_settings->setPointerScheme(selectedScheme());
        // As NT said of settings that wait for the next log on.
        auto *box = new QMessageBox(QMessageBox::Information, tr("Mouse Properties"),
            tr("The new pointers will be used the next time you log on."), QMessageBox::Ok, this);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    }
    m_apply->setEnabled(false);
}

void MousePropertiesDialog::accept()
{
    apply();
    QDialog::accept();
}

} // namespace Daytona
