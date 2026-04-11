#include "gui/support/RibbonIconFactory.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

namespace
{
const QColor kRibbonGlyphColor(28, 64, 111);
const QColor kRibbonAccentColor(59, 130, 246);
const QColor kDarkBackground(20, 28, 38);
const QColor kLightBackground(241, 244, 249);
}

namespace lasviewer::gui
{
QIcon createRibbonIcon(RibbonGlyph glyph)
{
    constexpr int iconSize = 48;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF canvas(2.0, 2.0, iconSize - 4.0, iconSize - 4.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(248, 250, 252));
    painter.drawRoundedRect(canvas, 12.0, 12.0);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(203, 213, 225), 1.2));
    painter.drawRoundedRect(canvas, 12.0, 12.0);

    QPen glyphPen(kRibbonGlyphColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(glyphPen);

    const QRectF r(11.0, 11.0, 26.0, 26.0);
    const auto drawTowerBase = [&painter, &r]() {
        painter.setPen(QPen(kRibbonGlyphColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 4.0), QPointF(r.center().x(), r.bottom() - 2.0));
        painter.drawLine(QPointF(r.center().x() - 7.0, r.top() + 10.0), QPointF(r.center().x() + 7.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.center().x() - 5.0, r.top() + 17.0), QPointF(r.center().x() + 5.0, r.top() + 17.0));
        painter.setBrush(QColor(249, 115, 22));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(r.center().x() - 5.0, r.bottom() - 9.0, 10.0, 10.0));
    };

    switch (glyph) {
    case RibbonGlyph::Open:
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 8.0, 20.0, 12.0), 3.0, 3.0);
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 8.0), QPointF(r.left() + 12.0, r.top() + 4.5));
        painter.drawLine(QPointF(r.left() + 12.0, r.top() + 4.5), QPointF(r.left() + 18.0, r.top() + 4.5));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 10.0), QPointF(r.center().x(), r.bottom() - 2.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom() - 2.0), QPointF(r.center().x() - 5.0, r.bottom() - 7.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom() - 2.0), QPointF(r.center().x() + 5.0, r.bottom() - 7.0));
        break;
    case RibbonGlyph::Clear:
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 10.0, 18.0, 12.0), 3.0, 3.0);
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 10.0), QPointF(r.left() + 12.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 12.0, r.top() + 5.0), QPointF(r.left() + 18.0, r.top() + 5.0));
        painter.setPen(QPen(QColor(220, 38, 38), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 21.0, r.top() + 9.0), QPointF(r.right(), r.bottom() - 1.0));
        painter.drawLine(QPointF(r.right(), r.top() + 9.0), QPointF(r.left() + 21.0, r.bottom() - 1.0));
        break;
    case RibbonGlyph::Exit:
        painter.drawRoundedRect(QRectF(r.left() + 4.0, r.top() + 4.0, 14.0, 18.0), 3.0, 3.0);
        painter.setPen(QPen(QColor(220, 38, 38), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 20.0, r.center().y()), QPointF(r.right(), r.center().y()));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 5.0, r.center().y() - 5.0));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 5.0, r.center().y() + 5.0));
        break;
    case RibbonGlyph::Fit:
        painter.drawRect(QRectF(r.left() + 4.0, r.top() + 4.0, 18.0, 18.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left(), r.top() + 8.0), QPointF(r.left() + 6.0, r.top() + 8.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.top()), QPointF(r.left() + 8.0, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 6.0, r.top()), QPointF(r.right() - 6.0, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 8.0, r.top() + 8.0), QPointF(r.right(), r.top() + 8.0));
        painter.drawLine(QPointF(r.left(), r.bottom() - 8.0), QPointF(r.left() + 6.0, r.bottom() - 8.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.bottom() - 6.0), QPointF(r.left() + 8.0, r.bottom()));
        painter.drawLine(QPointF(r.right() - 6.0, r.bottom() - 6.0), QPointF(r.right() - 6.0, r.bottom()));
        painter.drawLine(QPointF(r.right() - 8.0, r.bottom() - 8.0), QPointF(r.right(), r.bottom() - 8.0));
        break;
    case RibbonGlyph::Top:
        painter.drawEllipse(QRectF(r.left() + 6.0, r.top() + 3.0, 14.0, 6.0));
        painter.drawLine(QPointF(r.left() + 6.0, r.top() + 6.0), QPointF(r.left() + 6.0, r.bottom() - 2.0));
        painter.drawLine(QPointF(r.left() + 20.0, r.top() + 6.0), QPointF(r.left() + 20.0, r.bottom() - 2.0));
        painter.drawArc(QRectF(r.left() + 6.0, r.bottom() - 8.0, 14.0, 6.0), 180 * 16, 180 * 16);
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x(), r.top() + 10.0));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x() - 4.0, r.top() + 4.0));
        painter.drawLine(QPointF(r.center().x(), r.top()), QPointF(r.center().x() + 4.0, r.top() + 4.0));
        break;
    case RibbonGlyph::Front:
        painter.drawRect(QRectF(r.left() + 4.0, r.top() + 5.0, 18.0, 16.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x(), r.top() + 15.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x() - 4.0, r.bottom() - 4.0));
        painter.drawLine(QPointF(r.center().x(), r.bottom()), QPointF(r.center().x() + 4.0, r.bottom() - 4.0));
        break;
    case RibbonGlyph::Right:
        painter.drawRect(QRectF(r.left() + 5.0, r.top() + 5.0, 8.0, 16.0));
        painter.drawRect(QRectF(r.left() + 13.0, r.top() + 8.0, 8.0, 13.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.left() + 17.0, r.center().y()));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 4.0, r.center().y() - 4.0));
        painter.drawLine(QPointF(r.right(), r.center().y()), QPointF(r.right() - 4.0, r.center().y() + 4.0));
        break;
    case RibbonGlyph::Axes:
        painter.setPen(QPen(QColor(220, 38, 38), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.right() - 2.0, r.bottom() - 6.0));
        painter.setPen(QPen(QColor(22, 163, 74), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.left() + 6.0, r.top() + 2.0));
        painter.setPen(QPen(QColor(37, 99, 235), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 6.0, r.bottom() - 6.0), QPointF(r.right() - 6.0, r.top() + 6.0));
        break;
    case RibbonGlyph::Bounds:
        painter.drawRect(QRectF(r.left() + 5.0, r.top() + 8.0, 14.0, 14.0));
        painter.drawLine(QPointF(r.left() + 11.0, r.top() + 4.0), QPointF(r.right() - 1.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.right() - 1.0, r.top() + 10.0), QPointF(r.right() - 1.0, r.bottom() - 4.0));
        painter.drawLine(QPointF(r.left() + 19.0, r.top() + 8.0), QPointF(r.right() - 1.0, r.top() + 14.0));
        painter.drawLine(QPointF(r.left() + 19.0, r.bottom() - 2.0), QPointF(r.right() - 1.0, r.bottom() - 8.0));
        break;
    case RibbonGlyph::DarkBackground:
        painter.setBrush(kDarkBackground);
        painter.setPen(QPen(QColor(51, 65, 85), 1.2));
        painter.drawRoundedRect(QRectF(r.left() + 1.0, r.top() + 5.0, 24.0, 16.0), 6.0, 6.0);
        painter.setPen(QPen(QColor(248, 250, 252), 1.8));
        painter.setBrush(QColor(248, 250, 252));
        painter.drawEllipse(QRectF(r.left() + 6.0, r.top() + 9.0, 5.0, 5.0));
        painter.drawEllipse(QRectF(r.left() + 14.0, r.top() + 12.0, 4.0, 4.0));
        break;
    case RibbonGlyph::LightBackground:
        painter.setBrush(kLightBackground);
        painter.setPen(QPen(QColor(148, 163, 184), 1.2));
        painter.drawRoundedRect(QRectF(r.left() + 1.0, r.top() + 5.0, 24.0, 16.0), 6.0, 6.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(251, 191, 36));
        painter.drawEllipse(QRectF(r.left() + 17.0, r.top() + 7.0, 7.0, 7.0));
        painter.setBrush(QColor(148, 163, 184));
        painter.drawEllipse(QRectF(r.left() + 7.0, r.top() + 12.0, 5.0, 5.0));
        break;
    case RibbonGlyph::Rgb:
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(239, 68, 68));
        painter.drawEllipse(QRectF(r.left() + 3.0, r.top() + 9.0, 8.0, 8.0));
        painter.setBrush(QColor(34, 197, 94));
        painter.drawEllipse(QRectF(r.left() + 11.0, r.top() + 9.0, 8.0, 8.0));
        painter.setBrush(QColor(59, 130, 246));
        painter.drawEllipse(QRectF(r.left() + 7.0, r.top() + 16.0, 8.0, 8.0));
        break;
    case RibbonGlyph::Elevation: {
        painter.setPen(QPen(QColor(148, 163, 184), 2.0));
        painter.drawLine(QPointF(r.left() + 2.0, r.bottom() - 3.0), QPointF(r.right() - 2.0, r.bottom() - 3.0));
        QLinearGradient gradient(QPointF(r.left(), r.bottom()), QPointF(r.right(), r.top()));
        gradient.setColorAt(0.0, QColor(37, 99, 235));
        gradient.setColorAt(0.5, QColor(16, 185, 129));
        gradient.setColorAt(1.0, QColor(249, 115, 22));
        painter.setPen(QPen(QBrush(gradient), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF({
            QPointF(r.left() + 2.0, r.bottom() - 6.0),
            QPointF(r.left() + 8.0, r.top() + 14.0),
            QPointF(r.left() + 15.0, r.top() + 8.0),
            QPointF(r.right() - 1.0, r.top() + 2.0)
        }));
        break;
    }
    case RibbonGlyph::SingleColor:
        painter.setPen(QPen(kRibbonGlyphColor, 2.2));
        painter.drawLine(QPointF(r.left() + 5.0, r.bottom() - 4.0), QPointF(r.right() - 6.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 5.0), QPointF(r.right() - 3.0, r.bottom() - 4.0));
        painter.setBrush(QColor(53, 142, 255));
        painter.setPen(QPen(QColor(37, 99, 235), 1.2));
        painter.drawEllipse(QRectF(r.left() + 9.0, r.top() + 9.0, 8.0, 8.0));
        break;
    case RibbonGlyph::Classification:
        painter.setPen(QPen(QColor(148, 163, 184), 1.8));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 4.0, 20.0, 18.0), 4.0, 4.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(190, 242, 100));
        painter.drawRoundedRect(QRectF(r.left() + 5.0, r.top() + 6.0, 6.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(22, 163, 74));
        painter.drawRoundedRect(QRectF(r.left() + 12.0, r.top() + 6.0, 9.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(251, 146, 60));
        painter.drawRoundedRect(QRectF(r.left() + 5.0, r.top() + 12.0, 8.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(14, 165, 233));
        painter.drawRoundedRect(QRectF(r.left() + 14.0, r.top() + 12.0, 7.0, 5.0), 1.5, 1.5);
        painter.setBrush(QColor(168, 85, 247));
        painter.drawRoundedRect(QRectF(r.left() + 5.0, r.top() + 18.0, 16.0, 3.5), 1.2, 1.2);
        break;
    case RibbonGlyph::ThemeColorful:
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(59, 130, 246));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 5.0, 22.0, 5.0), 2.5, 2.5);
        painter.setBrush(QColor(244, 114, 182));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 13.0, 15.0, 5.0), 2.5, 2.5);
        painter.setBrush(QColor(16, 185, 129));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 21.0, 19.0, 5.0), 2.5, 2.5);
        break;
    case RibbonGlyph::ThemeWhite:
        painter.setPen(QPen(QColor(148, 163, 184), 1.8));
        painter.setBrush(QColor(255, 255, 255));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 4.0, 22.0, 18.0), 5.0, 5.0);
        painter.setPen(QPen(QColor(59, 130, 246), 2.5));
        painter.drawLine(QPointF(r.left() + 4.0, r.top() + 9.0), QPointF(r.right() - 4.0, r.top() + 9.0));
        break;
    case RibbonGlyph::ThemeDarkGray:
        painter.setPen(QPen(QColor(71, 85, 105), 1.8));
        painter.setBrush(QColor(51, 65, 85));
        painter.drawRoundedRect(QRectF(r.left() + 2.0, r.top() + 4.0, 22.0, 18.0), 5.0, 5.0);
        painter.setPen(QPen(QColor(148, 163, 184), 2.5));
        painter.drawLine(QPointF(r.left() + 4.0, r.top() + 9.0), QPointF(r.right() - 4.0, r.top() + 9.0));
        break;
    case RibbonGlyph::Log:
        painter.drawRoundedRect(QRectF(r.left() + 3.0, r.top() + 4.0, 20.0, 18.0), 4.0, 4.0);
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 9.0), QPointF(r.right() - 3.0, r.top() + 9.0));
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 14.0), QPointF(r.right() - 6.0, r.top() + 14.0));
        painter.drawLine(QPointF(r.left() + 7.0, r.top() + 19.0), QPointF(r.right() - 9.0, r.top() + 19.0));
        painter.setBrush(kRibbonAccentColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 7.0, 2.8, 2.8));
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 12.0, 2.8, 2.8));
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 17.0, 2.8, 2.8));
        break;
    case RibbonGlyph::Measure:
        painter.drawEllipse(QRectF(r.left() + 3.0, r.top() + 7.0, 6.0, 6.0));
        painter.drawEllipse(QRectF(r.right() - 9.0, r.bottom() - 9.0, 6.0, 6.0));
        painter.setPen(QPen(kRibbonAccentColor, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 8.0, r.top() + 12.0), QPointF(r.right() - 6.0, r.bottom() - 6.0));
        break;
    case RibbonGlyph::Tower:
        drawTowerBase();
        break;
    case RibbonGlyph::TowerAdd:
        drawTowerBase();
        painter.setPen(QPen(QColor(22, 163, 74), 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.right() - 3.0, r.top() + 2.0), QPointF(r.right() - 3.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.right() - 7.0, r.top() + 6.0), QPointF(r.right() + 1.0, r.top() + 6.0));
        break;
    case RibbonGlyph::TowerInsert:
        drawTowerBase();
        painter.setPen(QPen(QColor(37, 99, 235), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 5.0), QPointF(r.left() + 9.0, r.top() + 5.0));
        painter.drawLine(QPointF(r.left() + 9.0, r.top() + 5.0), QPointF(r.left() + 6.0, r.top() + 2.5));
        painter.drawLine(QPointF(r.left() + 9.0, r.top() + 5.0), QPointF(r.left() + 6.0, r.top() + 7.5));
        break;
    case RibbonGlyph::TowerMove:
        drawTowerBase();
        painter.setPen(QPen(QColor(37, 99, 235), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.center().x(), r.top() + 1.0), QPointF(r.center().x(), r.top() + 8.0));
        painter.drawLine(QPointF(r.center().x(), r.top() + 1.0), QPointF(r.center().x() - 2.5, r.top() + 3.5));
        painter.drawLine(QPointF(r.center().x(), r.top() + 1.0), QPointF(r.center().x() + 2.5, r.top() + 3.5));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 12.0), QPointF(r.left() + 8.0, r.top() + 12.0));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 12.0), QPointF(r.left() + 4.5, r.top() + 9.5));
        painter.drawLine(QPointF(r.left() + 2.0, r.top() + 12.0), QPointF(r.left() + 4.5, r.top() + 14.5));
        break;
    case RibbonGlyph::TowerAdjust:
        drawTowerBase();
        painter.setPen(QPen(QColor(2, 132, 199), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(QRectF(r.right() - 11.0, r.top() + 1.5, 8.0, 8.0));
        painter.drawLine(QPointF(r.right() - 7.0, r.top() + 1.5), QPointF(r.right() - 7.0, r.top() - 1.0));
        painter.drawLine(QPointF(r.right() - 7.0, r.top() + 9.5), QPointF(r.right() - 7.0, r.top() + 12.0));
        painter.drawLine(QPointF(r.right() - 11.0, r.top() + 5.5), QPointF(r.right() - 13.5, r.top() + 5.5));
        painter.drawLine(QPointF(r.right() - 3.0, r.top() + 5.5), QPointF(r.right() - 0.5, r.top() + 5.5));
        break;
    case RibbonGlyph::TowerFocus:
        drawTowerBase();
        painter.setPen(QPen(QColor(37, 99, 235), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.left() + 1.5, r.top() + 1.5), QPointF(r.left() + 6.0, r.top() + 1.5));
        painter.drawLine(QPointF(r.left() + 1.5, r.top() + 1.5), QPointF(r.left() + 1.5, r.top() + 6.0));
        painter.drawLine(QPointF(r.right() - 1.5, r.top() + 1.5), QPointF(r.right() - 6.0, r.top() + 1.5));
        painter.drawLine(QPointF(r.right() - 1.5, r.top() + 1.5), QPointF(r.right() - 1.5, r.top() + 6.0));
        painter.drawLine(QPointF(r.left() + 1.5, r.bottom() - 1.5), QPointF(r.left() + 6.0, r.bottom() - 1.5));
        painter.drawLine(QPointF(r.left() + 1.5, r.bottom() - 1.5), QPointF(r.left() + 1.5, r.bottom() - 6.0));
        painter.drawLine(QPointF(r.right() - 1.5, r.bottom() - 1.5), QPointF(r.right() - 6.0, r.bottom() - 1.5));
        painter.drawLine(QPointF(r.right() - 1.5, r.bottom() - 1.5), QPointF(r.right() - 1.5, r.bottom() - 6.0));
        break;
    case RibbonGlyph::TowerRemove:
        drawTowerBase();
        painter.setPen(QPen(QColor(220, 38, 38), 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(r.right() - 9.0, r.top() + 2.0), QPointF(r.right() - 1.0, r.top() + 10.0));
        painter.drawLine(QPointF(r.right() - 1.0, r.top() + 2.0), QPointF(r.right() - 9.0, r.top() + 10.0));
        break;
    case RibbonGlyph::Language:
        painter.drawEllipse(QRectF(r.left() + 4.0, r.top() + 4.0, 18.0, 18.0));
        painter.drawLine(QPointF(r.center().x(), r.top() + 4.0), QPointF(r.center().x(), r.bottom() + 4.0));
        painter.drawLine(QPointF(r.left() + 4.0, r.center().y()), QPointF(r.right() + 4.0, r.center().y()));
        painter.drawArc(QRectF(r.left() + 7.0, r.top() + 4.0, 12.0, 18.0), 90 * 16, 180 * 16);
        painter.drawArc(QRectF(r.left() + 7.0, r.top() + 4.0, 12.0, 18.0), 270 * 16, 180 * 16);
        break;
    }

    return QIcon(pixmap);
}

QIcon createResourceIconOrFallback(const QString& resourcePath, RibbonGlyph fallbackGlyph)
{
    const QIcon resourceIcon(resourcePath);
    return resourceIcon.isNull() ? createRibbonIcon(fallbackGlyph) : resourceIcon;
}

QIcon createWindowControlIcon(WindowControlGlyph glyph, const QColor& color)
{
    constexpr int iconSize = 12;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPen pen(color, glyph == WindowControlGlyph::Close ? 1.8 : 1.4, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
    painter.setPen(pen);

    switch (glyph) {
    case WindowControlGlyph::Minimize:
        painter.drawLine(QPointF(2.0, 8.0), QPointF(10.0, 8.0));
        break;
    case WindowControlGlyph::Maximize:
        painter.drawRect(QRectF(2.0, 2.0, 8.0, 8.0));
        break;
    case WindowControlGlyph::Restore:
        painter.drawRect(QRectF(4.0, 2.0, 6.0, 6.0));
        painter.drawLine(QPointF(4.0, 4.0), QPointF(2.0, 4.0));
        painter.drawLine(QPointF(2.0, 4.0), QPointF(2.0, 10.0));
        painter.drawLine(QPointF(2.0, 10.0), QPointF(8.0, 10.0));
        break;
    case WindowControlGlyph::Close:
        painter.drawLine(QPointF(2.5, 2.5), QPointF(9.5, 9.5));
        painter.drawLine(QPointF(9.5, 2.5), QPointF(2.5, 9.5));
        break;
    }

    return QIcon(pixmap);
}
}
