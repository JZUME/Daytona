// SPDX-License-Identifier: GPL-3.0-or-later

// Shows each control the Daytona style draws, in every state, side by side.
//
//   daytona-style-preview                   open a window
//   daytona-style-preview --save FILE [--scale N]
//                                           render to a PNG instead
//
// States are drawn with explicit style options rather than real widgets, so
// pressed, hover and focused states can all be seen at once.

#include "daytonastyle.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCommandLineParser>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QRadioButton>
#include <QScrollBar>
#include <QStyleOption>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace {

struct StateColumn
{
    const char *name;
    QStyle::State state;
};

const StateColumn Columns[] = {
    { "Normal", QStyle::State_Enabled },
    { "Hover", QStyle::State_Enabled | QStyle::State_MouseOver },
    { "Pressed", QStyle::State_Enabled | QStyle::State_Sunken },
    { "Checked", QStyle::State_Enabled | QStyle::State_On },
    { "Disabled", QStyle::State_None },
    { "Focused", QStyle::State_Enabled | QStyle::State_HasFocus },
};

constexpr int LabelWidth = 120;
constexpr int CellWidth = 100;
constexpr int RowHeight = 40;
constexpr int Margin = 8;

class Preview : public QWidget
{
public:
    Preview()
    {
        setWindowTitle(QStringLiteral("Daytona style preview"));
        setFixedSize(sizeHint());
    }

    QSize sizeHint() const override
    {
        const int columns = int(std::size(Columns));
        return { Margin * 2 + LabelWidth + columns * CellWidth, Margin * 2 + RowHeight * 10 };
    }

    void render(QPainter *p) const
    {
        p->fillRect(rect(), palette().window());
        int y = Margin;
        for (int c = 0; c < int(std::size(Columns)); ++c) {
            const QRect header(Margin + LabelWidth + c * CellWidth, y, CellWidth, RowHeight);
            style()->drawItemText(p, header, Qt::AlignCenter, palette(), true,
                QString::fromLatin1(Columns[c].name), QPalette::WindowText);
        }
        y += RowHeight;

        row(p, y, QStringLiteral("Push button"), [&](QPainter *pp, QRect r, QStyle::State s) {
            QStyleOptionButton o;
            o.initFrom(this);
            o.rect = r.adjusted(4, 6, -4, -6);
            o.state = s;
            o.text = QStringLiteral("OK");
            if (s & QStyle::State_HasFocus) {
                // Focus is drawn by the widget, inside the bevel.
            }
            style()->drawControl(QStyle::CE_PushButton, &o, pp, this);
        });
        row(p, y, QStringLiteral("Default button"), [&](QPainter *pp, QRect r, QStyle::State s) {
            QStyleOptionButton o;
            o.initFrom(this);
            o.rect = r.adjusted(4, 6, -4, -6);
            o.state = s;
            o.features = QStyleOptionButton::DefaultButton;
            o.text = QStringLiteral("Cancel");
            style()->drawControl(QStyle::CE_PushButton, &o, pp, this);
        });
        row(p, y, QStringLiteral("Tool button"),
            [&](QPainter *pp, QRect r, QStyle::State s) { toolButton(pp, r, s, false); });
        row(p, y, QStringLiteral("Flat tool button"),
            [&](QPainter *pp, QRect r, QStyle::State s) { toolButton(pp, r, s, true); });
        row(p, y, QStringLiteral("Taskbar button"), [&](QPainter *pp, QRect r, QStyle::State s) {
            QStyleOptionToolButton o;
            o.initFrom(this);
            o.rect = r.adjusted(2, 8, -2, -8);
            o.state = toolButtonState(s, false);
            o.activeSubControls
                = (s & QStyle::State_Sunken) ? QStyle::SC_ToolButton : QStyle::SC_None;
            o.text = QStringLiteral("Notepad");
            o.toolButtonStyle = Qt::ToolButtonTextOnly;
            o.subControls = QStyle::SC_ToolButton;
            if (s & QStyle::State_On)
                o.font.setBold(true);
            style()->drawComplexControl(QStyle::CC_ToolButton, &o, pp, this);
        });
        row(p, y, QStringLiteral("Frame WinPanel"),
            [&](QPainter *pp, QRect r, QStyle::State s) { frame(pp, r, s, QFrame::WinPanel); });
        row(p, y, QStringLiteral("Frame Panel"),
            [&](QPainter *pp, QRect r, QStyle::State s) { frame(pp, r, s, QFrame::Panel); });
        row(p, y, QStringLiteral("Label"), [&](QPainter *pp, QRect r, QStyle::State s) {
            QStyleOptionFocusRect f;
            f.initFrom(this);
            style()->drawItemText(pp, r, Qt::AlignCenter, palette(), s & QStyle::State_Enabled,
                QStringLiteral("Label"), QPalette::WindowText);
            if (s & QStyle::State_HasFocus) {
                f.rect = r.adjusted(28, 10, -28, -10);
                style()->drawPrimitive(QStyle::PE_FrameFocusRect, &f, pp, this);
            }
        });
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        render(&p);
    }

private:
    template <typename F> void row(QPainter *p, int &y, const QString &label, F draw) const
    {
        style()->drawItemText(p, QRect(Margin, y, LabelWidth, RowHeight),
            Qt::AlignLeft | Qt::AlignVCenter, palette(), true, label, QPalette::WindowText);
        for (int c = 0; c < int(std::size(Columns)); ++c) {
            const QRect cell(Margin + LabelWidth + c * CellWidth, y, CellWidth, RowHeight);
            draw(p, cell, Columns[c].state);
        }
        y += RowHeight;
    }

    // The state flags QToolButton itself adds: a normal tool button is always
    // raised; an auto-raise one only while hovered.
    static QStyle::State toolButtonState(QStyle::State s, bool autoRaise)
    {
        if (autoRaise) {
            s |= QStyle::State_AutoRaise;
            if (s & QStyle::State_MouseOver)
                s |= QStyle::State_Raised;
        } else {
            s |= QStyle::State_Raised;
        }
        return s;
    }

    void toolButton(QPainter *p, QRect r, QStyle::State s, bool autoRaise) const
    {
        QStyleOptionToolButton o;
        o.initFrom(this);
        o.rect = QRect(r.center().x() - 12, r.center().y() - 12, 24, 24);
        o.state = toolButtonState(s, autoRaise);
        o.activeSubControls = (s & QStyle::State_Sunken) ? QStyle::SC_ToolButton : QStyle::SC_None;
        o.text = QStringLiteral("B");
        o.toolButtonStyle = Qt::ToolButtonTextOnly;
        o.subControls = QStyle::SC_ToolButton;
        style()->drawComplexControl(QStyle::CC_ToolButton, &o, p, this);
    }

    void frame(QPainter *p, QRect r, QStyle::State s, QFrame::Shape shape) const
    {
        for (int i = 0; i < 2; ++i) {
            QStyleOptionFrame o;
            o.initFrom(this);
            o.rect = QRect(r.x() + 10 + i * 44, r.y() + 8, 36, 24);
            o.state = s | (i == 0 ? QStyle::State_Raised : QStyle::State_Sunken);
            o.frameShape = shape;
            o.lineWidth = 1;
            style()->drawControl(QStyle::CE_ShapedFrame, &o, p, this);
        }
    }
};

// Renders a menu bar with an open item, a menu with every kind of item (one
// highlighted), a menu with 32 pixel icons and a tooltip, side by side.
QImage renderMenus(qreal ratio)
{
    QStyle *style = QApplication::style();
    const QIcon icon16 = style->standardIcon(QStyle::SP_DirIcon);
    const QIcon icon32 = style->standardIcon(QStyle::SP_ComputerIcon);

    QMenu menu;
    QAction *open = menu.addAction(icon16, QStringLiteral("&Open"));
    open->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    menu.addAction(QStringLiteral("&Save"));
    QAction *disabled = menu.addAction(QStringLiteral("Save &As..."));
    disabled->setEnabled(false);
    menu.addSeparator();
    QAction *check = menu.addAction(QStringLiteral("&Word Wrap"));
    check->setCheckable(true);
    check->setChecked(true);
    QActionGroup group(&menu);
    for (const QString &label :
        { QStringLiteral("&Large Icons"), QStringLiteral("S&mall Icons") }) {
        QAction *radio = menu.addAction(label);
        radio->setCheckable(true);
        group.addAction(radio);
    }
    group.actions().first()->setChecked(true);
    QMenu *sub = menu.addMenu(QStringLiteral("&Programs"));
    sub->addAction(QStringLiteral("Accessories"));
    menu.addSeparator();
    menu.addAction(QStringLiteral("E&xit"));
    // QMenu needs a window (and so a screen) before an item can be
    // highlighted, so pop it up on the offscreen platform first.
    menu.popup(QPoint(0, 0));
    menu.setActiveAction(open);

    QMenu bigMenu;
    bigMenu.setProperty(Daytona::Style::MenuIconSizeProperty, 32);
    bigMenu.addAction(icon32, QStringLiteral("&Programs"));
    bigMenu.addAction(icon32, QStringLiteral("&Documents"));
    bigMenu.addAction(icon32, QStringLiteral("&Settings"));
    bigMenu.popup(QPoint(0, 0));

    QMenuBar bar;
    QMenu *file = bar.addMenu(QStringLiteral("&File"));
    bar.addMenu(QStringLiteral("&Edit"));
    bar.addMenu(QStringLiteral("&View"));
    bar.resize(240, bar.sizeHint().height());
    bar.show();
    bar.setActiveAction(file->menuAction());
    file->hide();

    // Item states drawn directly, since an offscreen menu has no hover.
    struct ItemState
    {
        const char *text;
        QStyle::State state;
        QStyleOptionMenuItem::MenuItemType type;
    };
    const ItemState states[] = {
        { "Normal", QStyle::State_Enabled, QStyleOptionMenuItem::Normal },
        { "Selected", QStyle::State_Enabled | QStyle::State_Selected,
            QStyleOptionMenuItem::Normal },
        { "Disabled", QStyle::State_None, QStyleOptionMenuItem::Normal },
        { "Disabled selected", QStyle::State_Selected, QStyleOptionMenuItem::Normal },
        { "&Restore (default)", QStyle::State_Enabled, QStyleOptionMenuItem::DefaultItem },
        { "&Programs", QStyle::State_Enabled | QStyle::State_Selected,
            QStyleOptionMenuItem::SubMenu },
    };
    const int itemWidth = 170;
    const int itemHeight = menu.actionGeometry(open).height();

    const QSize size(menu.width() + bigMenu.width() + itemWidth + 220,
        qMax(menu.height(), int(std::size(states)) * itemHeight + 60) + bar.height() + 16);
    QImage image(size * ratio, QImage::Format_RGB32);
    image.setDevicePixelRatio(ratio);
    image.fill(QApplication::palette().color(QPalette::Window));
    QPainter p(&image);
    bar.render(&p, QPoint(8, 8));
    menu.render(&p, QPoint(8, 8 + bar.height()));
    bigMenu.render(&p, QPoint(16 + menu.width(), 8 + bar.height()));

    const int stripX = 24 + menu.width() + bigMenu.width();
    int y = 8 + bar.height();
    for (const ItemState &s : states) {
        QStyleOptionMenuItem o;
        o.initFrom(&menu);
        o.rect = QRect(stripX, y, itemWidth, itemHeight);
        o.state = s.state;
        o.menuItemType = s.type;
        o.text = QString::fromLatin1(s.text);
        o.maxIconWidth = 16;
        o.font = menu.font();
        style->drawControl(QStyle::CE_MenuItem, &o, &p, &menu);
        y += itemHeight;
    }

    // Menu bar items: closed and open.
    y += 8;
    int x = stripX;
    const QStyle::State barStates[] = { QStyle::State_Enabled,
        QStyle::State_Enabled | QStyle::State_Selected | QStyle::State_Sunken };
    for (QStyle::State state : barStates) {
        QStyleOptionMenuItem o;
        o.initFrom(&bar);
        o.rect = QRect(x, y, 48, bar.height());
        o.state = state;
        o.menuItemType = QStyleOptionMenuItem::Normal;
        o.text = QStringLiteral("&File");
        style->drawControl(QStyle::CE_MenuBarItem, &o, &p, &bar);
        x += 56;
    }
    y += bar.height() + 8;

    // A tooltip, drawn the way QToolTip draws its label.
    const QString tip = QStringLiteral("Click here to begin.");
    const QFontMetrics fm(QApplication::font());
    const QRect tipRect(QPoint(stripX, y), fm.size(0, tip) + QSize(8, 4));
    QStyleOptionFrame frame;
    frame.rect = tipRect;
    frame.palette = QApplication::palette();
    frame.state = QStyle::State_Enabled;
    style->drawPrimitive(QStyle::PE_PanelTipLabel, &frame, &p);
    p.setPen(frame.palette.color(QPalette::ToolTipText));
    p.drawText(tipRect, Qt::AlignCenter, tip);
    return image;
}

// Renders real dialog controls: text boxes, combo boxes, check boxes, radio
// buttons, a group box, tabs, scroll bars, a list and dialog buttons.
QImage renderInputs(qreal ratio)
{
    QWidget page;
    auto *grid = new QGridLayout(&page);

    auto *edit = new QLineEdit(QStringLiteral("notepad.exe"));
    auto *focused = new QLineEdit(QStringLiteral("Focused"));
    auto *disabledEdit = new QLineEdit(QStringLiteral("Disabled"));
    disabledEdit->setEnabled(false);
    auto *openLabel = new QLabel(QStringLiteral("&Open:"));
    openLabel->setBuddy(edit);
    grid->addWidget(openLabel, 0, 0);
    grid->addWidget(edit, 0, 1);
    grid->addWidget(focused, 0, 2);
    grid->addWidget(disabledEdit, 0, 3);

    auto *editableCombo = new QComboBox;
    editableCombo->setEditable(true);
    editableCombo->addItem(QStringLiteral("C:\\WINNT"));
    auto *combo = new QComboBox;
    combo->addItems({ QStringLiteral("Windows Standard"), QStringLiteral("High Contrast") });
    auto *disabledCombo = new QComboBox;
    disabledCombo->addItem(QStringLiteral("Disabled"));
    disabledCombo->setEnabled(false);
    grid->addWidget(new QLabel(QStringLiteral("Combo:")), 1, 0);
    grid->addWidget(editableCombo, 1, 1);
    grid->addWidget(combo, 1, 2);
    grid->addWidget(disabledCombo, 1, 3);

    auto *group = new QGroupBox(QStringLiteral("Taskbar Options"));
    auto *groupLayout = new QVBoxLayout(group);
    auto *onTop = new QCheckBox(QStringLiteral("&Always on top"));
    onTop->setChecked(true);
    auto *autoHide = new QCheckBox(QStringLiteral("A&uto hide"));
    auto *partial = new QCheckBox(QStringLiteral("Partially checked"));
    partial->setTristate(true);
    partial->setCheckState(Qt::PartiallyChecked);
    auto *disabledCheck = new QCheckBox(QStringLiteral("Disabled checked"));
    disabledCheck->setChecked(true);
    disabledCheck->setEnabled(false);
    for (QWidget *w : { static_cast<QWidget *>(onTop), static_cast<QWidget *>(autoHide),
             static_cast<QWidget *>(partial), static_cast<QWidget *>(disabledCheck) }) {
        groupLayout->addWidget(w);
    }
    auto *shutDown = new QRadioButton(QStringLiteral("&Shut down the computer?"));
    shutDown->setChecked(true);
    auto *restart = new QRadioButton(QStringLiteral("&Restart the computer?"));
    auto *disabledRadio = new QRadioButton(QStringLiteral("Disabled"));
    disabledRadio->setEnabled(false);
    groupLayout->addWidget(shutDown);
    groupLayout->addWidget(restart);
    groupLayout->addWidget(disabledRadio);
    grid->addWidget(group, 2, 0, 1, 2);

    auto *tabs = new QTabWidget;
    tabs->addTab(new QLabel(QStringLiteral("  Background page")), QStringLiteral("Background"));
    tabs->addTab(new QWidget, QStringLiteral("Appearance"));
    tabs->addTab(new QWidget, QStringLiteral("Settings"));
    grid->addWidget(tabs, 2, 2, 1, 2);

    auto *list = new QListWidget;
    list->addItems({ QStringLiteral("(None)"), QStringLiteral("Clouds"), QStringLiteral("Forest"),
        QStringLiteral("Leaves"), QStringLiteral("Tiles"), QStringLiteral("Waves") });
    list->setCurrentRow(1);
    list->setFixedHeight(80);
    grid->addWidget(list, 3, 0, 1, 2);

    auto *vbar = new QScrollBar(Qt::Vertical);
    vbar->setRange(0, 100);
    vbar->setValue(30);
    vbar->setFixedHeight(80);
    auto *hbar = new QScrollBar(Qt::Horizontal);
    hbar->setRange(0, 100);
    hbar->setValue(60);
    grid->addWidget(vbar, 3, 2, Qt::AlignLeft);
    grid->addWidget(hbar, 3, 3, Qt::AlignTop);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    grid->addWidget(buttons, 4, 0, 1, 4);

    page.resize(page.sizeHint().expandedTo(QSize(640, 0)));
    page.show();
    page.activateWindow();
    focused->setFocus();
    QApplication::processEvents();

    QImage image(page.size() * ratio, QImage::Format_RGB32);
    image.setDevicePixelRatio(ratio);
    page.render(&image);
    return image;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle(new Daytona::Style);
    QApplication::setPalette(Daytona::Style::ntPalette());

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption save(
        QStringLiteral("save"), QStringLiteral("Render to a PNG file."), QStringLiteral("file"));
    const QCommandLineOption scale(QStringLiteral("scale"),
        QStringLiteral("Device pixel ratio for --save."), QStringLiteral("factor"),
        QStringLiteral("1"));
    const QCommandLineOption saveMenus(QStringLiteral("save-menus"),
        QStringLiteral("Render menus, the menu bar and a tooltip to a PNG file."),
        QStringLiteral("file"));
    const QCommandLineOption saveInputs(QStringLiteral("save-inputs"),
        QStringLiteral("Render dialog controls to a PNG file."), QStringLiteral("file"));
    parser.addOptions({ save, scale, saveMenus, saveInputs });
    parser.process(app);

    if (parser.isSet(saveInputs))
        return renderInputs(parser.value(scale).toDouble()).save(parser.value(saveInputs)) ? 0 : 1;
    if (parser.isSet(saveMenus))
        return renderMenus(parser.value(scale).toDouble()).save(parser.value(saveMenus)) ? 0 : 1;

    Preview preview;
    if (parser.isSet(save)) {
        const qreal ratio = parser.value(scale).toDouble();
        QImage image(preview.size() * ratio, QImage::Format_RGB32);
        image.setDevicePixelRatio(ratio);
        QPainter painter(&image);
        preview.render(&painter);
        painter.end();
        return image.save(parser.value(save)) ? 0 : 1;
    }
    preview.show();
    return app.exec();
}
