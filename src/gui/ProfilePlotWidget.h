#pragma once

#include <QRectF>
#include <QWidget>

#include "domain/ClearanceAnalysis.h"
#include "domain/ProfileMarkerProjection.h"

class QPaintEvent;
class QPainter;

class ProfilePlotWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePlotWidget(QWidget* parent = nullptr);

    void setAnalysisResult(const ClearanceAnalysisResult& analysisResult);
    void setProfileMarkers(const QList<ProjectedProfileMarker>& profileMarkers);
    void setSelectedSegmentIndex(int selectedSegmentIndex);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QRectF plotRect() const;
    QPointF mapToPlot(float chainage, float elevation, const QRectF& rect) const;
    void drawEmptyState(QPainter& painter, const QRectF& rect) const;
    void drawAxes(QPainter& painter, const QRectF& rect) const;
    void drawGrid(QPainter& painter, const QRectF& rect) const;
    void drawProfile(QPainter& painter, const QRectF& rect) const;
    void drawMarkers(QPainter& painter, const QRectF& rect) const;
    void drawSummary(QPainter& painter, const QRectF& rect) const;

    ClearanceAnalysisResult analysisResult_;
    QList<ProjectedProfileMarker> profileMarkers_;
    int selectedSegmentIndex_ = -1;
};
