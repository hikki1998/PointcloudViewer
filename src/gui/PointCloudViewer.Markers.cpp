#include "gui/PointCloudViewer.h"

#include <QLabel>

#include <algorithm>
#include <cmath>
#include <limits>

#include <osg/Array>
#include <osg/Camera>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/Point>
#include <osg/StateSet>
#include <osg/Viewport>

#include "domain/DataManager.h"

namespace
{
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

void applyForegroundState(osg::StateSet* stateSet)
{
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);
    stateSet->setAttributeAndModes(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false), osg::StateAttribute::ON);
    stateSet->setRenderBinDetails(100, "RenderBin");
}

osg::ref_ptr<osg::Geode> buildTowerMarkersGeode(
    const QList<TowerRecord>& towerMarkers,
    const osg::Vec3d& origin)
{
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    for (const TowerRecord& tower : towerMarkers) {
        vertices->push_back(toOverlayLocalVec3(tower.point, origin));
        colors->push_back(osg::Vec4(0.99f, 0.43f, 0.12f, 1.0f));
    }
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices->size())));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());
    geode->getOrCreateStateSet()->setAttributeAndModes(new osg::Point(12.0f), osg::StateAttribute::ON);
    applyForegroundState(geode->getOrCreateStateSet());
    return geode;
}

osg::ref_ptr<osg::Geode> buildInspectionIssuesGeode(
    const QList<InspectionIssue>& issues,
    const osg::Vec3d& origin)
{
    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    for (const InspectionIssue& issue : issues) {
        vertices->push_back(toOverlayLocalVec3(issue.point, origin));
        switch (issue.severity) {
        case IssueSeverity::Info: colors->push_back(osg::Vec4(0.29f, 0.60f, 0.94f, 1.0f)); break;
        case IssueSeverity::Minor: colors->push_back(osg::Vec4(0.98f, 0.76f, 0.24f, 1.0f)); break;
        case IssueSeverity::Major: colors->push_back(osg::Vec4(0.96f, 0.49f, 0.20f, 1.0f)); break;
        case IssueSeverity::Critical:
        default: colors->push_back(osg::Vec4(0.86f, 0.16f, 0.16f, 1.0f)); break;
        }
    }
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    geometry->setVertexArray(vertices.get());
    geometry->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices->size())));
    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(geometry.get());
    geode->getOrCreateStateSet()->setAttributeAndModes(new osg::Point(13.0f), osg::StateAttribute::ON);
    applyForegroundState(geode->getOrCreateStateSet());
    return geode;
}
}

const QList<TowerRecord>& PointCloudViewer::towerMarkers() const
{
    return towerMarkers_;
}

const QList<InspectionIssue>& PointCloudViewer::inspectionIssues() const
{
    return inspectionIssues_;
}

int PointCloudViewer::selectedTowerIndex() const
{
    return selectedTowerIndex_;
}

int PointCloudViewer::selectedIssueIndex() const
{
    return selectedIssueIndex_;
}

TowerEditMode PointCloudViewer::towerEditMode() const
{
    return towerEditMode_;
}

int PointCloudViewer::towerEditTargetIndex() const
{
    return towerEditTargetIndex_;
}

IssueEditMode PointCloudViewer::issueEditMode() const
{
    return issueEditMode_;
}

bool PointCloudViewer::addTowerMarker(const QString& name, const PointRecord& point)
{
    return insertTowerMarker(towerMarkers_.size(), name, point);
}

bool PointCloudViewer::insertTowerMarker(int index, const QString& name, const PointRecord& point)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty() || index < 0 || index > towerMarkers_.size()) {
        return false;
    }

    TowerRecord towerMarker;
    towerMarker.index = index;
    towerMarker.name = trimmedName;
    towerMarker.point = point;
    towerMarkers_.insert(index, towerMarker);
    normalizeTowerMarkerIndices();
    selectedTowerIndex_ = index;
    if (towerEditMode_ == TowerEditMode::InsertBeforeSelected) {
        towerEditTargetIndex_ = selectedTowerIndex_;
    }
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
    return true;
}

bool PointCloudViewer::addTowerMarkerFromHoveredPoint(const QString& name, QString* errorMessage)
{
    if (!hoveredPointValid_) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Hover a point before adding a tower marker.");
        }
        return false;
    }

    if (!addTowerMarker(name, hoveredPoint_)) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Tower marker name cannot be empty.");
        }
        return false;
    }

    return true;
}

void PointCloudViewer::setTowerMarkers(const QList<TowerRecord>& towerMarkers)
{
    towerMarkers_ = towerMarkers;
    normalizeTowerMarkerIndices();
    selectedTowerIndex_ = towerMarkers_.isEmpty() ? -1 : std::clamp(selectedTowerIndex_, 0, towerMarkers_.size() - 1);
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
}

bool PointCloudViewer::setTowerMarkerName(int index, const QString& name)
{
    if (index < 0 || index >= towerMarkers_.size()) {
        return false;
    }

    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return false;
    }

    if (towerMarkers_[index].name == trimmedName) {
        return true;
    }

    towerMarkers_[index].name = trimmedName;
    refreshTowerMarkersOverlay();
    emit towerMarkersChanged();
    return true;
}

bool PointCloudViewer::setTowerRecord(int index, const TowerRecord& towerRecord)
{
    if (index < 0 || index >= towerMarkers_.size() || towerRecord.name.trimmed().isEmpty()) {
        return false;
    }

    towerMarkers_[index] = towerRecord;
    towerMarkers_[index].index = index;
    towerMarkers_[index].name = towerRecord.name.trimmed();
    refreshTowerMarkersOverlay();
    emit towerMarkersChanged();
    return true;
}

void PointCloudViewer::setSelectedTowerIndex(int index)
{
    const int normalizedIndex = (index >= 0 && index < towerMarkers_.size()) ? index : -1;
    if (selectedTowerIndex_ == normalizedIndex) {
        return;
    }

    selectedTowerIndex_ = normalizedIndex;
    if ((towerEditMode_ == TowerEditMode::InsertBeforeSelected || towerEditMode_ == TowerEditMode::MoveSelected)
        && selectedTowerIndex_ >= 0) {
        towerEditTargetIndex_ = selectedTowerIndex_;
    }
    selectedIssueIndex_ = -1;
    refreshTowerMarkersOverlay();
    refreshInspectionIssuesOverlay();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit selectedIssueChanged(selectedIssueIndex_);
}

bool PointCloudViewer::removeTowerMarker(int index)
{
    if (index < 0 || index >= towerMarkers_.size()) {
        return false;
    }

    towerMarkers_.removeAt(index);
    normalizeTowerMarkerIndices();
    if (towerMarkers_.isEmpty()) {
        selectedTowerIndex_ = -1;
    } else {
        selectedTowerIndex_ = std::clamp(index, 0, towerMarkers_.size() - 1);
    }
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
    return true;
}

bool PointCloudViewer::moveTowerMarker(int index, const PointRecord& point)
{
    if (index < 0 || index >= towerMarkers_.size()) {
        return false;
    }

    towerMarkers_[index].point = point;
    towerMarkers_[index].index = index;
    selectedTowerIndex_ = index;
    if (towerEditMode_ == TowerEditMode::MoveSelected) {
        towerEditTargetIndex_ = selectedTowerIndex_;
    }
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
    return true;
}

void PointCloudViewer::clearTowerMarkers()
{
    if (towerMarkers_.isEmpty()) {
        return;
    }

    towerMarkers_.clear();
    selectedTowerIndex_ = -1;
    cancelTowerEditMode();
    updateSceneClickCapture();
    refreshTowerMarkersOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerMarkersChanged();
}

void PointCloudViewer::normalizeTowerMarkerIndices()
{
    for (int index = 0; index < towerMarkers_.size(); ++index) {
        towerMarkers_[index].index = index;
    }
}

void PointCloudViewer::beginTowerAddMode()
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    towerEditMode_ = TowerEditMode::AddAfterLast;
    towerEditTargetIndex_ = -1;
    towerAddModeStartCount_ = towerMarkers_.size();
    cancelIssueEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    emit towerEditModeChanged();
}

void PointCloudViewer::beginTowerInsertMode(int beforeIndex)
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    if (beforeIndex < 0 || beforeIndex >= towerMarkers_.size()) {
        return;
    }

    selectedTowerIndex_ = beforeIndex;
    selectedIssueIndex_ = -1;
    towerEditMode_ = TowerEditMode::InsertBeforeSelected;
    towerEditTargetIndex_ = beforeIndex;
    towerAddModeStartCount_ = towerMarkers_.size();
    cancelIssueEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    refreshTowerMarkersOverlay();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
}

void PointCloudViewer::beginTowerMoveMode(int towerIndex)
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    if (towerIndex < 0 || towerIndex >= towerMarkers_.size()) {
        return;
    }

    selectedTowerIndex_ = towerIndex;
    selectedIssueIndex_ = -1;
    towerEditMode_ = TowerEditMode::MoveSelected;
    towerEditTargetIndex_ = towerIndex;
    towerAddModeStartCount_ = towerMarkers_.size();
    cancelIssueEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    refreshTowerMarkersOverlay();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit towerEditModeChanged();
}

void PointCloudViewer::cancelTowerEditMode()
{
    if (towerEditMode_ == TowerEditMode::None) {
        return;
    }

    towerEditMode_ = TowerEditMode::None;
    towerEditTargetIndex_ = -1;
    towerAddModeStartCount_ = towerMarkers_.size();
    updateSceneClickCapture();
    emit towerEditModeChanged();
}

void PointCloudViewer::setInspectionIssues(const QList<InspectionIssue>& issues)
{
    inspectionIssues_ = issues;
    hiddenInspectionIssueIndices_.clear();
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = inspectionIssues_.isEmpty() ? -1 : std::clamp(selectedIssueIndex_, 0, inspectionIssues_.size() - 1);
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
}

bool PointCloudViewer::addInspectionIssue(const InspectionIssue& issue)
{
    if (issue.title.trimmed().isEmpty()) {
        return false;
    }

    InspectionIssue normalizedIssue = issue;
    normalizedIssue.id = normalizedIssue.id.trimmed().isEmpty() ? issueDefaultId() : normalizedIssue.id.trimmed();
    normalizedIssue.title = normalizedIssue.title.trimmed();
    if (normalizedIssue.createdAt.trimmed().isEmpty()) {
        normalizedIssue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
    inspectionIssues_.append(normalizedIssue);
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = inspectionIssues_.size() - 1;
    selectedTowerIndex_ = -1;
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedTowerChanged(selectedTowerIndex_);
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
    return true;
}

bool PointCloudViewer::updateInspectionIssue(int index, const InspectionIssue& issue)
{
    if (index < 0 || index >= inspectionIssues_.size() || issue.title.trimmed().isEmpty()) {
        return false;
    }

    InspectionIssue normalizedIssue = issue;
    normalizedIssue.id = normalizedIssue.id.trimmed().isEmpty() ? issueDefaultId() : normalizedIssue.id.trimmed();
    normalizedIssue.title = normalizedIssue.title.trimmed();
    if (normalizedIssue.createdAt.trimmed().isEmpty()) {
        normalizedIssue.createdAt = inspectionIssues_.at(index).createdAt;
    }
    inspectionIssues_[index] = normalizedIssue;
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    refreshInspectionIssuesOverlay();
    emit inspectionIssuesChanged();
    return true;
}

bool PointCloudViewer::removeInspectionIssue(int index)
{
    if (index < 0 || index >= inspectionIssues_.size()) {
        return false;
    }

    inspectionIssues_.removeAt(index);
    QSet<int> remappedHiddenIndices;
    for (int hiddenIndex : hiddenInspectionIssueIndices_) {
        if (hiddenIndex == index) {
            continue;
        }
        remappedHiddenIndices.insert(hiddenIndex > index ? hiddenIndex - 1 : hiddenIndex);
    }
    hiddenInspectionIssueIndices_ = std::move(remappedHiddenIndices);
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = inspectionIssues_.isEmpty() ? -1 : std::clamp(index, 0, inspectionIssues_.size() - 1);
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
    return true;
}

void PointCloudViewer::clearInspectionIssues()
{
    if (inspectionIssues_.isEmpty()) {
        return;
    }

    inspectionIssues_.clear();
    hiddenInspectionIssueIndices_.clear();
    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    selectedIssueIndex_ = -1;
    cancelIssueEditMode();
    updateSceneClickCapture();
    refreshInspectionIssuesOverlay();
    updateFooter();
    emit selectedIssueChanged(selectedIssueIndex_);
    emit inspectionIssuesChanged();
}

void PointCloudViewer::setSelectedIssueIndex(int index)
{
    const int normalizedIndex = (index >= 0 && index < inspectionIssues_.size()) ? index : -1;
    if (selectedIssueIndex_ == normalizedIndex) {
        return;
    }

    selectedIssueIndex_ = normalizedIndex;
    if (selectedIssueIndex_ >= 0) {
        selectedTowerIndex_ = -1;
        emit selectedTowerChanged(selectedTowerIndex_);
    }
    refreshInspectionIssuesOverlay();
    refreshTowerMarkersOverlay();
    emit selectedIssueChanged(selectedIssueIndex_);
}

void PointCloudViewer::beginIssueAddMode()
{
    if (profileClassificationModeEnabled_) {
        setProfileClassificationModeEnabled(false);
    }
    issueEditMode_ = IssueEditMode::Add;
    cancelTowerEditMode();
    if (measurementEnabled_) {
        setMeasurementEnabled(false);
    } else {
        updateSceneClickCapture();
    }
    emit issueEditModeChanged();
}

void PointCloudViewer::cancelIssueEditMode()
{
    if (issueEditMode_ == IssueEditMode::None) {
        return;
    }

    issueEditMode_ = IssueEditMode::None;
    updateSceneClickCapture();
    emit issueEditModeChanged();
}

bool PointCloudViewer::isInspectionIssueVisible(int index) const
{
    return index >= 0
        && index < inspectionIssues_.size()
        && !hiddenInspectionIssueIndices_.contains(index);
}

void PointCloudViewer::setInspectionIssueVisible(int index, bool visible)
{
    if (index < 0 || index >= inspectionIssues_.size()) {
        return;
    }

    const bool currentlyVisible = !hiddenInspectionIssueIndices_.contains(index);
    if (currentlyVisible == visible) {
        return;
    }

    if (visible) {
        hiddenInspectionIssueIndices_.remove(index);
    } else {
        hiddenInspectionIssueIndices_.insert(index);
    }

    DataManager::instance().setImagesFromIssues(inspectionIssues_, hiddenInspectionIssueIndices_);
    refreshInspectionIssuesOverlay();
    updateFooter();
}

int PointCloudViewer::pickTowerMarkerAtScreenPosition(const QPointF& localPos, float tolerancePixels) const
{
    if (towerMarkers_.isEmpty() || osgWidget_ == nullptr) {
        return -1;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return -1;
    }

    osg::Camera* camera = viewer->getCamera();
    const osg::Matrixd worldToWindow =
        camera->getViewMatrix() *
        camera->getProjectionMatrix() *
        camera->getViewport()->computeWindowMatrix();
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const osg::Matrixd localToWindow = osg::Matrixd::translate(sceneOrigin) * worldToWindow;

    const double devicePixelRatio = osgWidget_->devicePixelRatioF();
    const double clickX = localPos.x() * devicePixelRatio;
    const double clickY = (static_cast<double>(osgWidget_->height()) - localPos.y()) * devicePixelRatio;
    const double tolerance = static_cast<double>(tolerancePixels) * devicePixelRatio;
    const double toleranceSquared = tolerance * tolerance;

    int bestIndex = -1;
    double bestDistanceSquared = toleranceSquared;
    double bestDepth = std::numeric_limits<double>::max();

    for (int towerIndex = 0; towerIndex < towerMarkers_.size(); ++towerIndex) {
        const TowerMarker& towerMarker = towerMarkers_.at(towerIndex);
        const osg::Vec3d projected = osg::Vec3d(
            towerMarker.point.x - sceneOrigin.x(),
            towerMarker.point.y - sceneOrigin.y(),
            towerMarker.point.z - sceneOrigin.z())
            * localToWindow;
        if (projected.z() < 0.0 || projected.z() > 1.0) {
            continue;
        }

        const double dx = projected.x() - clickX;
        const double dy = projected.y() - clickY;
        const double distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > bestDistanceSquared) {
            continue;
        }

        if (bestIndex < 0
            || distanceSquared < bestDistanceSquared - 0.001
            || (std::abs(distanceSquared - bestDistanceSquared) <= 0.001 && projected.z() < bestDepth)) {
            bestIndex = towerIndex;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
        }
    }

    return bestIndex;
}

int PointCloudViewer::pickInspectionIssueAtScreenPosition(const QPointF& localPos, float tolerancePixels) const
{
    if (inspectionIssues_.isEmpty() || osgWidget_ == nullptr) {
        return -1;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCamera() == nullptr || viewer->getCamera()->getViewport() == nullptr) {
        return -1;
    }

    osg::Camera* camera = viewer->getCamera();
    const osg::Matrixd worldToWindow =
        camera->getViewMatrix() *
        camera->getProjectionMatrix() *
        camera->getViewport()->computeWindowMatrix();
    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    const osg::Matrixd localToWindow = osg::Matrixd::translate(sceneOrigin) * worldToWindow;

    const double devicePixelRatio = osgWidget_->devicePixelRatioF();
    const double clickX = localPos.x() * devicePixelRatio;
    const double clickY = (static_cast<double>(osgWidget_->height()) - localPos.y()) * devicePixelRatio;
    const double tolerance = static_cast<double>(tolerancePixels) * devicePixelRatio;
    const double toleranceSquared = tolerance * tolerance;

    int bestIndex = -1;
    double bestDistanceSquared = toleranceSquared;
    double bestDepth = std::numeric_limits<double>::max();

    for (int issueIndex = 0; issueIndex < inspectionIssues_.size(); ++issueIndex) {
        if (hiddenInspectionIssueIndices_.contains(issueIndex)) {
            continue;
        }
        const InspectionIssue& issue = inspectionIssues_.at(issueIndex);
        const osg::Vec3d projected = osg::Vec3d(
            issue.point.x - sceneOrigin.x(),
            issue.point.y - sceneOrigin.y(),
            issue.point.z - sceneOrigin.z())
            * localToWindow;
        if (projected.z() < 0.0 || projected.z() > 1.0) {
            continue;
        }

        const double dx = projected.x() - clickX;
        const double dy = projected.y() - clickY;
        const double distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > bestDistanceSquared) {
            continue;
        }

        if (bestIndex < 0
            || distanceSquared < bestDistanceSquared - 0.001
            || (std::abs(distanceSquared - bestDistanceSquared) <= 0.001 && projected.z() < bestDepth)) {
            bestIndex = issueIndex;
            bestDistanceSquared = distanceSquared;
            bestDepth = projected.z();
        }
    }

    return bestIndex;
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildTowerMarkersOverlay() const
{
    if (towerMarkers_.isEmpty()) {
        return nullptr;
    }

    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    osg::ref_ptr<osg::Geode> markersGeode = buildTowerMarkersGeode(towerMarkers_, sceneOrigin);
    return wrapOverlayNodeWithSceneOrigin(markersGeode.release(), sceneOrigin);
}

osg::ref_ptr<osg::Node> PointCloudViewer::buildInspectionIssuesOverlay() const
{
    if (inspectionIssues_.isEmpty()) {
        return nullptr;
    }

    QList<InspectionIssue> visibleIssues;
    visibleIssues.reserve(inspectionIssues_.size());
    for (int issueIndex = 0; issueIndex < inspectionIssues_.size(); ++issueIndex) {
        if (hiddenInspectionIssueIndices_.contains(issueIndex)) {
            continue;
        }
        visibleIssues.append(inspectionIssues_.at(issueIndex));
    }

    if (visibleIssues.isEmpty()) {
        return nullptr;
    }

    const osg::Vec3d sceneOrigin = overlaySceneOrigin();
    osg::ref_ptr<osg::Geode> markersGeode = buildInspectionIssuesGeode(visibleIssues, sceneOrigin);
    return wrapOverlayNodeWithSceneOrigin(markersGeode.release(), sceneOrigin);
}

void PointCloudViewer::updateTowerOverlayWidgets()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    while (towerOverlayLabels_.size() < towerMarkers_.size()) {
        auto* label = new QLabel(osgWidget_);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: rgba(124, 45, 18, 220);"
            "color: #fff7ed;"
            "border: 1px solid rgba(251, 146, 60, 180);"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}"));
        label->hide();
        towerOverlayLabels_.append(label);
    }

    for (int towerIndex = 0; towerIndex < towerOverlayLabels_.size(); ++towerIndex) {
        QLabel* label = towerOverlayLabels_.at(towerIndex);
        if (label == nullptr) {
            continue;
        }

        if (towerIndex >= towerMarkers_.size()) {
            label->hide();
            continue;
        }

        bool pointVisible = false;
        const TowerMarker& towerMarker = towerMarkers_.at(towerIndex);
        const QPointF anchor = projectPointToViewport(towerMarker.point, &pointVisible);
        if (!pointVisible) {
            label->hide();
            continue;
        }

        label->setText(towerMarker.name);
        const bool isSelected = towerIndex == selectedTowerIndex_;
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: %1;"
            "color: %2;"
            "border: 1px solid %3;"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}").arg(
                isSelected ? QStringLiteral("rgba(180, 83, 9, 235)") : QStringLiteral("rgba(124, 45, 18, 220)"),
                QStringLiteral("#fff7ed"),
                isSelected ? QStringLiteral("rgba(253, 224, 71, 220)") : QStringLiteral("rgba(251, 146, 60, 180)")));
        positionOverlayLabel(label, anchor, QPoint(14, -18));
    }
}

void PointCloudViewer::updateInspectionIssueOverlayWidgets()
{
    if (osgWidget_ == nullptr) {
        return;
    }

    while (issueOverlayLabels_.size() < inspectionIssues_.size()) {
        auto* label = new QLabel(osgWidget_);
        label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        label->hide();
        issueOverlayLabels_.append(label);
    }

    for (int issueIndex = 0; issueIndex < issueOverlayLabels_.size(); ++issueIndex) {
        QLabel* label = issueOverlayLabels_.at(issueIndex);
        if (label == nullptr) {
            continue;
        }

        if (issueIndex >= inspectionIssues_.size() || hiddenInspectionIssueIndices_.contains(issueIndex)) {
            label->hide();
            continue;
        }

        bool pointVisible = false;
        const InspectionIssue& issue = inspectionIssues_.at(issueIndex);
        const QPointF anchor = projectPointToViewport(issue.point, &pointVisible);
        if (!pointVisible) {
            label->hide();
            continue;
        }

        QString backgroundColor = QStringLiteral("rgba(153, 27, 27, 220)");
        QString borderColor = QStringLiteral("rgba(252, 165, 165, 180)");
        switch (issue.severity) {
        case IssueSeverity::Info:
            backgroundColor = QStringLiteral("rgba(30, 64, 175, 220)");
            borderColor = QStringLiteral("rgba(147, 197, 253, 190)");
            break;
        case IssueSeverity::Minor:
            backgroundColor = QStringLiteral("rgba(161, 98, 7, 220)");
            borderColor = QStringLiteral("rgba(253, 230, 138, 190)");
            break;
        case IssueSeverity::Major:
            backgroundColor = QStringLiteral("rgba(194, 65, 12, 220)");
            borderColor = QStringLiteral("rgba(253, 186, 116, 190)");
            break;
        case IssueSeverity::Critical:
        default:
            break;
        }

        const bool isSelected = issueIndex == selectedIssueIndex_;
        label->setText(issue.title);
        label->setStyleSheet(QStringLiteral(
            "QLabel {"
            "background-color: %1;"
            "color: #fff7ed;"
            "border: 1px solid %2;"
            "border-radius: 8px;"
            "padding: 4px 8px;"
            "font-size: 12px;"
            "font-weight: 600;"
            "}").arg(
                isSelected ? QStringLiteral("rgba(126, 34, 206, 230)") : backgroundColor,
                isSelected ? QStringLiteral("rgba(233, 213, 255, 220)") : borderColor));
        positionOverlayLabel(label, anchor, QPoint(14, 16));
    }
}

void PointCloudViewer::refreshTowerMarkersOverlay()
{
    if (rootGroup_.valid() && towerMarkersNode_.valid()) {
        rootGroup_->removeChild(towerMarkersNode_.get());
        towerMarkersNode_ = nullptr;
    }

    if (rootGroup_.valid() && !towerMarkers_.isEmpty()) {
        towerMarkersNode_ = buildTowerMarkersOverlay();
        if (towerMarkersNode_.valid()) {
            rootGroup_->addChild(towerMarkersNode_.get());
        }
    }

    updateTowerOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::refreshInspectionIssuesOverlay()
{
    if (rootGroup_.valid() && inspectionIssuesNode_.valid()) {
        rootGroup_->removeChild(inspectionIssuesNode_.get());
        inspectionIssuesNode_ = nullptr;
    }

    if (rootGroup_.valid() && !inspectionIssues_.isEmpty()) {
        inspectionIssuesNode_ = buildInspectionIssuesOverlay();
        if (inspectionIssuesNode_.valid()) {
            rootGroup_->addChild(inspectionIssuesNode_.get());
        }
    }

    updateInspectionIssueOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}
