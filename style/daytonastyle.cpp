// SPDX-License-Identifier: GPL-3.0-or-later

#include "daytonastyle.h"

#include "bevel.h"
#include "glyphs.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QStyleFactory>
#include <QStyleOption>

namespace Daytona {

SchemeColours Style::s_colours = standardColours();

Style::Style()
    : QProxyStyle(QStyleFactory::create(QStringLiteral("windows")))
{
    setObjectName(QStringLiteral("daytona"));
}

QPalette Style::ntPalette()
{
    return paletteFromColours(standardColours());
}

QColor Style::schemeColour(const QString &key)
{
    const QColor colour = s_colours.value(key);
    return colour.isValid() ? colour : standardColours().value(key);
}

QBrush Style::titleBarBrush(const QRect &rect)
{
    return Daytona::titleBarBrush(s_colours, true, rect);
}

QPalette Style::standardPalette() const
{
    return paletteFromColours(s_colours);
}

void Style::setColourFile(const QString &path)
{
    m_colourFile = path;
}

void Style::watchColourFile()
{
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        m_reload.setSingleShot(true);
        // Writers replace the file (write, then rename), and editors may
        // touch it several times: wait for things to settle.
        m_reload.setInterval(100);
        connect(&m_reload, &QTimer::timeout, this, &Style::loadColourFile);
        connect(
            m_watcher, &QFileSystemWatcher::fileChanged, &m_reload, qOverload<>(&QTimer::start));
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, &m_reload,
            qOverload<>(&QTimer::start));
    }
    // Watch the file and its directory, or the nearest directory that exists
    // so a file created later is noticed. A replaced file drops out of the
    // watch, so this is redone on every reload.
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    if (QFileInfo::exists(m_colourFile))
        m_watcher->addPath(m_colourFile);
    QString dir = QFileInfo(m_colourFile).absolutePath();
    while (!QFileInfo(dir).isDir() && dir != QDir(dir).absoluteFilePath(QStringLiteral("..")))
        dir = QFileInfo(dir).absolutePath();
    if (QFileInfo(dir).isDir())
        m_watcher->addPath(dir);
}

void Style::loadColourFile()
{
    if (m_colourFile.isEmpty())
        return;
    const std::optional<ColourFile> file = readColourFile(m_colourFile);
    const SchemeColours colours = file ? file->colours : standardColours();
    if (m_watcher)
        watchColourFile();
    if (colours == s_colours && m_applied)
        return;
    s_colours = colours;
    m_applied = true;
    QApplication::setPalette(paletteFromColours(colours));
    Q_EMIT coloursChanged();
}

namespace {

    // Menu item layout, in logical pixels.
    constexpr int MenuItemVPadding = 2; // above and below text or icon
    constexpr int MenuCheckMargin = 3; // either side of the check/icon column
    constexpr int MenuTextGap = 4; // check column to text
    constexpr int MenuShortcutGap = 16; // text to shortcut
    constexpr int MenuArrowWidth = 16; // room for the submenu arrow
    constexpr int MenuSeparatorHeight = 8;
    constexpr int DefaultMenuIconSize = 16;

    int menuIconSize(const QWidget *widget)
    {
        if (widget) {
            const int size = widget->property(Style::MenuIconSizeProperty).toInt();
            if (size > 0)
                return size;
        }
        return DefaultMenuIconSize;
    }

    int checkColumnWidth(const QStyleOptionMenuItem *item)
    {
        return qMax(item->maxIconWidth, DefaultMenuIconSize) + 2 * MenuCheckMargin;
    }

} // namespace

void Style::polish(QApplication *application)
{
    QProxyStyle::polish(application);
    // The colour scheme set by daytona-themetool, followed while it changes.
    if (m_colourFile.isEmpty())
        m_colourFile = colourFilePath();
    loadColourFile();
    watchColourFile();
    // Applications without an icon theme of their own (no platform theme
    // set one) use Chicago95's, installed with Daytona, rather than
    // Qt's empty fallback.
    const QString theme = QIcon::themeName();
    if (theme.isEmpty() || theme == QLatin1String("hicolor"))
        QIcon::setThemeName(QStringLiteral("Chicago95"));
    // NT menus, tooltips and combo box lists appear instantly.
    for (Qt::UIEffect effect : { Qt::UI_AnimateMenu, Qt::UI_FadeMenu, Qt::UI_AnimateCombo,
             Qt::UI_AnimateTooltip, Qt::UI_FadeTooltip, Qt::UI_AnimateToolBox }) {
        QApplication::setEffectEnabled(effect, false);
    }
}

void Style::drawMenuItem(const QStyleOptionMenuItem *item, QPainter *painter) const
{
    const QPalette &pal = item->palette;
    const QRect rect = item->rect;

    if (item->menuItemType == QStyleOptionMenuItem::Separator) {
        // Etched line: shadow above highlight.
        const int y = rect.center().y();
        painter->fillRect(QRect(rect.x() + 1, y, rect.width() - 2, 1), pal.color(QPalette::Dark));
        painter->fillRect(
            QRect(rect.x() + 1, y + 1, rect.width() - 2, 1), pal.color(QPalette::Light));
        return;
    }

    const bool enabled = item->state & State_Enabled;
    const bool selected = item->state & State_Selected;
    painter->fillRect(rect, selected ? pal.highlight() : pal.window());

    const QColor textColour = selected
        ? (enabled ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Dark))
        : pal.color(QPalette::WindowText);

    // Check column: icon, check mark or radio bullet, centred.
    const int checkWidth = checkColumnWidth(item);
    const QRect checkRect(rect.x(), rect.y(), checkWidth, rect.height());
    if (!item->icon.isNull()) {
        const int size = qMax(item->maxIconWidth, DefaultMenuIconSize);
        const QIcon::Mode mode = enabled ? QIcon::Normal : QIcon::Disabled;
        const QRect iconRect
            = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, QSize(size, size), checkRect);
        item->icon.paint(painter, iconRect, Qt::AlignCenter, mode);
    } else if (item->checked) {
        const Glyph::Kind kind = item->checkType == QStyleOptionMenuItem::Exclusive
            ? Glyph::Kind::Bullet
            : Glyph::Kind::Check;
        const QRect glyphRect
            = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, Glyph::size(kind), checkRect);
        if (!enabled && !selected)
            Glyph::draw(
                painter, kind, glyphRect.topLeft() + QPoint(1, 1), pal.color(QPalette::Light));
        Glyph::draw(painter, kind, glyphRect.topLeft(),
            enabled || selected ? textColour : pal.color(QPalette::Dark));
    }

    // QMenu passes "text<tab>shortcut".
    QString text = item->text;
    QString shortcut;
    const int tab = text.indexOf(QLatin1Char('\t'));
    if (tab >= 0) {
        shortcut = text.mid(tab + 1);
        text.truncate(tab);
    }
    QFont font = item->font;
    if (item->menuItemType == QStyleOptionMenuItem::DefaultItem)
        font.setBold(true);
    painter->save();
    painter->setFont(font);
    const int textFlags
        = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
    const QRect textRect = rect.adjusted(checkWidth + MenuTextGap, 0, -MenuArrowWidth, 0);
    QPalette textPalette = pal;
    textPalette.setColor(QPalette::WindowText, textColour);
    // Disabled items are embossed unless highlighted, where NT uses grey text.
    const bool emboss = !enabled && !selected;
    drawItemText(painter, textRect, textFlags | Qt::AlignLeft, textPalette, !emboss, text,
        QPalette::WindowText);
    if (!shortcut.isEmpty()) {
        drawItemText(painter, textRect, textFlags | Qt::AlignRight, textPalette, !emboss, shortcut,
            QPalette::WindowText);
    }
    painter->restore();

    if (item->menuItemType == QStyleOptionMenuItem::SubMenu) {
        const QSize arrow = Glyph::size(Glyph::Kind::ArrowRight);
        const QPoint at(rect.right() - MenuArrowWidth / 2 - arrow.width() / 2,
            rect.y() + (rect.height() - arrow.height()) / 2);
        Glyph::draw(painter, Glyph::Kind::ArrowRight, at,
            enabled || selected ? textColour : pal.color(QPalette::Dark));
    }
}

void Style::drawMenuBarItem(
    const QStyleOptionMenuItem *item, QPainter *painter, const QWidget *widget) const
{
    const QPalette &pal = item->palette;
    // NT 4.0 menu bars are flat: an item is highlighted while its menu is
    // open or while it is chosen with the keyboard, never on mere hover.
    const bool open = item->state & State_Sunken;
    const bool keyboard = (item->state & State_Selected) && widget && widget->hasFocus();
    const bool highlighted = (open || keyboard) && (item->state & State_Enabled);
    painter->fillRect(item->rect, highlighted ? pal.highlight() : pal.window());
    QPalette textPalette = pal;
    textPalette.setColor(QPalette::ButtonText,
        highlighted ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::WindowText));
    drawItemText(painter, item->rect,
        Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine, textPalette,
        item->state & State_Enabled, item->text, QPalette::ButtonText);
}

void Style::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
    const QWidget *widget) const
{
    switch (element) {
    case PE_PanelButtonCommand:
    case PE_PanelButtonBevel: {
        const bool down = option->state & (State_Sunken | State_On);
        painter->fillRect(option->rect, option->palette.button());
        if (down)
            Bevel::pressed(painter, option->rect, option->palette);
        else
            Bevel::raised(painter, option->rect, option->palette);
        return;
    }
    case PE_PanelButtonTool: {
        const bool down = option->state & State_Sunken;
        const bool latched = (option->state & State_On) && !down;
        const bool autoRaise = option->state & State_AutoRaise;
        if (autoRaise && !(option->state & (State_MouseOver | State_Sunken | State_On))
            && option->state & State_Enabled) {
            return;
        }
        if (latched) {
            // NT draws a latched (checked) button, such as the active
            // window's taskbar button, pressed over a checkerboard of the
            // face and highlight colours.
            painter->fillRect(option->rect, option->palette.button());
            painter->fillRect(
                option->rect, QBrush(option->palette.color(QPalette::Light), Qt::Dense4Pattern));
        } else {
            painter->fillRect(option->rect, option->palette.button());
        }
        if (down || latched)
            Bevel::pressed(painter, option->rect, option->palette);
        else if (autoRaise)
            Bevel::raisedThin(painter, option->rect, option->palette);
        else
            Bevel::raised(painter, option->rect, option->palette);
        return;
    }
    case PE_FrameDefaultButton:
        Bevel::edge(painter, option->rect, option->palette.color(QPalette::Shadow),
            option->palette.color(QPalette::Shadow));
        return;
    case PE_FrameFocusRect:
        Bevel::focusRect(painter, option->rect, option->palette.color(QPalette::WindowText));
        return;
    case PE_IndicatorCheckBox: {
        // NT: a sunken white box with a black check. A partially checked
        // box shows a grey check over a dithered face; a disabled one has a
        // grey face.
        const QPalette &pal = option->palette;
        const QRect box = option->rect;
        const bool enabled = option->state & State_Enabled;
        const bool partial = option->state & State_NoChange;
        if (partial) {
            painter->fillRect(box, pal.button());
            painter->fillRect(box, QBrush(pal.color(QPalette::Light), Qt::Dense4Pattern));
        } else {
            painter->fillRect(
                box, enabled && !(option->state & State_Sunken) ? pal.base() : pal.button());
        }
        Bevel::sunken(painter, box, pal);
        if (option->state & (State_On | State_NoChange)) {
            const QRect glyph = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                Glyph::size(Glyph::Kind::Check), box.adjusted(2, 2, -2, -2));
            const QColor colour
                = enabled && !partial ? pal.color(QPalette::Text) : pal.color(QPalette::Dark);
            Glyph::draw(painter, Glyph::Kind::Check, glyph.topLeft(), colour);
        }
        return;
    }
    case PE_IndicatorRadioButton: {
        const QPalette &pal = option->palette;
        const bool enabled = option->state & State_Enabled;
        const QColor field = enabled && !(option->state & State_Sunken)
            ? pal.color(QPalette::Base)
            : pal.color(QPalette::Button);
        const QColor dot = enabled ? pal.color(QPalette::Text) : pal.color(QPalette::Dark);
        const QRect area = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
            QSize(Glyph::RadioSize, Glyph::RadioSize), option->rect);
        Glyph::drawRadio(painter, area.topLeft(), pal.color(QPalette::Dark),
            pal.color(QPalette::Light), pal.color(QPalette::Shadow), pal.color(QPalette::Midlight),
            field, (option->state & State_On) ? &dot : nullptr);
        return;
    }
    case PE_PanelLineEdit: {
        // Text boxes: field colour inside a 2 pixel sunken edge.
        // A frameless line edit (e.g. inside an editable combo box) fills
        // its whole rectangle.
        const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option);
        const bool framed = frame && frame->lineWidth > 0;
        painter->fillRect(
            framed ? option->rect.adjusted(2, 2, -2, -2) : option->rect, option->palette.base());
        if (framed)
            Bevel::sunken(painter, option->rect, option->palette);
        return;
    }
    case PE_FrameLineEdit:
        Bevel::sunken(painter, option->rect, option->palette);
        return;
    case PE_FrameMenu:
        Bevel::raised(painter, option->rect, option->palette);
        return;
    case PE_PanelMenu:
        painter->fillRect(option->rect, option->palette.window());
        return;
    case PE_PanelMenuBar:
        // NT menu bars have no frame.
        return;
    case PE_PanelTipLabel:
        // NT tooltip: info background with a 1 pixel border in the tooltip
        // text colour, no shadow and square corners.
        painter->fillRect(option->rect, option->palette.toolTipBase());
        Bevel::edge(painter, option->rect, option->palette.color(QPalette::ToolTipText),
            option->palette.color(QPalette::ToolTipText));
        return;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void Style::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
    const QWidget *widget) const
{
    switch (element) {
    case CE_PushButtonBevel: {
        const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
        if (!button)
            break;
        QRect rect = button->rect;
        const bool isDefault = button->features & QStyleOptionButton::DefaultButton;
        const bool down = button->state & (State_Sunken | State_On);
        if (isDefault) {
            // NT: a default button has an extra black outline; pressed, it
            // shows only the outline and a flat grey border.
            Bevel::edge(painter, rect, option->palette.color(QPalette::Shadow),
                option->palette.color(QPalette::Shadow));
            rect.adjust(1, 1, -1, -1);
            if (down) {
                painter->fillRect(rect, option->palette.button());
                Bevel::edge(painter, rect, option->palette.color(QPalette::Dark),
                    option->palette.color(QPalette::Dark));
                return;
            }
        }
        QStyleOptionButton inner = *button;
        inner.rect = rect;
        drawPrimitive(PE_PanelButtonCommand, &inner, painter, widget);
        return;
    }
    case CE_MenuItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            drawMenuItem(item, painter);
            return;
        }
        break;
    case CE_MenuBarItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            drawMenuBarItem(item, painter, widget);
            return;
        }
        break;
    case CE_MenuEmptyArea:
        painter->fillRect(option->rect, option->palette.window());
        return;
    case CE_MenuBarEmptyArea:
        painter->fillRect(option->rect, option->palette.window());
        return;
    case CE_ShapedFrame: {
        const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option);
        if (!frame)
            break;
        const bool sunkenShadow = frame->state & State_Sunken;
        const bool raisedShadow = frame->state & State_Raised;
        if (frame->frameShape == QFrame::WinPanel) {
            if (sunkenShadow)
                Bevel::sunken(painter, frame->rect, frame->palette);
            else if (raisedShadow)
                Bevel::raised(painter, frame->rect, frame->palette);
            return;
        }
        if (frame->frameShape == QFrame::StyledPanel) {
            // Scroll areas, lists and text views: NT's 2 pixel client edge.
            if (sunkenShadow)
                Bevel::sunken(painter, frame->rect, frame->palette);
            else if (raisedShadow)
                Bevel::raised(painter, frame->rect, frame->palette);
            return;
        }
        if (frame->frameShape == QFrame::Panel) {
            if (sunkenShadow)
                Bevel::sunkenThin(painter, frame->rect, frame->palette);
            else if (raisedShadow)
                Bevel::raisedThin(painter, frame->rect, frame->palette);
            return;
        }
        break;
    }
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void Style::drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &palette,
    bool enabled, const QString &text, QPalette::ColorRole textRole) const
{
    if (enabled || text.isEmpty()) {
        QProxyStyle::drawItemText(painter, rect, flags, palette, enabled, text, textRole);
        return;
    }
    // NT embosses disabled text: a highlight copy one pixel down and right,
    // with the shadow colour on top.
    const QPen savedPen = painter->pen();
    painter->setPen(palette.color(QPalette::Light));
    painter->drawText(rect.translated(1, 1), flags, text);
    painter->setPen(palette.color(QPalette::Dark));
    painter->drawText(rect, flags, text);
    painter->setPen(savedPen);
}

QSize Style::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contents,
    const QWidget *widget) const
{
    if (type == CT_MenuItem) {
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            if (item->menuItemType == QStyleOptionMenuItem::Separator)
                return { contents.width(), MenuSeparatorHeight };
            const int iconSize = qMax(item->maxIconWidth, DefaultMenuIconSize);
            const int height = qMax(item->fontMetrics.height(), iconSize) + 2 * MenuItemVPadding;
            int width = checkColumnWidth(item) + MenuTextGap + contents.width() + MenuArrowWidth;
            if (item->text.contains(QLatin1Char('\t')))
                width += MenuShortcutGap;
            return { width, height };
        }
    }
    return QProxyStyle::sizeFromContents(type, option, contents, widget);
}

int Style::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_SmallIconSize:
        if (qobject_cast<const QMenu *>(widget))
            return menuIconSize(widget);
        break;
    case PM_MenuPanelWidth:
        return 2;
    case PM_MenuHMargin:
        return 0;
    case PM_MenuVMargin:
        return 1;
    case PM_MenuBarPanelWidth:
    case PM_MenuBarVMargin:
    case PM_MenuBarHMargin:
        return 0;
    case PM_MenuBarItemSpacing:
        return 0;
    case PM_ToolTipLabelFrameWidth:
        return 1;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 1;
    case PM_ButtonDefaultIndicator:
        return 0;
    case PM_DefaultFrameWidth:
        return 2;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

int Style::styleHint(
    StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *data) const
{
    switch (hint) {
    case SH_EtchDisabledText:
        return 1;
    case SH_Widget_Animation_Duration:
        return 0;
    case SH_DialogButtonLayout:
        return QDialogButtonBox::WinLayout;
    case SH_DialogButtonBox_ButtonsHaveIcons:
        return 0;
    case SH_UnderlineShortcut:
        // NT 4.0 always underlines mnemonics.
        return 1;
    case SH_Menu_FadeOutOnHide:
    case SH_Menu_FlashTriggeredItem:
    case SH_Menu_SupportsSections:
        return 0;
    case SH_Menu_SubMenuPopupDelay:
        // NT's default MenuShowDelay.
        return 400;
    case SH_Menu_MouseTracking:
    case SH_MenuBar_MouseTracking:
        return 1;
    case SH_LineEdit_PasswordCharacter:
        // NT hides passwords behind asterisks.
        return '*';
    default:
        return QProxyStyle::styleHint(hint, option, widget, data);
    }
}

} // namespace Daytona
