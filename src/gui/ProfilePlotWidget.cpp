#include "gui/ProfilePlotWidget.h"

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>

namespace
{
constexpr qreal kChartPaddingLeft = 58.0;
constexpr qreal kChartPaddingTop = 20.0;
constexpr qreal kChartPaddingRight = 18.0;
constexpr qreal kChartPaddingBottom = 34.0;
constexpr qreal kMinChartWidth = 120.0;
constexpr qreal kMinChartHeight = 90.0;

QString markerBadgeText(const ProjectedProfileMarker& marker)
{
    const QString prefix = marker.kind == ProfileMarkerKind::Tower
        ? QObject::tr("T%1").arg(marker.sourceIndex + 1)
        : QObject::tr("I%1").arg(marker.sourceIndex + 1);
    return marker.selected && !marker.title.trimmed().isEmpty()
        ? QObject::tr("%1 %2").arg(prefix, marker.title.trimmed())
        : prefix;
}
}

ProfilePlotWidget::ProfilePlotWidget(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setMinimumHeight(220);
}

void ProfilePlotWidget::setAnalysisResult(const ClearanceAnalysisResult& analysisResult)
{
    analysisResult_ = analysisResult;
    update();
}

void ProfilePlotWidget::setRuleEvaluation(const ClearanceRuleEvaluationResult& ruleEvaluation)
{
    ruleEvaluation_ = ruleEvaluation;
    update();
}

void ProfilePlotWidget::setProfileMarkers(const QList<ProjectedProfileMarker>& profileMarkers)
{
    profileMarkers_ = profileMarkers;
    update();
}

void ProfilePlotWidget::setSelectedSegmentIndex(int selectedSegmentIndex)
{
    if (selectedSegmentIndex_ == selectedSegmentIndex) {
        return;
    }

    selectedSegmentIndex_ = selectedSegmentIndex;
    update();
}

QSize ProfilePlotWidget::minimumSizeHint() const
{
    return QSize(360, 220);
}

QSize ProfilePlotWidget::sizeHint() const
{
    return QSize(640, 260);
}

void ProfilePlotWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(248, 250, 252));

    QRectF chartRect = plotRect();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(chartRect.adjusted(-10.0, -10.0, 10.0, 10.0), 14.0, 14.0);

    if (!analysisResult_.isValid()) {
        drawEmptyState(painter, chartRect);
        return;
    }

    drawGrid(painter, chartRect);
    drawAxes(painter, chartRect);
    drawProfile(painter, chartRect);
    drawMarkers(painter, chartRect);
    drawSummary(painter, chartRect);
}

QRectF ProfilePlotWidget::plotRect() const
{
    const QRectF widgetRect = rect();
    return QRectF(
        widgetRect.left() + kChartPaddingLeft,
        widgetRect.top() + kChartPaddingTop,
        std::max(kMinChartWidth, widgetRect.width() - kChartPaddingLeft - kChartPaddingRight),
        std::max(kMinChartHeight, widgetRect.height() - kChartPaddingTop - kChartPaddingBottom));
}

QPointF ProfilePlotWidget::mapToPlot(float chainage, float elevation, const QRectF& rect) const
{
    const float maxChainage = std::max(analysisResult_.totalHorizontalDistance, 0.001f);
    const float minElevation = analysisResult_.minimumElevation;
    const float maxElevation = std::max(analysisResult_.maximumElevation, minElevation + 0.001f);

    const qreal xRatio = static_cast<qreal>(chainage / maxChainage);
    const qreal yRatio = static_cast<qreal>((elevation - minElevation) / (maxElevation - minElevation));

    return QPointF(
        rect.left() + rect.width() * xRatio,
        rect.bottom() - rect.height() * yRatio);
}

void ProfilePlotWidget::drawEmptyState(QPainter& painter, const QRectF& rect) const
{
    painter.setPen(QColor(100, 116, 139));
    QFont titleFont = painter.font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(rect.adjusted(0.0, 20.0, 0.0, -20.0), Qt::AlignCenter, tr("Profile view is waiting for a measured path."));

    QFont detailFont = painter.font();
    detailFont.setBold(false);
    detailFont.setPointSizeF(std::max(9.0, detailFont.pointSizeF() - 1.0));
    painter.setFont(detailFont);
    painter.setPen(QColor(148, 163, 184));
    painter.drawText(
        rect.adjusted(36.0, 60.0, -36.0, -18.0),
        Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
        tr("Start measurement, click at least two points in the scene, and the span profile will appear here."));
}

void ProfilePlotWidget::drawAxes(QPainter& painter, const QRectF& rect) const
{
    painter.setPen(QPen(QColor(148, 163, 184), 1.2));
    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
    painter.drawLine(rect.bottomLeft(), rect.topLeft());

    painter.setPen(QColor(71, 85, 105));
    QFont axisFont = painter.font();
    axisFont.setPointSizeF(std::max(8.5, axisFont.pointSizeF() - 1.0));
    painter.setFont(axisFont);
    painter.drawText(
        QRectF(rect.left(), rect.bottom() + 8.0, rect.width(), 20.0),
        Qt::AlignCenter,
        tr("Horizontal chainage (m)"));
    painter.save();
    painter.translate(rect.left() - 38.0, rect.center().y());
    painter.rotate(-90.0);
    painter.drawText(QRectF(-rect.height() * 0.5, -14.0, rect.height(), 20.0), Qt::AlignCenter, tr("Elevation (m)"));
    painter.restore();
}

void ProfilePlotWidget::drawGrid(QPainter& painter, const QRectF& rect) const
{
    painter.setPen(QPen(QColor(226, 232, 240), 1.0));
    constexpr int verticalGridCount = 5;
    constexpr int horizontalGridCount = 4;
    for (int gridIndex = 0; gridIndex <= verticalGridCount; ++gridIndex) {
        const qreal ratio = static_cast<qreal>(gridIndex) / static_cast<qreal>(verticalGridCount);
        const qreal x = rect.left() + rect.width() * ratio;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    for (int gridIndex = 0; gridIndex <= horizontalGridCount; ++gridIndex) {
        const qreal ratio = static_cast<qreal>(gridIndex) / static_cast<qreal>(horizontalGridCount);
        const qreal y = rect.top() + rect.height() * ratio;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
}

void ProfilePlotWidget::drawProfile(QPainter& painter, const QRectF& rect) const
{
    if (analysisResult_.profilePoints.size() < 2) {
        return;
    }

    QPainterPath overallPath;
    overallPath.moveTo(mapToPlot(
        analysisResult_.profilePoints.constFirst().chainage,
        analysisResult_.profilePoints.constFirst().point.z,
        rect));

    for (int pointIndex = 1; pointIndex < analysisResult_.profilePoints.size(); ++pointIndex) {
        const ClearanceProfilePoint& profilePoint = analysisResult_.profilePoints.at(pointIndex);
        overallPath.lineTo(mapToPlot(profilePoint.chainage, profilePoint.point.z, rect));
    }

    painter.setPen(QPen(QColor(148, 163, 184), 2.0));
    painter.drawPath(overallPath);

    if (selectedSegmentIndex_ >= 0 && selectedSegmentIndex_ < analysisResult_.segments.size()) {
        const ClearanceSegment& selectedSegment = analysisResult_.segments.at(selectedSegmentIndex_);
        const ClearanceProfilePoint& selectedStart = analysisResult_.profilePoints.at(selectedSegment.startPointIndex);
        const ClearanceProfilePoint& selectedEnd = analysisResult_.profilePoints.at(selectedSegment.endPointIndex);
        painter.setPen(QPen(QColor(251, 191, 36, 160), 8.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(
            mapToPlot(selectedStart.chainage, selectedStart.point.z, rect),
            mapToPlot(selectedEnd.chainage, selectedEnd.point.z, rect));
    }

    for (int segmentIndex = 0; segmentIndex < analysisResult_.segments.size(); ++segmentIndex) {
        const ClearanceSegment& segment = analysisResult_.segments.at(segmentIndex);
        const ClearanceSegmentEvaluation evaluation = segmentIndex < ruleEvaluation_.segmentEvaluations.size()
            ? ruleEvaluation_.segmentEvaluations.at(segmentIndex)
            : ClearanceSegmentEvaluation();
        const ClearanceProfilePoint& startPoint = analysisResult_.profilePoints.at(segment.startPointIndex);
        const ClearanceProfilePoint& endPoint = analysisResult_.profilePoints.at(segment.endPointIndex);
        QColor segmentColor(37, 99, 235);
        qreal segmentWidth = 2.4;
        switch (evaluation.severity) {
        case AnalysisSeverity::Advisory:
            segmentColor = QColor(217, 119, 6);
            segmentWidth = 2.8;
            break;
        case AnalysisSeverity::Warning:
            segmentColor = QColor(220, 38, 38);
            segmentWidth = 3.0;
            break;
        case AnalysisSeverity::Critical:
            segmentColor = QColor(127, 29, 29);
            segmentWidth = 3.3;
            break;
        case AnalysisSeverity::None:
        default:
            segmentColor = segment.belowThreshold ? QColor(220, 38, 38) : QColor(37, 99, 235);
            break;
        }
        painter.setPen(QPen(segmentColor, segmentWidth, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(
            mapToPlot(startPoint.chainage, startPoint.point.z, rect),
            mapToPlot(endPoint.chainage, endPoint.point.z, rect));
    }

    for (const ClearanceProfilePoint& profilePoint : analysisResult_.profilePoints) {
        const QPointF anchor = mapToPlot(profilePoint.chainage, profilePoint.point.z, rect);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(15, 23, 42));
        painter.drawEllipse(anchor, 3.2, 3.2);
    }

    painter.setPen(QColor(71, 85, 105));
    QFont tickFont = painter.font();
    tickFont.setPointSizeF(std::max(8.0, tickFont.pointSizeF() - 1.0));
    painter.setFont(tickFont);

    const float maxChainage = std::max(analysisResult_.totalHorizontalDistance, 0.001f);
    const float minElevation = analysisResult_.minimumElevation;
    const float maxElevation = std::max(analysisResult_.maximumElevation, minElevation + 0.001f);

    constexpr int tickCount = 5;
    for (int tickIndex = 0; tickIndex <= tickCount; ++tickIndex) {
        const qreal ratio = static_cast<qreal>(tickIndex) / static_cast<qreal>(tickCount);
        const qreal x = rect.left() + rect.width() * ratio;
        painter.drawText(
            QRectF(x - 30.0, rect.bottom() + 6.0, 60.0, 18.0),
            Qt::AlignCenter,
            QString::number(maxChainage * ratio, 'f', 1));

        const qreal y = rect.bottom() - rect.height() * ratio;
        painter.drawText(
            QRectF(rect.left() - 52.0, y - 9.0, 46.0, 18.0),
            Qt::AlignRight | Qt::AlignVCenter,
            QString::number(minElevation + (maxElevation - minElevation) * ratio, 'f', 1));
    }
}

void ProfilePlotWidget::drawMarkers(QPainter& painter, const QRectF& rect) const
{
    if (profileMarkers_.isEmpty()) {
        return;
    }

    QFont badgeFont = painter.font();
    badgeFont.setPointSizeF(std::max(8.0, badgeFont.pointSizeF() - 1.0));
    badgeFont.setBold(true);
    painter.setFont(badgeFont);
    QFontMetrics metrics(badgeFont);

    QList<QRectF> occupiedRects;
    for (const ProjectedProfileMarker& marker : profileMarkers_) {
        const QPointF anchor = mapToPlot(marker.chainage, marker.elevation, rect);
        const QColor fillColor = marker.kind == ProfileMarkerKind::Tower
            ? (marker.selected ? QColor(180, 83, 9) : QColor(217, 119, 6))
            : (marker.selected ? QColor(185, 28, 28) : QColor(220, 38, 38));
        const QColor borderColor = marker.selected ? QColor(253, 224, 71) : QColor(255, 255, 255, 160);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(fillColor.red(), fillColor.green(), fillColor.blue(), marker.selected ? 150 : 90), marker.selected ? 1.4 : 1.0, Qt::DashLine));
        painter.drawLine(anchor, QPointF(anchor.x(), rect.bottom()));

        painter.setPen(QPen(borderColor, marker.selected ? 2.0 : 1.2));
        painter.setBrush(fillColor);
        if (marker.kind == ProfileMarkerKind::Tower) {
            QPolygonF diamond({
                QPointF(anchor.x(), anchor.y() - 7.0),
                QPointF(anchor.x() + 7.0, anchor.y()),
                QPointF(anchor.x(), anchor.y() + 7.0),
                QPointF(anchor.x() - 7.0, anchor.y())
            });
            painter.drawPolygon(diamond);
        } else {
            painter.drawEllipse(anchor, marker.selected ? 7.0 : 6.0, marker.selected ? 7.0 : 6.0);
        }
        painter.restore();

        const QString badgeText = markerBadgeText(marker);
        const int badgeWidth = std::max(36, metrics.horizontalAdvance(badgeText) + 18);
        const int badgeHeight = metrics.height() + 8;
        QRectF badgeRect(anchor.x() + 10.0, anchor.y() - badgeHeight - 10.0, badgeWidth, badgeHeight);
        int overlapAttempt = 0;
        while (overlapAttempt < 6) {
            bool hasOverlap = false;
            for (const QRectF& occupiedRect : occupiedRects) {
                if (occupiedRect.adjusted(-4.0, -3.0, 4.0, 3.0).intersects(badgeRect)) {
                    hasOverlap = true;
                    badgeRect.translate(0.0, -badgeHeight - 6.0);
                    ++overlapAttempt;
                    break;
                }
            }
            if (!hasOverlap) {
                break;
            }
        }
        occupiedRects.append(badgeRect);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, marker.selected ? 238 : 220));
        painter.drawRoundedRect(badgeRect, 10.0, 10.0);
        painter.setPen(QPen(fillColor, 1.1));
        painter.drawRoundedRect(badgeRect, 10.0, 10.0);
        painter.setPen(QColor(15, 23, 42));
        painter.drawText(badgeRect.adjusted(9.0, 0.0, -9.0, 0.0), Qt::AlignVCenter | Qt::AlignLeft, badgeText);
    }
}

void ProfilePlotWidget::drawSummary(QPainter& painter, const QRectF& rect) const
{
    QString badgeText;
    QColor badgeColor(22, 163, 74);
    if (!ruleEvaluation_.enabled()) {
        badgeText = tr("Threshold off");
        badgeColor = QColor(71, 85, 105);
    } else if (ruleEvaluation_.criticalCount > 0) {
        badgeText = tr("%1 critical segment(s)").arg(ruleEvaluation_.criticalCount);
        badgeColor = QColor(127, 29, 29);
    } else if (ruleEvaluation_.warningCount > 0) {
        badgeText = tr("%1 warning segment(s)").arg(ruleEvaluation_.warningCount);
        badgeColor = QColor(220, 38, 38);
    } else if (ruleEvaluation_.advisoryCount > 0) {
        badgeText = tr("%1 advisory segment(s)").arg(ruleEvaluation_.advisoryCount);
        badgeColor = QColor(217, 119, 6);
    } else {
        badgeText = tr("All segments outside risk bands");
    }

    const QRectF badgeRect(rect.right() - 190.0, rect.top() - 4.0, 182.0, 24.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(badgeColor);
    painter.drawRoundedRect(badgeRect, 12.0, 12.0);

    painter.setPen(Qt::white);
    QFont badgeFont = painter.font();
    badgeFont.setBold(true);
    badgeFont.setPointSizeF(std::max(8.5, badgeFont.pointSizeF() - 1.0));
    painter.setFont(badgeFont);
    painter.drawText(badgeRect, Qt::AlignCenter, badgeText);

    painter.setPen(QColor(71, 85, 105));
    QFont summaryFont = painter.font();
    summaryFont.setBold(false);
    painter.setFont(summaryFont);
    const QString summaryText = tr("Horizontal %1 m | 3D %2 m | dZ %3 m")
        .arg(QString::number(analysisResult_.totalHorizontalDistance, 'f', 2))
        .arg(QString::number(analysisResult_.totalDistance3d, 'f', 2))
        .arg(QString::number(analysisResult_.deltaZ, 'f', 2));
    painter.drawText(QRectF(rect.left(), rect.top() - 2.0, rect.width() - 204.0, 22.0), Qt::AlignLeft | Qt::AlignVCenter, summaryText);
}
