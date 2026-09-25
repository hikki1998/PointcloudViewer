#include "gui/PointCloudViewer.h"
#include "gui/PointCloudViewerOverlays.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <osg/Array>
#include <osg/Camera>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LineWidth>
#include <osg/MatrixTransform>
#include <osg/StateSet>
#include <osg/Viewport>

#include "pointcloud/ClipFilter.h"

namespace
{
ClipPlane makeClipPlane(const osg::Vec3d& pointOnPlane, const osg::Vec3d& inwardNormal)
{
    ClipPlane plane;
    plane.normal = inwardNormal;
    if (plane.normal.length2() > 0.0) plane.normal.normalize();
    plane.distance = -(plane.normal * pointOnPlane);
    return plane;
}

void orientPlaneTowardPoint(ClipPlane* plane, const osg::Vec3d& interiorPoint)
{
    if (plane != nullptr && plane->normal * interiorPoint + plane->distance < 0.0) {
        plane->normal = -plane->normal;
        plane->distance = -plane->distance;
    }
}

osg::ref_ptr<osg::Node> wrapOverlayNodeWithSceneOrigin(osg::Node* node, const osg::Vec3d& origin)
{
    if (node == nullptr) {
        return nullptr;
    }
    osg::ref_ptr<osg::MatrixTransform> transform = new osg::MatrixTransform();
    transform->setMatrix(osg::Matrixd::translate(origin));
    transform->addChild(node);
    return transform;
}
}

bool PointCloudViewer::hasActiveClipRegion() const
{
    return visualizationOptions_.clipRegion.isActive();
}

ClipRegion::Kind PointCloudViewer::clipMode() const
{
    if (clipEditMode_ != ClipRegion::None) {
        return clipEditMode_;
    }

    if (visualizationOptions_.clipRegion.isActive()) {
        return visualizationOptions_.clipRegion.kind;
    }

    return ClipRegion::None;
}

bool PointCloudViewer::clipKeepInside() const
{
    return clipKeepInside_;
}

ClipRegion::Scope PointCloudViewer::clipScope() const
{
    return clipScope_;
}

ClipRegion::BoxAlignment PointCloudViewer::clipBoxAlignment() const
{
    return clipBoxAlignment_;
}

void PointCloudViewer::setClipRegion(const ClipRegion& clipRegion)
{
    visualizationOptions_.clipRegion = clipRegion;
    clipKeepInside_ = clipRegion.keepInside;
    clipScope_ = clipRegion.scope;
    clipBoxAlignment_ = clipRegion.boxAlignment;
    clipActiveDatasetId_ = clipRegion.activeDatasetId;
    clearClipPreview();
    rebuildScene();
    refreshClipOverlay();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClipKeepInside(bool keepInside)
{
    if (clipKeepInside_ == keepInside && visualizationOptions_.clipRegion.keepInside == keepInside) {
        return;
    }

    clipKeepInside_ = keepInside;
    if (visualizationOptions_.clipRegion.kind != ClipRegion::None) {
        visualizationOptions_.clipRegion.keepInside = keepInside;
    }
    if (clipPreviewActive_) {
        clipPreviewRegion_.keepInside = keepInside;
    }

    rebuildScene();
    refreshClipOverlay();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClipScope(ClipRegion::Scope scope)
{
    if (clipScope_ == scope && visualizationOptions_.clipRegion.scope == scope) {
        return;
    }

    clipScope_ = scope;
    if (visualizationOptions_.clipRegion.kind != ClipRegion::None) {
        visualizationOptions_.clipRegion.scope = scope;
        visualizationOptions_.clipRegion.activeDatasetId = clipActiveDatasetId_;
    }
    if (clipPreviewActive_) {
        clipPreviewRegion_.scope = scope;
        clipPreviewRegion_.activeDatasetId = clipActiveDatasetId_;
    }

    rebuildScene();
    refreshClipOverlay();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClipBoxAlignment(ClipRegion::BoxAlignment alignment)
{
    if (clipBoxAlignment_ == alignment) {
        return;
    }

    clipBoxAlignment_ = alignment;
    if (clipEditMode_ == ClipRegion::Box) {
        resetClipEditingState();
        clipEditMode_ = ClipRegion::Box;
        updateSceneClickCapture();
        updateClipPolygonOverlay();
        clearClipPreview();
        refreshClipOverlay();
        emit measurementMessage(
            alignment == ClipRegion::ViewAligned
                ? tr("Box clip switched to view-aligned mode. Click the first corner point to start.")
                : tr("Box clip switched to world-aligned mode. Click the first corner point to start."),
            false);
    }
    emit visualizationOptionsChanged();
}

void PointCloudViewer::setClipActiveDatasetPath(const QString& datasetPath)
{
    clipActiveDatasetPath_ = datasetPath.trimmed();
    clipActiveDatasetId_ = -1;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (dataset.info.filePath.compare(clipActiveDatasetPath_, Qt::CaseInsensitive) == 0) {
            clipActiveDatasetId_ = dataset.info.datasetId;
            break;
        }
    }

    if (visualizationOptions_.clipRegion.kind != ClipRegion::None) {
        visualizationOptions_.clipRegion.activeDatasetId = clipActiveDatasetId_;
        rebuildScene();
    }
    if (clipPreviewActive_) {
        clipPreviewRegion_.activeDatasetId = clipActiveDatasetId_;
        refreshClipOverlay();
    }
}

void PointCloudViewer::beginPolygonClip()
{
    if (!hasPointCloud()) {
        emit measurementMessage(tr("Load a point cloud before starting clip selection."), true);
        return;
    }
    if (clipScope_ == ClipRegion::ActiveDataset && clipActiveDatasetId_ < 0) {
        emit measurementMessage(tr("Select a point cloud dataset before starting an active-dataset clip."), true);
        return;
    }

    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    }
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    if (towerEditMode_ != TowerEditMode::None) {
        cancelTowerEditMode();
    }
    if (issueEditMode_ != IssueEditMode::None) {
        cancelIssueEditMode();
    }

    resetClipEditingState();
    clearClipPreview();
    visualizationOptions_.clipRegion = ClipRegion();
    clipEditMode_ = ClipRegion::ScreenPolygonPrism;
    updateSceneClickCapture();
    updateClipPolygonOverlay();
    rebuildScene();
    refreshClipOverlay();
    emit visualizationOptionsChanged();
    emit measurementMessage(
        tr("Polygon clip mode enabled. Left-click to add vertices, right-click to undo, and double-click to finish."),
        false);
}

void PointCloudViewer::beginBoxClip()
{
    if (!hasPointCloud()) {
        emit measurementMessage(tr("Load a point cloud before starting clip selection."), true);
        return;
    }
    if (clipScope_ == ClipRegion::ActiveDataset && clipActiveDatasetId_ < 0) {
        emit measurementMessage(tr("Select a point cloud dataset before starting an active-dataset clip."), true);
        return;
    }

    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    }
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    if (towerEditMode_ != TowerEditMode::None) {
        cancelTowerEditMode();
    }
    if (issueEditMode_ != IssueEditMode::None) {
        cancelIssueEditMode();
    }

    resetClipEditingState();
    clearClipPreview();
    visualizationOptions_.clipRegion = ClipRegion();
    clipEditMode_ = ClipRegion::Box;
    updateSceneClickCapture();
    rebuildScene();
    refreshClipOverlay();
    emit visualizationOptionsChanged();
    emit measurementMessage(
        clipBoxAlignment_ == ClipRegion::ViewAligned
            ? tr("View-aligned box clip enabled. Click the first corner point to start.")
            : tr("World-aligned box clip enabled. Click the first corner point to start."),
        false);
}

void PointCloudViewer::clearClip()
{
    resetClipEditingState();
    clearClipPreview();
    visualizationOptions_.clipRegion = ClipRegion();
    updateSceneClickCapture();
    updateClipPolygonOverlay();
    rebuildScene();
    refreshClipOverlay();
    emit visualizationOptionsChanged();
}

void PointCloudViewer::resetClipEditingState()
{
    clipEditMode_ = ClipRegion::None;
    clipPolygonPoints_.clear();
    clipPolygonPreviewPoint_ = QPointF();
    clipPolygonPreviewActive_ = false;
    clipBoxFirstPointValid_ = false;
    clipBoxFirstPoint_ = PointRecord();
    clipLockedCamera_ = ClipCameraSnapshot();
}

void PointCloudViewer::clearClipPreview()
{
    clipPreviewActive_ = false;
    clipPreviewRegion_ = ClipRegion();
}

void PointCloudViewer::updateClipPolygonOverlay()
{
    if (clipPolygonOverlay_ == nullptr || osgWidget_ == nullptr) {
        return;
    }

    clipPolygonOverlay_->setGeometry(osgWidget_->rect());
    auto* overlay = static_cast<PolygonSelectionOverlay*>(clipPolygonOverlay_);
    const bool polygonVisible =
        clipEditMode_ == ClipRegion::ScreenPolygonPrism
        && !clipPolygonPoints_.isEmpty();

    if (!polygonVisible) {
        overlay->setPolygonState(QPolygonF(), false, QPointF());
        return;
    }

    overlay->setPolygonState(
        clipPolygonPoints_,
        clipPolygonPreviewActive_,
        clipPolygonPreviewPoint_);
}

bool PointCloudViewer::captureClipCameraSnapshot(
    osg::Vec3d* eye,
    osg::Vec3d* forward,
    osg::Vec3d* right,
    osg::Vec3d* up,
    osg::Matrixd* windowToWorld) const
{
    if (osgWidget_ == nullptr) {
        return false;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return false;
    }

    osg::Vec3d center;
    osg::Vec3d upVector;
    viewer->getCamera()->getViewMatrixAsLookAt(*eye, center, upVector);

    *forward = center - *eye;
    if (forward->length2() <= 0.0) {
        return false;
    }
    forward->normalize();

    *right = (*forward) ^ upVector;
    if (right->length2() <= 0.0) {
        return false;
    }
    right->normalize();

    *up = (*right) ^ (*forward);
    if (up->length2() <= 0.0) {
        return false;
    }
    up->normalize();

    osg::Camera* camera = viewer->getCamera();
    const osg::Matrixd worldToWindow =
        camera->getViewMatrix() *
        camera->getProjectionMatrix() *
        camera->getViewport()->computeWindowMatrix();
    *windowToWorld = osg::Matrixd::inverse(worldToWindow);
    return true;
}

bool PointCloudViewer::screenPointToWorldRay(
    const QPointF& localPos,
    const osg::Vec3d& eye,
    const osg::Matrixd& windowToWorld,
    osg::Vec3d* rayDirection) const
{
    if (osgWidget_ == nullptr || rayDirection == nullptr) {
        return false;
    }

    const double devicePixelRatio = osgWidget_->devicePixelRatioF();
    const osg::Vec3d farPoint(
        localPos.x() * devicePixelRatio,
        (static_cast<double>(osgWidget_->height()) - localPos.y()) * devicePixelRatio,
        1.0);
    *rayDirection = farPoint * windowToWorld - eye;
    if (rayDirection->length2() <= 0.0) {
        return false;
    }
    rayDirection->normalize();
    return true;
}

bool PointCloudViewer::buildScreenPolygonClipRegion(
    const QPolygonF& polygon,
    const osg::Vec3d& eye,
    const osg::Vec3d& forward,
    const osg::Matrixd& windowToWorld,
    ClipRegion* clipRegion) const
{
    if (clipRegion == nullptr || polygon.size() < 3) {
        return false;
    }

    if (polygon.size() + 2 > kMaxClipPlaneCount) {
        return false;
    }

    PointRecord minBounds;
    PointRecord maxBounds;
    if (!visiblePointCloudBounds(&minBounds, &maxBounds)) {
        return false;
    }
    const osg::Vec3d boundsCorners[8] = {
        osg::Vec3d(minBounds.x, minBounds.y, minBounds.z),
        osg::Vec3d(maxBounds.x, minBounds.y, minBounds.z),
        osg::Vec3d(maxBounds.x, maxBounds.y, minBounds.z),
        osg::Vec3d(minBounds.x, maxBounds.y, minBounds.z),
        osg::Vec3d(minBounds.x, minBounds.y, maxBounds.z),
        osg::Vec3d(maxBounds.x, minBounds.y, maxBounds.z),
        osg::Vec3d(maxBounds.x, maxBounds.y, maxBounds.z),
        osg::Vec3d(minBounds.x, maxBounds.y, maxBounds.z)
    };

    double frontDepth = std::numeric_limits<double>::max();
    double backDepth = std::numeric_limits<double>::lowest();
    for (const osg::Vec3d& corner : boundsCorners) {
        const double depth = forward * (corner - eye);
        frontDepth = std::min(frontDepth, depth);
        backDepth = std::max(backDepth, depth);
    }

    const double span = std::max(0.5, backDepth - frontDepth);
    const double margin = std::max(0.5, span * 0.01);
    frontDepth = std::max(0.01, frontDepth - margin);
    backDepth = std::max(frontDepth + 0.1, backDepth + margin);

    std::vector<osg::Vec3d> frontVertices;
    std::vector<osg::Vec3d> backVertices;
    frontVertices.reserve(polygon.size());
    backVertices.reserve(polygon.size());

    osg::Vec3d interiorPoint(0.0, 0.0, 0.0);
    for (const QPointF& vertex : polygon) {
        osg::Vec3d rayDirection;
        if (!screenPointToWorldRay(vertex, eye, windowToWorld, &rayDirection)) {
            return false;
        }

        const double frontDenominator = forward * rayDirection;
        if (std::abs(frontDenominator) <= 1e-6) {
            return false;
        }

        const osg::Vec3d frontPoint = eye + rayDirection * (frontDepth / frontDenominator);
        const osg::Vec3d backPoint = eye + rayDirection * (backDepth / frontDenominator);
        frontVertices.push_back(frontPoint);
        backVertices.push_back(backPoint);
        interiorPoint += frontPoint;
        interiorPoint += backPoint;
    }

    interiorPoint /= static_cast<double>(frontVertices.size() * 2);

    ClipRegion clip;
    clip.kind = ClipRegion::ScreenPolygonPrism;
    clip.scope = clipScope_;
    clip.boxAlignment = clipBoxAlignment_;
    clip.activeDatasetId = clipActiveDatasetId_;
    clip.enabled = true;
    clip.keepInside = clipKeepInside_;

    ClipPlane frontPlane = makeClipPlane(frontVertices.front(), forward);
    orientPlaneTowardPoint(&frontPlane, interiorPoint);
    clip.planes.push_back(frontPlane);

    ClipPlane backPlane = makeClipPlane(backVertices.front(), -forward);
    orientPlaneTowardPoint(&backPlane, interiorPoint);
    clip.planes.push_back(backPlane);

    const int vertexCount = static_cast<int>(frontVertices.size());
    for (int index = 0; index < vertexCount; ++index) {
        const int nextIndex = (index + 1) % vertexCount;
        osg::Vec3d sideNormal = (frontVertices[nextIndex] - eye) ^ (frontVertices[index] - eye);
        if (sideNormal.length2() <= 0.0) {
            return false;
        }
        ClipPlane sidePlane = makeClipPlane(eye, sideNormal);
        orientPlaneTowardPoint(&sidePlane, interiorPoint);
        clip.planes.push_back(sidePlane);
    }

    clip.overlayVertices = frontVertices;
    clip.overlayVertices.insert(clip.overlayVertices.end(), backVertices.begin(), backVertices.end());
    for (int index = 0; index < vertexCount; ++index) {
        const int nextIndex = (index + 1) % vertexCount;
        clip.overlayEdges.push_back({ index, nextIndex });
        clip.overlayEdges.push_back({ vertexCount + index, vertexCount + nextIndex });
        clip.overlayEdges.push_back({ index, vertexCount + index });
    }

    *clipRegion = std::move(clip);
    return true;
}

bool PointCloudViewer::buildBoxClipRegion(
    const PointRecord& firstPoint,
    const PointRecord& secondPoint,
    ClipRegion::BoxAlignment alignment,
    ClipRegion* clipRegion) const
{
    if (clipRegion == nullptr) {
        return false;
    }

    const osg::Vec3d p0(firstPoint.x, firstPoint.y, firstPoint.z);
    const osg::Vec3d p1(secondPoint.x, secondPoint.y, secondPoint.z);
    if ((p1 - p0).length2() <= 1e-8) {
        return false;
    }

    osg::Vec3d axisX(1.0, 0.0, 0.0);
    osg::Vec3d axisY(0.0, 1.0, 0.0);
    osg::Vec3d axisZ(0.0, 0.0, 1.0);
    if (alignment == ClipRegion::ViewAligned) {
        if (!clipLockedCamera_.valid) {
            return false;
        }
        axisX = clipLockedCamera_.right;
        axisY = clipLockedCamera_.up;
        axisZ = clipLockedCamera_.forward;
    }

    const osg::Vec3d delta = p1 - p0;
    const double sMax = delta * axisX;
    const double tMax = delta * axisY;
    const double uMax = delta * axisZ;
    const double sMin = std::min(0.0, sMax);
    const double tMin = std::min(0.0, tMax);
    const double uMin = std::min(0.0, uMax);
    const double sUpper = std::max(0.0, sMax);
    const double tUpper = std::max(0.0, tMax);
    const double uUpper = std::max(0.0, uMax);

    const auto cornerAt = [&](double s, double t, double u) {
        return p0 + axisX * s + axisY * t + axisZ * u;
    };

    const std::array<osg::Vec3d, 8> corners = {
        cornerAt(sMin, tMin, uMin),
        cornerAt(sUpper, tMin, uMin),
        cornerAt(sUpper, tUpper, uMin),
        cornerAt(sMin, tUpper, uMin),
        cornerAt(sMin, tMin, uUpper),
        cornerAt(sUpper, tMin, uUpper),
        cornerAt(sUpper, tUpper, uUpper),
        cornerAt(sMin, tUpper, uUpper)
    };

    const osg::Vec3d center = cornerAt((sMin + sUpper) * 0.5, (tMin + tUpper) * 0.5, (uMin + uUpper) * 0.5);

    ClipRegion clip;
    clip.kind = ClipRegion::Box;
    clip.scope = clipScope_;
    clip.boxAlignment = alignment;
    clip.activeDatasetId = clipActiveDatasetId_;
    clip.enabled = true;
    clip.keepInside = clipKeepInside_;
    clip.overlayVertices.assign(corners.begin(), corners.end());
    clip.overlayEdges = {
        std::array<int, 2>{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
        { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
    };

    ClipPlane planes[6] = {
        makeClipPlane(cornerAt(sMin, tMin, uMin), axisX),
        makeClipPlane(cornerAt(sUpper, tMin, uMin), -axisX),
        makeClipPlane(cornerAt(sMin, tMin, uMin), axisY),
        makeClipPlane(cornerAt(sMin, tUpper, uMin), -axisY),
        makeClipPlane(cornerAt(sMin, tMin, uMin), axisZ),
        makeClipPlane(cornerAt(sMin, tMin, uUpper), -axisZ)
    };
    for (ClipPlane& plane : planes) {
        orientPlaneTowardPoint(&plane, center);
        clip.planes.push_back(plane);
    }

    *clipRegion = std::move(clip);
    return true;
}

std::shared_ptr<PointCloudData> PointCloudViewer::buildClipExportData(
    const QString& activeDatasetPath,
    QString* errorMessage) const
{
    if (!visualizationOptions_.clipRegion.isActive()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No active clip region to export.");
        }
        return nullptr;
    }

    const ClipRegion& clip = visualizationOptions_.clipRegion;
    if (clip.scope == ClipRegion::ActiveDataset) {
        const QString resolvedPath = !activeDatasetPath.trimmed().isEmpty()
            ? activeDatasetPath.trimmed()
            : clipActiveDatasetPath_;
        if (resolvedPath.isEmpty()) {
            if (errorMessage != nullptr) {
                *errorMessage = tr("Select a point cloud dataset before exporting an active-dataset clip.");
            }
            return nullptr;
        }

        for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
            if (dataset.pointCloud == nullptr) {
                continue;
            }
            if (dataset.info.filePath.compare(resolvedPath, Qt::CaseInsensitive) != 0) {
                continue;
            }
            return ClipFilter::apply(*dataset.pointCloud, clip);
        }

        if (errorMessage != nullptr) {
            *errorMessage = tr("The selected dataset is not currently loaded.");
        }
        return nullptr;
    }

    auto mergedResult = std::make_shared<PointCloudData>();
    bool foundVisibleDataset = false;
    for (const LoadedPointCloudDataset& dataset : loadedPointCloudDatasets_) {
        if (!dataset.info.visible || dataset.pointCloud == nullptr) {
            continue;
        }

        foundVisibleDataset = true;
        std::shared_ptr<PointCloudData> filtered = ClipFilter::apply(*dataset.pointCloud, clip);
        if (filtered != nullptr && !filtered->empty()) {
            mergedResult->append(*filtered);
        }
    }

    if (!foundVisibleDataset) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No visible datasets are available for clip export.");
        }
        return nullptr;
    }

    return mergedResult;
}

void PointCloudViewer::refreshClipOverlay()
{
    if (rootGroup_.valid() && clipOverlayNode_.valid()) {
        rootGroup_->removeChild(clipOverlayNode_.get());
        clipOverlayNode_ = nullptr;
    }

    clipOverlayNode_ = buildClipOverlay();
    if (clipOverlayNode_.valid() && rootGroup_.valid()) {
        rootGroup_->addChild(clipOverlayNode_.get());
    }
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildClipOverlay() const
{
    const ClipRegion& clip = clipPreviewActive_ ? clipPreviewRegion_ : visualizationOptions_.clipRegion;
    if (!clip.isActive() || clip.overlayVertices.empty() || clip.overlayEdges.empty()) {
        return nullptr;
    }

    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    osg::ref_ptr<osg::Vec3dArray> vertices = new osg::Vec3dArray();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    const osg::Vec4 clipColor(0.0f, 0.88f, 0.82f, clipPreviewActive_ ? 0.95f : 0.85f);

    for (const std::array<int, 2>& edge : clip.overlayEdges) {
        if (edge[0] < 0
            || edge[1] < 0
            || edge[0] >= static_cast<int>(clip.overlayVertices.size())
            || edge[1] >= static_cast<int>(clip.overlayVertices.size())) {
            continue;
        }

        vertices->push_back(clip.overlayVertices[edge[0]] - sceneOrigin);
        vertices->push_back(clip.overlayVertices[edge[1]] - sceneOrigin);
        colors->push_back(clipColor);
        colors->push_back(clipColor);
    }

    if (vertices->empty()) {
        return nullptr;
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices->size())));

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());
    osg::StateSet* stateSet = geode->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(clipPreviewActive_ ? 2.4f : 2.0f), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    return wrapOverlayNodeWithSceneOrigin(geode.release(), sceneOrigin);
}
