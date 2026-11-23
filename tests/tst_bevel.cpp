// SPDX-License-Identifier: GPL-3.0-or-later

#include "bevel.h"
#include "daytonastyle.h"
#include "glyphs.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QImage>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTest>

using namespace Daytona;

namespace {

// Draws with fn onto a 10x8 logical image at the given scale, filled first
// with magenta so untouched pixels are obvious.
template <typename F> QImage draw(qreal scale, F fn)
{
    QImage image(QSize(10, 8) * scale, QImage::Format_RGB32);
    image.setDevicePixelRatio(scale);
    image.fill(Qt::magenta);
    QPainter painter(&image);
    fn(&painter, QRect(0, 0, 10, 8));
    return image;
}

QColor at(const QImage &image, int logicalX, int logicalY, int offset = 0)
{
    const qreal s = image.devicePixelRatio();
    return image.pixelColor(int(logicalX * s) + offset, int(logicalY * s) + offset);
}

} // namespace

class TestBevel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void raisedEdges_data()
    {
        QTest::addColumn<qreal>("scale");
        QTest::newRow("1x") << 1.0;
        QTest::newRow("2x") << 2.0;
    }

    void raisedEdges()
    {
        QFETCH(qreal, scale);
        const QPalette pal = Style::ntPalette();
        const QImage img = draw(scale, [&](QPainter *p, QRect r) { Bevel::raised(p, r, pal); });
        QCOMPARE(at(img, 0, 0), pal.color(QPalette::Light)); // outer top-left
        QCOMPARE(at(img, 9, 7), pal.color(QPalette::Shadow)); // outer bottom-right
        QCOMPARE(at(img, 9, 0), pal.color(QPalette::Shadow)); // top-right corner is dark
        QCOMPARE(at(img, 0, 7), pal.color(QPalette::Shadow)); // bottom-left corner is dark
        QCOMPARE(at(img, 1, 1), pal.color(QPalette::Midlight)); // inner top-left
        QCOMPARE(at(img, 8, 6), pal.color(QPalette::Dark)); // inner bottom-right
        QCOMPARE(at(img, 4, 4), QColor(Qt::magenta)); // interior untouched
        if (scale == 2.0) {
            // A 1 logical pixel edge fills both device pixels: crisp, no blend.
            QCOMPARE(at(img, 0, 3, 1), pal.color(QPalette::Light));
            QCOMPARE(at(img, 9, 3, 1), pal.color(QPalette::Shadow));
        }
    }

    void pressedIsInvertedRaised()
    {
        const QPalette pal = Style::ntPalette();
        const QImage img = draw(1, [&](QPainter *p, QRect r) { Bevel::pressed(p, r, pal); });
        QCOMPARE(at(img, 0, 0), pal.color(QPalette::Shadow));
        QCOMPARE(at(img, 9, 7), pal.color(QPalette::Light));
        QCOMPARE(at(img, 1, 1), pal.color(QPalette::Dark));
        QCOMPARE(at(img, 8, 6), pal.color(QPalette::Midlight));
    }

    void sunkenField()
    {
        const QPalette pal = Style::ntPalette();
        const QImage img = draw(1, [&](QPainter *p, QRect r) { Bevel::sunken(p, r, pal); });
        QCOMPARE(at(img, 0, 0), pal.color(QPalette::Dark));
        QCOMPARE(at(img, 9, 7), pal.color(QPalette::Light));
        QCOMPARE(at(img, 1, 1), pal.color(QPalette::Shadow));
        QCOMPARE(at(img, 8, 6), pal.color(QPalette::Midlight));
    }

    void thinEdges()
    {
        const QPalette pal = Style::ntPalette();
        const QImage up = draw(1, [&](QPainter *p, QRect r) { Bevel::raisedThin(p, r, pal); });
        QCOMPARE(at(up, 0, 0), pal.color(QPalette::Light));
        QCOMPARE(at(up, 9, 7), pal.color(QPalette::Dark));
        QCOMPARE(at(up, 1, 1), QColor(Qt::magenta));
        const QImage down = draw(1, [&](QPainter *p, QRect r) { Bevel::sunkenThin(p, r, pal); });
        QCOMPARE(at(down, 0, 0), pal.color(QPalette::Dark));
        QCOMPARE(at(down, 9, 7), pal.color(QPalette::Light));
    }

    void focusRectIsDotted()
    {
        const QImage img = draw(1, [](QPainter *p, QRect r) { Bevel::focusRect(p, r, Qt::black); });
        // Pixels with even x + y are dots; the others are untouched.
        QCOMPARE(at(img, 0, 0), QColor(Qt::black));
        QCOMPARE(at(img, 1, 0), QColor(Qt::magenta));
        QCOMPARE(at(img, 2, 0), QColor(Qt::black));
        QCOMPARE(at(img, 0, 1), QColor(Qt::magenta));
        QCOMPARE(at(img, 0, 2), QColor(Qt::black));
        QCOMPARE(at(img, 9, 7), QColor(Qt::black));
        QCOMPARE(at(img, 4, 4), QColor(Qt::magenta));
    }

    void styleDrawsFromTheGivenPalette()
    {
        // A scheme change only swaps the palette; the style must pick it up.
        QPalette custom = Style::ntPalette();
        custom.setColor(QPalette::Button, QColor(0x10, 0x20, 0x30));
        custom.setColor(QPalette::Light, QColor(0xaa, 0xbb, 0xcc));
        custom.setColor(QPalette::Shadow, QColor(0x01, 0x02, 0x03));

        Style style;
        QStyleOptionButton option;
        option.state = QStyle::State_Enabled | QStyle::State_Raised;
        option.palette = custom;
        const QImage img = draw(1, [&](QPainter *p, QRect r) {
            option.rect = r;
            style.drawControl(QStyle::CE_PushButtonBevel, &option, p);
        });
        QCOMPARE(at(img, 0, 0), custom.color(QPalette::Light));
        QCOMPARE(at(img, 9, 7), custom.color(QPalette::Shadow));
        QCOMPARE(at(img, 4, 4), custom.color(QPalette::Button));
    }

    void defaultButtonHasBlackOutline()
    {
        const QPalette pal = Style::ntPalette();
        Style style;
        QStyleOptionButton option;
        option.state = QStyle::State_Enabled | QStyle::State_Raised;
        option.features = QStyleOptionButton::DefaultButton;
        option.palette = pal;
        const QImage img = draw(1, [&](QPainter *p, QRect r) {
            option.rect = r;
            style.drawControl(QStyle::CE_PushButtonBevel, &option, p);
        });
        QCOMPARE(at(img, 0, 0), pal.color(QPalette::Shadow)); // outline
        QCOMPARE(at(img, 1, 1), pal.color(QPalette::Light)); // then the raised bevel
    }

    void glyphSizes()
    {
        QCOMPARE(Glyph::size(Glyph::Kind::Check), QSize(7, 7));
        QCOMPARE(Glyph::size(Glyph::Kind::Bullet), QSize(6, 6));
        QCOMPARE(Glyph::size(Glyph::Kind::ArrowRight), QSize(4, 7));
    }

    void checkGlyphPixels()
    {
        const QImage img = draw(2, [](QPainter *p, QRect) {
            Glyph::draw(p, Glyph::Kind::Check, QPoint(1, 0), Qt::black);
        });
        QCOMPARE(at(img, 7, 0), QColor(Qt::black)); // top-right tip of the tick
        QCOMPARE(at(img, 7, 0, 1), QColor(Qt::black)); // whole 2x2 device block
        QCOMPARE(at(img, 1, 0), QColor(Qt::magenta)); // top-left is clear
        QCOMPARE(at(img, 3, 6), QColor(Qt::black)); // bottom of the tick
    }

    void selectedMenuItemIsHighlighted()
    {
        const QPalette pal = Style::ntPalette();
        Style style;
        QStyleOptionMenuItem item;
        item.palette = pal;
        item.menuItemType = QStyleOptionMenuItem::Normal;
        item.maxIconWidth = 16;
        item.state = QStyle::State_Enabled | QStyle::State_Selected;
        const QImage selected = draw(1, [&](QPainter *p, QRect r) {
            item.rect = r;
            style.drawControl(QStyle::CE_MenuItem, &item, p);
        });
        QCOMPARE(
            at(selected, 0, 0), pal.color(QPalette::Highlight)); // full width, incl. check column
        QCOMPARE(at(selected, 9, 7), pal.color(QPalette::Highlight));
        item.state = QStyle::State_Enabled;
        const QImage normal = draw(1, [&](QPainter *p, QRect r) {
            item.rect = r;
            style.drawControl(QStyle::CE_MenuItem, &item, p);
        });
        QCOMPARE(at(normal, 0, 0), pal.color(QPalette::Window));
    }

    void tooltipPanel()
    {
        const QPalette pal = Style::ntPalette();
        Style style;
        QStyleOptionFrame frame;
        frame.palette = pal;
        const QImage img = draw(1, [&](QPainter *p, QRect r) {
            frame.rect = r;
            style.drawPrimitive(QStyle::PE_PanelTipLabel, &frame, p);
        });
        QCOMPARE(at(img, 0, 0), pal.color(QPalette::ToolTipText));
        QCOMPARE(at(img, 9, 7), pal.color(QPalette::ToolTipText));
        QCOMPARE(at(img, 4, 4), pal.color(QPalette::ToolTipBase));
    }

    void menuIconSizeProperty()
    {
        Style style;
        QMenu menu;
        QCOMPARE(style.pixelMetric(QStyle::PM_SmallIconSize, nullptr, &menu), 16);
        menu.setProperty(Style::MenuIconSizeProperty, 32);
        QCOMPARE(style.pixelMetric(QStyle::PM_SmallIconSize, nullptr, &menu), 32);
    }

    void effectsAreDisabled()
    {
        Style style;
        QApplication::setEffectEnabled(Qt::UI_AnimateMenu, true);
        QApplication::setEffectEnabled(Qt::UI_FadeTooltip, true);
        style.polish(qApp);
        QVERIFY(!QApplication::isEffectEnabled(Qt::UI_AnimateMenu));
        QVERIFY(!QApplication::isEffectEnabled(Qt::UI_FadeMenu));
        QVERIFY(!QApplication::isEffectEnabled(Qt::UI_FadeTooltip));
        QVERIFY(!QApplication::isEffectEnabled(Qt::UI_AnimateTooltip));
        QCOMPARE(style.styleHint(QStyle::SH_Menu_FadeOutOnHide), 0);
        QCOMPARE(style.styleHint(QStyle::SH_UnderlineShortcut), 1);
    }

    void checkBoxStates()
    {
        const QPalette pal = Style::ntPalette();
        Style style;
        QStyleOptionButton box;
        box.palette = pal;
        auto render = [&](QStyle::State state) {
            box.state = state;
            QImage img(13, 13, QImage::Format_RGB32);
            img.fill(Qt::magenta);
            QPainter p(&img);
            box.rect = img.rect();
            style.drawPrimitive(QStyle::PE_IndicatorCheckBox, &box, &p);
            return img;
        };
        const QImage off = render(QStyle::State_Enabled | QStyle::State_Off);
        QCOMPARE(off.pixelColor(0, 0), pal.color(QPalette::Dark)); // sunken edge
        QCOMPARE(off.pixelColor(6, 6), pal.color(QPalette::Base)); // white field
        const QImage on = render(QStyle::State_Enabled | QStyle::State_On);
        QCOMPARE(on.pixelColor(6, 6), pal.color(QPalette::Text)); // check crosses the centre
        const QImage disabled = render(QStyle::State_On);
        QCOMPARE(disabled.pixelColor(2, 10), pal.color(QPalette::Button)); // grey field
        QCOMPARE(disabled.pixelColor(6, 6), pal.color(QPalette::Dark)); // grey check
    }

    void radioButton()
    {
        const QPalette pal = Style::ntPalette();
        Style style;
        QStyleOptionButton radio;
        radio.palette = pal;
        radio.state = QStyle::State_Enabled | QStyle::State_On;
        QImage img(Glyph::RadioSize, Glyph::RadioSize, QImage::Format_RGB32);
        img.fill(Qt::magenta);
        QPainter p(&img);
        radio.rect = img.rect();
        style.drawPrimitive(QStyle::PE_IndicatorRadioButton, &radio, &p);
        p.end();
        QCOMPARE(img.pixelColor(0, 0), QColor(Qt::magenta)); // outside the circle
        QCOMPARE(img.pixelColor(5, 0), pal.color(QPalette::Dark)); // outer ring, top
        QCOMPARE(img.pixelColor(5, 11), pal.color(QPalette::Light)); // outer ring, bottom
        QCOMPARE(img.pixelColor(5, 1), pal.color(QPalette::Shadow)); // inner ring, top
        QCOMPARE(img.pixelColor(5, 5), pal.color(QPalette::Text)); // dot
        QCOMPARE(img.pixelColor(3, 5), pal.color(QPalette::Base)); // field
    }

    void styledPanelIsTwoPixelClientEdge()
    {
        const QPalette pal = Style::ntPalette();
        Style style;
        QStyleOptionFrame frame;
        frame.palette = pal;
        frame.frameShape = QFrame::StyledPanel;
        frame.state = QStyle::State_Enabled | QStyle::State_Sunken;
        const QImage img = draw(1, [&](QPainter *p, QRect r) {
            frame.rect = r;
            style.drawControl(QStyle::CE_ShapedFrame, &frame, p);
        });
        QCOMPARE(at(img, 0, 0), pal.color(QPalette::Dark));
        QCOMPARE(at(img, 1, 1), pal.color(QPalette::Shadow));
        QCOMPARE(at(img, 9, 7), pal.color(QPalette::Light));
    }

    void dialogButtons()
    {
        Style style;
        QCOMPARE(style.styleHint(QStyle::SH_DialogButtonLayout), int(QDialogButtonBox::WinLayout));
        QCOMPARE(style.styleHint(QStyle::SH_DialogButtonBox_ButtonsHaveIcons), 0);

        QDialogButtonBox box(QDialogButtonBox::Ok);
        box.setStyle(&style);
        QPushButton *ok = box.button(QDialogButtonBox::Ok);
        ok->setStyle(&style);
        const QSize hint = ok->sizeHint();
        QVERIFY2(hint.width() >= 75 && hint.height() >= 23,
            qPrintable(QStringLiteral("%1x%2").arg(hint.width()).arg(hint.height())));
    }

    void emptyRectsDrawNothing()
    {
        const QPalette pal = Style::ntPalette();
        const QImage img = draw(1, [&](QPainter *p, QRect) {
            Bevel::raised(p, QRect(2, 2, 0, 5), pal);
            Bevel::focusRect(p, QRect(2, 2, 5, 0), Qt::black);
        });
        QCOMPARE(at(img, 2, 2), QColor(Qt::magenta));
    }
};

QTEST_MAIN(TestBevel)
#include "tst_bevel.moc"
