// SPDX-License-Identifier: GPL-3.0-or-later

#include "screenchooser.h"

#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextStream>
#include <QVBoxLayout>
#include <cstdio>

namespace Daytona {

ScreenSource parseScreenSource(const QString &line)
{
    ScreenSource source;
    source.line = line;
    static const QString monitor = QStringLiteral("Monitor: ");
    static const QString window = QStringLiteral("Window: ");
    if (line.startsWith(window)) {
        source.window = true;
        source.name = line.mid(window.size());
        // "TITLE (ID)": the identifier is no use to the user.
        const qsizetype open = source.name.lastIndexOf(QLatin1String(" ("));
        if (open >= 0 && source.name.endsWith(QLatin1Char(')')))
            source.name.truncate(open);
    } else if (line.startsWith(monitor)) {
        const QString rest = line.mid(monitor.size());
        const qsizetype space = rest.indexOf(QLatin1Char(' '));
        source.name = space < 0 ? rest : rest.left(space);
        source.description = space < 0 ? QString() : rest.mid(space + 1).trimmed();
    } else {
        source.name = line;
    }
    if (source.name.isEmpty())
        source.name = line;
    return source;
}

QStringList screenSourceLabels(const QList<ScreenSource> &sources)
{
    qsizetype screens = 0;
    for (const ScreenSource &source : sources)
        screens += source.window ? 0 : 1;
    QStringList labels;
    for (const ScreenSource &source : sources) {
        if (source.window)
            labels << source.name;
        else if (screens == 1)
            labels << ScreenChooser::tr("Entire screen");
        else
            labels << (source.description.isEmpty() ? source.name : source.description);
    }
    return labels;
}

ScreenChooser::ScreenChooser(const QList<ScreenSource> &sources, QWidget *parent)
    : QDialog(parent)
    , m_sources(sources)
{
    setWindowTitle(tr("Share Screen"));

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon::fromTheme(QStringLiteral("video-display")).pixmap(32));
    icon->setAlignment(Qt::AlignTop);
    auto *text = new QLabel(
        tr("A program wants to share what is on your screen. Choose what it may share:"), this);
    text->setWordWrap(true);

    m_list = new QListWidget(this);
    const QStringList labels = screenSourceLabels(sources);
    for (qsizetype i = 0; i < sources.size(); ++i) {
        const QIcon itemIcon
            = QIcon::fromTheme(sources[i].window ? QStringLiteral("application-x-executable")
                                                 : QStringLiteral("video-display"));
        m_list->addItem(new QListWidgetItem(itemIcon, labels[i]));
    }
    m_list->setIconSize(QSize(16, 16));
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
    connect(m_list, &QListWidget::itemActivated, this, &QDialog::accept);

    auto *ok = new QPushButton(tr("OK"), this);
    ok->setDefault(true);
    ok->setEnabled(m_list->count() > 0);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    for (QPushButton *b : { ok, cancel })
        b->setMinimumSize(75, 23);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto *top = new QHBoxLayout;
    top->addWidget(icon);
    top->addWidget(text, 1);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    auto *layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_list);
    layout->addLayout(buttons);
    resize(347, 240);
}

QString ScreenChooser::chosenLine() const
{
    const int row = m_list->currentRow();
    return row >= 0 && row < m_sources.size() ? m_sources[row].line : QString();
}

int runScreenChooser()
{
    QList<ScreenSource> sources;
    QFile in;
    if (in.open(stdin, QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&in);
        QString line;
        while (stream.readLineInto(&line)) {
            if (!line.trimmed().isEmpty())
                sources << parseScreenSource(line);
        }
    }

    ScreenChooser chooser(sources);
    if (chooser.exec() != QDialog::Accepted)
        return 0;
    const QByteArray chosen = chooser.chosenLine().toUtf8();
    if (chosen.isEmpty())
        return 0;
    std::fwrite(chosen.constData(), 1, size_t(chosen.size()), stdout);
    std::fputc('\n', stdout);
    return 0;
}

} // namespace Daytona
