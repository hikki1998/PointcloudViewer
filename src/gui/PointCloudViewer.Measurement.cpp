#include "gui/PointCloudViewer.h"

#include <QLabel>
#include <QLocale>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include <osg/Array>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LineWidth>
#include <osg/MatrixTransform>
#include <osg/Point>
#include <osg/StateSet>

namespace
{
constexpr int kMeasurementOverlayRenderBin = 100;

osg::Vec3 toOverlayLocalVec3(const PointRecord& point, const osg::Vec3d& origin)
{
    return osg::Vec3(point.x - origin.x(), point.y - origin.y(), point.z - origin.z());
}

osg::ref_ptr<osg::Node> wrapOverlayNodeWithSceneOrigin(osg::Node* node, const osg::Vec3d& origin)
{
    if (node == nullptr) return nullptr;
    osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform();
    transform->setMatrix(osg::Matrixd::translate(origin));
    transform->addChild(node);
    return transform;
}

void applyMeasurementForegroundState(osg::StateSet* stateSet)
{
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setAttributeAndModes(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false), osg::StateAttribute::ON);
    stateSet->setRenderBinDetails(kMeasurementOverlayRenderBin, "RenderBin");
}

osg::ref_ptr<osg::Geode> buildMeasurementGeode(const MeasurementResult& result, const osg::Vec3d& origin, bool line)
{
    if (result.points.isEmpty() || (line && result.points.size() < 2)) return nullptr;
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    for (int i = 0; i < result.points.size(); ++i) {
        vertices->push_back(toOverlayLocalVec3(result.points.at(i), origin));
        colors->push_back(i == 0 ? osg::Vec4(1.0f, 0.78f, 0.20f, 1.0f)
            : (i == result.points.size() - 1 ? osg::Vec4(0.20f, 0.83f, 0.96f, 1.0f)
                                             : osg::Vec4(0.96f, 0.93f, 0.55f, 1.0f)));
    }
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(line ? GL_LINE_STRIP : GL_POINTS, 0, static_cast<GLsizei>(vertices->size())));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());
    osg::StateSet* state = geode->getOrCreateStateSet();
    state->setAttributeAndModes(line ? static_cast<osg::StateAttribute*>(new osg::LineWidth(3.0f))
                                     : static_cast<osg::StateAttribute*>(new osg::Point(10.0f)), osg::StateAttribute::ON);
    applyMeasurementForegroundState(state);
    return geode;
}

QString formatCoordinate(float value)
{
    return QLocale().toString(static_cast<double>(value), 'f', 2);
}
}

bool PointCloudViewer::measurementEnabled() const
{
    return measurementEnabled_;
}

const MeasurementResult& PointCloudViewer::measurementResult() const
{
    return measurementResult_;
}

void PointCloudViewer::setMeasurementEnabled(bool enabled)
{
    if (enabled && !hasPointCloud()) {
        emit measurementMessage(tr("Load a point cloud before starting measurement."), true);
        return;
    }

    if (measurementEnabled_ == enabled) {
        return;
    }

    if (enabled && towerEditMode_ != TowerEditMode::None) {
        towerEditMode_ = TowerEditMode::None;
        towerEditTargetIndex_ = -1;
        emit towerEditModeChanged();
    }

    if (enabled && profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }

    measurementEnabled_ = enabled;
    updateSceneClickCapture();

    if (!measurementEnabled_) {
        resetMeasurementState();
        emit measurementMessage(tr("Measurement mode disabled."), false);
    } else {
        resetMeasurementState();
        emit measurementMessage(tr("Measurement mode enabled. Click points to measure, and right-click to undo the last point."), false);
    }

    updateFooter();
    emit measurementModeChanged();
}

void PointCloudViewer::clearMeasurement()
{
    const bool hadMeasurement = measurementResult_.hasStartPoint || measurementResult_.hasEndPoint;
    resetMeasurementState();
    if (hadMeasurement) {
        emit measurementMessage(tr("Measurement cleared."), false);
    }
    updateFooter();
}

void PointCloudViewer::createMeasurementOverlayWidgets()
{
    auto configureOverlayLabel = [this](QLabel*& label, const QString& objectName, const QString& extraStyle = QString()) {
        label = new QLabel(osgWidget_);
        label->setObjectName(objectName);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: rgba(15, 23, 42, 220);"
            "color: #f8fafc;"
            "border: 1px solid rgba(148, 163, 184, 180);"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}")
            + extraStyle);
        label->hide();
        label->raise();
    };

    configureOverlayLabel(measurementStartOverlayLabel_, QStringLiteral("measurementStartOverlayLabel"));
    configureOverlayLabel(measurementEndOverlayLabel_, QStringLiteral("measurementEndOverlayLabel"));
    configureOverlayLabel(
        measurementSummaryOverlayLabel_,
        QStringLiteral("measurementSummaryOverlayLabel"),
        QStringLiteral(
            "QLabel {"
            "font-size: 13px;"
            "padding: 6px 10px;"
            "}"));
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildMeasurementOverlay() const
{
    if (!measurementResult_.hasStartPoint) {
        return nullptr;
    }

    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    osg::ref_ptr<osg::Group> overlay = new osg::Group();

    osg::ref_ptr<osg::Geode> markersGeode = buildMeasurementGeode(measurementResult_, sceneOrigin, false);
    if (markersGeode.valid()) {
        overlay->addChild(markersGeode.get());
    }

    osg::ref_ptr<osg::Geode> lineGeode = buildMeasurementGeode(measurementResult_, sceneOrigin, true);
    if (lineGeode.valid()) {
        overlay->addChild(lineGeode.get());
    }

    return overlay->getNumChildren() > 0
        ? wrapOverlayNodeWithSceneOrigin(overlay.release(), sceneOrigin)
        : nullptr;
}

void PointCloudViewer::updateMeasurementOverlayWidgets()
{
    auto hideAllLabels = [this]() {
        if (measurementStartOverlayLabel_ != nullptr) {
            measurementStartOverlayLabel_->hide();
        }
        if (measurementEndOverlayLabel_ != nullptr) {
            measurementEndOverlayLabel_->hide();
        }
        if (measurementSummaryOverlayLabel_ != nullptr) {
            measurementSummaryOverlayLabel_->hide();
        }
    };

    if (osgWidget_ == nullptr || measurementResult_.points.isEmpty()) {
        hideAllLabels();
        return;
    }

    bool startVisible = false;
    const QPointF startPoint = projectPointToViewport(measurementResult_.startPoint, &startVisible);
    if (measurementStartOverlayLabel_ != nullptr && startVisible) {
        measurementStartOverlayLabel_->setText(QStringLiteral("1"));
        positionOverlayLabel(measurementStartOverlayLabel_, startPoint, QPoint(14, -18));
    } else if (measurementStartOverlayLabel_ != nullptr) {
        measurementStartOverlayLabel_->hide();
    }

    if (!measurementResult_.hasEndPoint) {
        if (measurementEndOverlayLabel_ != nullptr) {
            measurementEndOverlayLabel_->hide();
        }
        if (measurementSummaryOverlayLabel_ != nullptr) {
            measurementSummaryOverlayLabel_->hide();
        }
        return;
    }

    bool endVisible = false;
    const QPointF endPoint = projectPointToViewport(measurementResult_.endPoint, &endVisible);
    if (measurementEndOverlayLabel_ != nullptr && endVisible) {
        measurementEndOverlayLabel_->setText(QLocale().toString(measurementResult_.pointCount()));
        positionOverlayLabel(measurementEndOverlayLabel_, endPoint, QPoint(14, -18));
    } else if (measurementEndOverlayLabel_ != nullptr) {
        measurementEndOverlayLabel_->hide();
    }

    if (measurementSummaryOverlayLabel_ == nullptr || (!startVisible && !endVisible)) {
        if (measurementSummaryOverlayLabel_ != nullptr) {
            measurementSummaryOverlayLabel_->hide();
        }
        return;
    }

    measurementSummaryOverlayLabel_->setText(
        tr("%1 pts | 3D %2 | Height %3")
            .arg(QLocale().toString(measurementResult_.pointCount()))
            .arg(formatCoordinate(measurementResult_.distance3d))
            .arg(formatCoordinate(measurementResult_.deltaZ)));

    const QPointF summaryAnchor = startVisible && endVisible
        ? QPointF((startPoint.x() + endPoint.x()) * 0.5, (startPoint.y() + endPoint.y()) * 0.5)
        : (startVisible ? startPoint : endPoint);
    positionOverlayLabel(measurementSummaryOverlayLabel_, summaryAnchor, QPoint(0, -34));
}

void PointCloudViewer::refreshMeasurementOverlay()
{
    if (rootGroup_.valid() && measurementOverlayNode_.valid()) {
        rootGroup_->removeChild(measurementOverlayNode_.get());
        measurementOverlayNode_ = nullptr;
    }

    if (rootGroup_.valid() && measurementResult_.hasStartPoint) {
        measurementOverlayNode_ = buildMeasurementOverlay();
        if (measurementOverlayNode_.valid()) {
            rootGroup_->addChild(measurementOverlayNode_.get());
        }
    }

    updateMeasurementOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::recalculateMeasurementResult()
{
    measurementResult_.hasStartPoint = !measurementResult_.points.isEmpty();
    measurementResult_.hasEndPoint = measurementResult_.points.size() >= 2;
    measurementResult_.distance3d = 0.0f;
    measurementResult_.deltaZ = 0.0f;

    if (!measurementResult_.hasStartPoint) {
        measurementResult_.startPoint = PointRecord();
        measurementResult_.endPoint = PointRecord();
        return;
    }

    measurementResult_.startPoint = measurementResult_.points.constFirst();
    measurementResult_.endPoint = measurementResult_.points.constLast();

    if (!measurementResult_.hasEndPoint) {
        return;
    }

    for (int pointIndex = 1; pointIndex < measurementResult_.points.size(); ++pointIndex) {
        const PointRecord& previousPoint = measurementResult_.points.at(pointIndex - 1);
        const PointRecord& currentPoint = measurementResult_.points.at(pointIndex);
        const double dx = static_cast<double>(currentPoint.x - previousPoint.x);
        const double dy = static_cast<double>(currentPoint.y - previousPoint.y);
        const double dz = static_cast<double>(currentPoint.z - previousPoint.z);
        measurementResult_.distance3d += static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
    }

    measurementResult_.deltaZ = measurementResult_.endPoint.z - measurementResult_.startPoint.z;
}

bool PointCloudViewer::undoLastMeasurementPoint()
{
    if (measurementResult_.points.isEmpty()) {
        return false;
    }

    measurementResult_.points.removeLast();
    recalculateMeasurementResult();
    refreshMeasurementOverlay();
    updateFooter();
    emit measurementChanged();
    return true;
}

void PointCloudViewer::resetMeasurementState(bool notifyChange)
{
    const bool hadMeasurement = !measurementResult_.points.isEmpty();
    measurementResult_ = MeasurementResult();

    if (hasPointCloud()) {
        refreshMeasurementOverlay();
        updateFooter();
    }

    if (notifyChange && hadMeasurement) {
        emit measurementChanged();
    } else if (notifyChange && measurementEnabled_) {
        emit measurementChanged();
    }
}
