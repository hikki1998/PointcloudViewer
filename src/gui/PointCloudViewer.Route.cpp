#include "gui/PointCloudViewer.h"

#include <QLocale>

#include <algorithm>
#include <cmath>

#include "domain/DataManager.h"

namespace
{
constexpr double kRoutePreviewDefaultFocalLengthRatio = 1.0;

double normalizedRoutePreviewFocalLengthRatio(double ratio)
{
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        return kRoutePreviewDefaultFocalLengthRatio;
    }
    return std::clamp(ratio, 0.1, 64.0);
}

bool routeLabelModeUsesSequence(RouteLabelDisplayMode mode)
{
    return mode == RouteLabelDisplayMode::Sequence || mode == RouteLabelDisplayMode::CompactSequence;
}

bool routeLabelModeHidden(RouteLabelDisplayMode mode)
{
    return mode == RouteLabelDisplayMode::Hidden;
}
}

const QList<PointRecord>& PointCloudViewer::inspectionRouteWaypoints() const
{
    return inspectionRouteWaypoints_;
}

QColor PointCloudViewer::inspectionRouteWaypointColor() const
{
    return inspectionRouteWaypointColor_;
}

QColor PointCloudViewer::inspectionRoutePartPointColor() const
{
    return inspectionRoutePartPointColor_;
}

QColor PointCloudViewer::inspectionRouteTrajectoryColor() const
{
    return inspectionRouteTrajectoryColor_;
}

int PointCloudViewer::selectedInspectionRouteWaypointIndex() const
{
    return selectedInspectionRouteWaypointIndex_;
}

void PointCloudViewer::setInspectionRouteDisplayData(const InspectionRouteDisplayData& displayData)
{
    routeRoamStopInternal(false);
    inspectionRouteWaypoints_ = displayData.waypoints;
    inspectionRouteLabels_ = displayData.labels;
    inspectionRoutePartPoints_ = displayData.partPoints;
    inspectionRoutePartLabels_ = displayData.partLabels;
    inspectionRoutePartPointIndices_ = displayData.partPointIndices;
    inspectionRouteWaypointTargetPoints_ = displayData.waypointTargetPoints;
    inspectionRouteWaypointHasTargetPoints_ = displayData.waypointHasTargetPoints;
    inspectionRouteWaypointAircraftYawDegs_ = displayData.waypointAircraftYawDegs;
    inspectionRouteWaypointGimbalPitchDegs_ = displayData.waypointGimbalPitchDegs;
    inspectionRouteWaypointCameraYawDegs_ = displayData.waypointCameraYawDegs;
    inspectionRouteWaypointCameraPitchDegs_ = displayData.waypointCameraPitchDegs;
    inspectionRouteWaypointFocalLengthRatios_ = displayData.waypointFocalLengthRatios;
    inspectionRouteWaypointTargetLabels_ = displayData.waypointTargetLabels;
    inspectionRouteWaypointAllTargetPoints_ = displayData.waypointAllTargetPoints;
    inspectionRouteWaypointAllTargetPartIndices_ = displayData.waypointAllTargetPartIndices;
    inspectionRouteWaypointAllCameraYawDegs_ = displayData.waypointAllCameraYawDegs;
    inspectionRouteWaypointAllCameraPitchDegs_ = displayData.waypointAllCameraPitchDegs;
    inspectionRouteWaypointAllFocalLengthRatios_ = displayData.waypointAllFocalLengthRatios;
    inspectionRouteWaypointAllTargetLabels_ = displayData.waypointAllTargetLabels;
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    DataManager::instance().setTrajectory(
        DataManager::instance().trajectoryItem().name.trimmed().isEmpty()
            ? tr("Inspection Route")
            : DataManager::instance().trajectoryItem().name,
        inspectionRouteWaypoints_,
        inspectionRouteVisible_);
    if (inspectionRouteLabels_.size() < inspectionRouteWaypoints_.size()) {
        for (int index = inspectionRouteLabels_.size(); index < inspectionRouteWaypoints_.size(); ++index) {
            inspectionRouteLabels_.append(QString::number(index + 1));
        }
    } else if (inspectionRouteLabels_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteLabels_.erase(
            inspectionRouteLabels_.begin() + inspectionRouteWaypoints_.size(),
            inspectionRouteLabels_.end());
    }

    while (inspectionRoutePartLabels_.size() < inspectionRoutePartPoints_.size()) {
        inspectionRoutePartLabels_.append(QString());
    }
    while (inspectionRoutePartLabels_.size() > inspectionRoutePartPoints_.size()) {
        inspectionRoutePartLabels_.removeLast();
    }

    while (inspectionRoutePartPointIndices_.size() < inspectionRoutePartPoints_.size()) {
        inspectionRoutePartPointIndices_.append(-1);
    }
    while (inspectionRoutePartPointIndices_.size() > inspectionRoutePartPoints_.size()) {
        inspectionRoutePartPointIndices_.removeLast();
    }

    if (inspectionRouteWaypointTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointTargetPoints_.append(PointRecord());
        }
    } else if (inspectionRouteWaypointTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointTargetPoints_.removeLast();
        }
    }

    if (inspectionRouteWaypointHasTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointHasTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointHasTargetPoints_.append(false);
        }
    } else if (inspectionRouteWaypointHasTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
        while (inspectionRouteWaypointHasTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
            inspectionRouteWaypointHasTargetPoints_.removeLast();
        }
    }

    while (inspectionRouteWaypointAircraftYawDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAircraftYawDegs_.append(0.0);
    }
    while (inspectionRouteWaypointAircraftYawDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAircraftYawDegs_.removeLast();
    }

    while (inspectionRouteWaypointGimbalPitchDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointGimbalPitchDegs_.append(0.0);
    }
    while (inspectionRouteWaypointGimbalPitchDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointGimbalPitchDegs_.removeLast();
    }

    while (inspectionRouteWaypointCameraYawDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraYawDegs_.append(0.0);
    }
    while (inspectionRouteWaypointCameraYawDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraYawDegs_.removeLast();
    }

    while (inspectionRouteWaypointCameraPitchDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraPitchDegs_.append(0.0);
    }
    while (inspectionRouteWaypointCameraPitchDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointCameraPitchDegs_.removeLast();
    }

    while (inspectionRouteWaypointFocalLengthRatios_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointFocalLengthRatios_.append(kRoutePreviewDefaultFocalLengthRatio);
    }
    while (inspectionRouteWaypointFocalLengthRatios_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointFocalLengthRatios_.removeLast();
    }

    while (inspectionRouteWaypointTargetLabels_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointTargetLabels_.append(QString());
    }
    while (inspectionRouteWaypointTargetLabels_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointTargetLabels_.removeLast();
    }

    while (inspectionRouteWaypointAllTargetPoints_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPoints_.append(QList<PointRecord>());
    }
    while (inspectionRouteWaypointAllTargetPoints_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPoints_.removeLast();
    }

    while (inspectionRouteWaypointAllTargetPartIndices_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPartIndices_.append(QList<int>());
    }
    while (inspectionRouteWaypointAllTargetPartIndices_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetPartIndices_.removeLast();
    }

    while (inspectionRouteWaypointAllCameraYawDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraYawDegs_.append(QList<double>());
    }
    while (inspectionRouteWaypointAllCameraYawDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraYawDegs_.removeLast();
    }

    while (inspectionRouteWaypointAllCameraPitchDegs_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraPitchDegs_.append(QList<double>());
    }
    while (inspectionRouteWaypointAllCameraPitchDegs_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllCameraPitchDegs_.removeLast();
    }

    while (inspectionRouteWaypointAllFocalLengthRatios_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllFocalLengthRatios_.append(QList<double>());
    }
    while (inspectionRouteWaypointAllFocalLengthRatios_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllFocalLengthRatios_.removeLast();
    }

    while (inspectionRouteWaypointAllTargetLabels_.size() < inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetLabels_.append(QStringList());
    }
    while (inspectionRouteWaypointAllTargetLabels_.size() > inspectionRouteWaypoints_.size()) {
        inspectionRouteWaypointAllTargetLabels_.removeLast();
    }

    for (int waypointIndex = 0; waypointIndex < inspectionRouteWaypoints_.size(); ++waypointIndex) {
        QList<PointRecord>& allTargetPoints = inspectionRouteWaypointAllTargetPoints_[waypointIndex];
        QList<int>& allTargetPartIndices = inspectionRouteWaypointAllTargetPartIndices_[waypointIndex];
        QList<double>& allCameraYawDegs = inspectionRouteWaypointAllCameraYawDegs_[waypointIndex];
        QList<double>& allCameraPitchDegs = inspectionRouteWaypointAllCameraPitchDegs_[waypointIndex];
        QList<double>& allFocalLengthRatios = inspectionRouteWaypointAllFocalLengthRatios_[waypointIndex];
        QStringList& allTargetLabels = inspectionRouteWaypointAllTargetLabels_[waypointIndex];

        const bool hasLegacyTarget =
            waypointIndex < inspectionRouteWaypointHasTargetPoints_.size()
            && inspectionRouteWaypointHasTargetPoints_.at(waypointIndex)
            && waypointIndex < inspectionRouteWaypointTargetPoints_.size();
        if (allTargetPoints.isEmpty() && hasLegacyTarget) {
            allTargetPoints.append(inspectionRouteWaypointTargetPoints_.at(waypointIndex));
        }

        while (allTargetPartIndices.size() < allTargetPoints.size()) {
            allTargetPartIndices.append(-1);
        }
        while (allTargetPartIndices.size() > allTargetPoints.size()) {
            allTargetPartIndices.removeLast();
        }

        const double legacyCameraYaw = waypointIndex < inspectionRouteWaypointCameraYawDegs_.size()
            ? inspectionRouteWaypointCameraYawDegs_.at(waypointIndex)
            : 0.0;
        const double legacyCameraPitch = waypointIndex < inspectionRouteWaypointCameraPitchDegs_.size()
            ? inspectionRouteWaypointCameraPitchDegs_.at(waypointIndex)
            : 0.0;
        const double legacyFocalLengthRatio = waypointIndex < inspectionRouteWaypointFocalLengthRatios_.size()
            ? normalizedRoutePreviewFocalLengthRatio(inspectionRouteWaypointFocalLengthRatios_.at(waypointIndex))
            : kRoutePreviewDefaultFocalLengthRatio;
        while (allCameraYawDegs.size() < allTargetPoints.size()) {
            allCameraYawDegs.append(legacyCameraYaw);
        }
        while (allCameraYawDegs.size() > allTargetPoints.size()) {
            allCameraYawDegs.removeLast();
        }

        while (allCameraPitchDegs.size() < allTargetPoints.size()) {
            allCameraPitchDegs.append(legacyCameraPitch);
        }
        while (allCameraPitchDegs.size() > allTargetPoints.size()) {
            allCameraPitchDegs.removeLast();
        }

        while (allFocalLengthRatios.size() < allTargetPoints.size()) {
            allFocalLengthRatios.append(legacyFocalLengthRatio);
        }
        while (allFocalLengthRatios.size() > allTargetPoints.size()) {
            allFocalLengthRatios.removeLast();
        }
        for (double& focalLengthRatio : allFocalLengthRatios) {
            focalLengthRatio = normalizedRoutePreviewFocalLengthRatio(focalLengthRatio);
        }

        while (allTargetLabels.size() < allTargetPoints.size()) {
            allTargetLabels.append(tr("Target %1").arg(QLocale().toString(allTargetLabels.size() + 1)));
        }
        while (allTargetLabels.size() > allTargetPoints.size()) {
            allTargetLabels.removeLast();
        }

        if (!allTargetPoints.isEmpty()) {
            inspectionRouteWaypointHasTargetPoints_[waypointIndex] = true;
            inspectionRouteWaypointTargetPoints_[waypointIndex] = allTargetPoints.constFirst();
            inspectionRouteWaypointCameraYawDegs_[waypointIndex] = allCameraYawDegs.isEmpty() ? 0.0 : allCameraYawDegs.constFirst();
            inspectionRouteWaypointCameraPitchDegs_[waypointIndex] = allCameraPitchDegs.isEmpty() ? 0.0 : allCameraPitchDegs.constFirst();
            inspectionRouteWaypointFocalLengthRatios_[waypointIndex] =
                allFocalLengthRatios.isEmpty() ? legacyFocalLengthRatio : allFocalLengthRatios.constFirst();
            if (inspectionRouteWaypointTargetLabels_.at(waypointIndex).trimmed().isEmpty()) {
                inspectionRouteWaypointTargetLabels_[waypointIndex] = allTargetLabels.isEmpty() ? QString() : allTargetLabels.constFirst();
            }
        } else {
            inspectionRouteWaypointHasTargetPoints_[waypointIndex] = false;
            inspectionRouteWaypointTargetPoints_[waypointIndex] = PointRecord();
            inspectionRouteWaypointCameraYawDegs_[waypointIndex] = 0.0;
            inspectionRouteWaypointCameraPitchDegs_[waypointIndex] = 0.0;
            inspectionRouteWaypointFocalLengthRatios_[waypointIndex] = legacyFocalLengthRatio;
        }
    }

    selectedInspectionRouteWaypointIndex_ =
        inspectionRouteWaypoints_.isEmpty()
            ? -1
            : std::clamp(selectedInspectionRouteWaypointIndex_, 0, inspectionRouteWaypoints_.size() - 1);
    selectedInspectionRouteWaypointTargetIndex_ =
        normalizeInspectionRouteWaypointTargetIndex(selectedInspectionRouteWaypointIndex_, selectedInspectionRouteWaypointTargetIndex_);

    refreshInspectionRouteOverlay();
    updateFooter();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
    emit inspectionRouteChanged();
}

void PointCloudViewer::setInspectionRouteWaypoints(const QList<PointRecord>& waypoints, const QStringList& labels)
{
    InspectionRouteDisplayData displayData;
    displayData.waypoints = waypoints;
    displayData.labels = labels;
    setInspectionRouteDisplayData(displayData);
}

RouteLabelDisplayMode PointCloudViewer::inspectionRouteWaypointLabelDisplayMode() const
{
    return routeWaypointLabelDisplayMode_;
}

RouteLabelDisplayMode PointCloudViewer::inspectionRoutePartLabelDisplayMode() const
{
    return routePartLabelDisplayMode_;
}

void PointCloudViewer::setInspectionRouteWaypointLabelDisplayMode(RouteLabelDisplayMode mode)
{
    if (routeWaypointLabelDisplayMode_ == mode) {
        return;
    }

    routeWaypointLabelDisplayMode_ = mode;
    updateInspectionRouteOverlayWidgets();
    updateRouteCameraPreviewOverlay();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::setInspectionRoutePartLabelDisplayMode(RouteLabelDisplayMode mode)
{
    if (routePartLabelDisplayMode_ == mode) {
        return;
    }

    routePartLabelDisplayMode_ = mode;
    updateInspectionRouteOverlayWidgets();
    if (osgWidget_ != nullptr) {
        osgWidget_->update();
    }
}

void PointCloudViewer::setInspectionRouteWaypointColor(const QColor& color)
{
    if (!color.isValid() || inspectionRouteWaypointColor_ == color) {
        return;
    }

    inspectionRouteWaypointColor_ = color;
    refreshInspectionRouteOverlay();
}

void PointCloudViewer::setInspectionRoutePartPointColor(const QColor& color)
{
    if (!color.isValid() || inspectionRoutePartPointColor_ == color) {
        return;
    }

    inspectionRoutePartPointColor_ = color;
    refreshInspectionRouteOverlay();
}

void PointCloudViewer::setInspectionRouteTrajectoryColor(const QColor& color)
{
    if (!color.isValid() || inspectionRouteTrajectoryColor_ == color) {
        return;
    }

    inspectionRouteTrajectoryColor_ = color;
    refreshInspectionRouteOverlay();
}

void PointCloudViewer::clearInspectionRouteWaypoints()
{
    if (inspectionRouteWaypoints_.isEmpty()) {
        return;
    }

    routeRoamStopInternal(false);
    inspectionRouteWaypoints_.clear();
    inspectionRouteLabels_.clear();
    inspectionRoutePartPoints_.clear();
    inspectionRoutePartLabels_.clear();
    inspectionRoutePartPointIndices_.clear();
    inspectionRouteWaypointTargetPoints_.clear();
    inspectionRouteWaypointHasTargetPoints_.clear();
    inspectionRouteWaypointAircraftYawDegs_.clear();
    inspectionRouteWaypointGimbalPitchDegs_.clear();
    inspectionRouteWaypointCameraYawDegs_.clear();
    inspectionRouteWaypointCameraPitchDegs_.clear();
    inspectionRouteWaypointFocalLengthRatios_.clear();
    inspectionRouteWaypointTargetLabels_.clear();
    inspectionRouteWaypointAllTargetPoints_.clear();
    inspectionRouteWaypointAllTargetPartIndices_.clear();
    inspectionRouteWaypointAllCameraYawDegs_.clear();
    inspectionRouteWaypointAllCameraPitchDegs_.clear();
    inspectionRouteWaypointAllFocalLengthRatios_.clear();
    inspectionRouteWaypointAllTargetLabels_.clear();
    inspectionRouteVisible_ = true;
    routeWaypointDragActive_ = false;
    routeWaypointDragIndex_ = -1;
    routeWaypointDragPreviewValid_ = false;
    DataManager::instance().clearTrajectory();
    selectedInspectionRouteWaypointIndex_ = -1;
    selectedInspectionRouteWaypointTargetIndex_ = -1;
    refreshInspectionRouteOverlay();
    updateFooter();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
    emit inspectionRouteChanged();
}

void PointCloudViewer::setSelectedInspectionRouteWaypointIndex(int index)
{
    const int normalizedIndex =
        (index >= 0 && index < inspectionRouteWaypoints_.size()) ? index : -1;
    const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(
        normalizedIndex,
        -1);

    if (selectedInspectionRouteWaypointIndex_ == normalizedIndex
        && selectedInspectionRouteWaypointTargetIndex_ == normalizedTargetIndex) {
        return;
    }

    selectedInspectionRouteWaypointIndex_ = normalizedIndex;
    selectedInspectionRouteWaypointTargetIndex_ = normalizedTargetIndex;
    if (inspectionRouteRoamActive()) {
        updateInspectionRouteOverlayWidgets();
        updateRouteCameraPreviewOverlay();
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
    } else {
        refreshInspectionRouteOverlay();
    }
    updateFooter();
    emit selectedInspectionRouteWaypointChanged(selectedInspectionRouteWaypointIndex_);
}

int PointCloudViewer::selectedInspectionRouteWaypointTargetIndex() const
{
    return selectedInspectionRouteWaypointTargetIndex_;
}

void PointCloudViewer::setSelectedInspectionRouteWaypointTargetIndex(int index)
{
    const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(
        selectedInspectionRouteWaypointIndex_,
        index);
    if (selectedInspectionRouteWaypointTargetIndex_ == normalizedTargetIndex) {
        return;
    }

    selectedInspectionRouteWaypointTargetIndex_ = normalizedTargetIndex;
    if (inspectionRouteRoamActive()) {
        updateInspectionRouteOverlayWidgets();
        updateRouteCameraPreviewOverlay();
        if (osgWidget_ != nullptr) {
            osgWidget_->update();
        }
    } else {
        refreshInspectionRouteOverlay();
    }
    updateFooter();
}

bool PointCloudViewer::inspectionRouteEditingEnabled() const
{
    return inspectionRouteEditingEnabled_;
}

void PointCloudViewer::setInspectionRouteEditingEnabled(bool enabled)
{
    if (inspectionRouteEditingEnabled_ == enabled) {
        return;
    }

    inspectionRouteEditingEnabled_ = enabled;
    if (inspectionRouteEditingEnabled_) {
        return;
    }

    if (osgWidget_ != nullptr) {
        osgWidget_->setSceneDragCaptureEnabled(false);
        osgWidget_->unsetCursor();
    }

    if (routeWaypointDragActive_ || routeWaypointDragPreviewValid_) {
        routeWaypointDragActive_ = false;
        routeWaypointDragIndex_ = -1;
        routeWaypointDragPreviewValid_ = false;
        refreshInspectionRouteOverlay();
    }
}

bool PointCloudViewer::inspectionRouteVisible() const
{
    return inspectionRouteVisible_;
}

void PointCloudViewer::setInspectionRouteVisible(bool visible)
{
    if (inspectionRouteVisible_ == visible) {
        return;
    }

    inspectionRouteVisible_ = visible;
    if (!inspectionRouteVisible_) {
        routeRoamStopInternal(true);
    }
    DataManager::instance().setTrajectory(
        DataManager::instance().trajectoryItem().name.trimmed().isEmpty()
            ? tr("Inspection Route")
            : DataManager::instance().trajectoryItem().name,
        inspectionRouteWaypoints_,
        inspectionRouteVisible_);
    refreshInspectionRouteOverlay();
    updateFooter();
    emit inspectionRouteChanged();
}

QString PointCloudViewer::inspectionRouteWaypointLabelText(int index) const
{
    if (index < 0) {
        return QString();
    }

    if (routeLabelModeHidden(routeWaypointLabelDisplayMode_)) {
        return QString();
    }

    if (routeLabelModeUsesSequence(routeWaypointLabelDisplayMode_)) {
        return QLocale().toString(index + 1);
    }

    if (index < inspectionRouteLabels_.size()) {
        const QString label = inspectionRouteLabels_.at(index).trimmed();
        if (!label.isEmpty()) {
            return label;
        }
    }

    return QLocale().toString(index + 1);
}

QString PointCloudViewer::inspectionRoutePartLabelText(int index) const
{
    if (index < 0) {
        return QString();
    }

    if (routeLabelModeHidden(routePartLabelDisplayMode_)) {
        return QString();
    }

    if (routeLabelModeUsesSequence(routePartLabelDisplayMode_)) {
        return QLocale().toString(index + 1);
    }

    if (index < inspectionRoutePartLabels_.size()) {
        const QString label = inspectionRoutePartLabels_.at(index).trimmed();
        if (!label.isEmpty()) {
            return label;
        }
    }

    return QLocale().toString(index + 1);
}

int PointCloudViewer::normalizeInspectionRouteWaypointTargetIndex(int waypointIndex, int targetIndex) const
{
    if (waypointIndex < 0 || waypointIndex >= inspectionRouteWaypoints_.size()) {
        return -1;
    }

    if (waypointIndex < inspectionRouteWaypointAllTargetPoints_.size()) {
        const QList<PointRecord>& targets = inspectionRouteWaypointAllTargetPoints_.at(waypointIndex);
        if (!targets.isEmpty()) {
            return (targetIndex >= 0 && targetIndex < targets.size()) ? targetIndex : 0;
        }
    }

    const bool hasLegacyTarget =
        waypointIndex < inspectionRouteWaypointHasTargetPoints_.size()
        && inspectionRouteWaypointHasTargetPoints_.at(waypointIndex)
        && waypointIndex < inspectionRouteWaypointTargetPoints_.size();
    return hasLegacyTarget ? 0 : -1;
}
