#pragma once

#include <QPointF>
#include <QPolygonF>
#include <QWidget>

class PolygonSelectionOverlay final : public QWidget
{
public:
    explicit PolygonSelectionOverlay(QWidget* parent = nullptr);

    void setPolygonState(const QPolygonF& polygon, bool hasPreviewPoint, const QPointF& previewPoint);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPolygonF polygon_;
    QPointF previewPoint_;
    bool hasPreviewPoint_ = false;
};
