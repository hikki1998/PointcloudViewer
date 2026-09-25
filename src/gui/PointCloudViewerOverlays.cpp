#include "gui/PointCloudViewerOverlays.h"

#include <QPainter>
#include <QPainterPath>

PolygonSelectionOverlay::PolygonSelectionOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    hide();
}

void PolygonSelectionOverlay::setPolygonState(
    const QPolygonF& polygon,
    bool hasPreviewPoint,
    const QPointF& previewPoint)
{
    polygon_ = polygon;
    hasPreviewPoint_ = hasPreviewPoint;
    previewPoint_ = previewPoint;

    if (polygon_.isEmpty()) {
        hide();
        return;
    }

    show();
    raise();
    update();
}

void PolygonSelectionOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    if (polygon_.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (polygon_.size() >= 3) {
        QPainterPath fillPath;
        fillPath.addPolygon(polygon_);
        painter.fillPath(fillPath, QColor(56, 189, 248, 36));
    }

    painter.setPen(QPen(QColor(56, 189, 248, 225), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (int index = 1; index < polygon_.size(); ++index) {
        painter.drawLine(polygon_.at(index - 1), polygon_.at(index));
    }
    if (polygon_.size() >= 3) {
        painter.drawLine(polygon_.constLast(), polygon_.constFirst());
    }

    if (hasPreviewPoint_) {
        painter.setPen(QPen(QColor(14, 165, 233, 210), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(polygon_.constLast(), previewPoint_);
    }

    painter.setPen(QPen(QColor(255, 255, 255, 210), 1.0));
    painter.setBrush(QColor(14, 165, 233, 220));
    for (const QPointF& point : polygon_) {
        painter.drawEllipse(point, 4.0, 4.0);
    }
}
