#include "gui/PointCloudViewer.h"

#include <QLocale>
#include <QTimer>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include <osg/Camera>
#include <osgGA/CameraManipulator>
#include <osgGA/TrackballManipulator>

namespace
{
constexpr double kRouteRoamMinSpeedMetersPerSecond = 0.1;
constexpr double kRouteRoamMaxSpeedMetersPerSecond = 80.0;
constexpr double kRouteRoamDwellSeconds = 0.8;
constexpr double kRouteRoamThirdPersonDistanceMeters = 10.0;
constexpr double kRouteRoamThirdPersonHeightMeters = 3.0;
constexpr double kRouteRoamFirstPersonLookAheadMeters = 18.0;
constexpr double kRouteRoamThirdPersonLookAheadMeters = 8.0;

double routeSegmentLength(const PointRecord& startPoint, const PointRecord& endPoint)
{
    const double dx = static_cast<double>(endPoint.x - startPoint.x);
    const double dy = static_cast<double>(endPoint.y - startPoint.y);
    const double dz = static_cast<double>(endPoint.z - startPoint.z);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double clampRouteRoamSpeed(double speedMetersPerSecond)
{
    return std::clamp(speedMetersPerSecond, kRouteRoamMinSpeedMetersPerSecond, kRouteRoamMaxSpeedMetersPerSecond);
}
}

bool PointCloudViewer::inspectionRouteRoamActive() const
{
    return inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Stopped;
}

bool PointCloudViewer::inspectionRouteRoamPlaying() const
{
    return inspectionRouteRoamPlaybackState_ == InspectionRouteRoamPlaybackState::Playing;
}

bool PointCloudViewer::inspectionRouteRoamPaused() const
{
    return inspectionRouteRoamPlaybackState_ == InspectionRouteRoamPlaybackState::Paused;
}

double PointCloudViewer::inspectionRouteRoamSpeedMetersPerSecond() const
{
    return inspectionRouteRoamSpeedMetersPerSecond_;
}

RouteRoamViewMode PointCloudViewer::inspectionRouteRoamViewMode() const
{
    return inspectionRouteRoamViewMode_;
}

void PointCloudViewer::setInspectionRouteRoamSpeedMetersPerSecond(double speedMetersPerSecond)
{
    const double clampedSpeed = clampRouteRoamSpeed(speedMetersPerSecond);
    if (qFuzzyCompare(inspectionRouteRoamSpeedMetersPerSecond_, clampedSpeed)) {
        return;
    }

    inspectionRouteRoamSpeedMetersPerSecond_ = clampedSpeed;
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::setInspectionRouteRoamViewMode(RouteRoamViewMode mode)
{
    if (inspectionRouteRoamViewMode_ == mode) {
        return;
    }

    inspectionRouteRoamViewMode_ = mode;
    inspectionRouteRoamThirdPersonFollowInitialized_ = false;
    if (inspectionRouteRoamActive()) {
        updateInspectionRouteRoam();
        refreshInspectionRouteOverlay();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::startInspectionRouteRoam(int startWaypointIndex)
{
    if (!hasPointCloud() || inspectionRouteWaypoints_.isEmpty() || !inspectionRouteVisible_) {
        return;
    }

    if (!inspectionRouteRoamActive()) {
        routeRoamCaptureManualView();
    }
    inspectionRouteRoamLastCaptureWaypointIndex_ = -1;
    inspectionRouteRoamCaptureCount_ = 0;
    inspectionRouteRoamCaptureFlashRemainingSeconds_ = 0.0;
    inspectionRouteRoamCurrentPositionValid_ = false;
    inspectionRouteRoamThirdPersonFollowInitialized_ = false;

    if (inspectionRouteWaypoints_.size() == 1) {
        inspectionRouteRoamCurrentSegmentIndex_ = 0;
        inspectionRouteRoamSegmentProgressMeters_ = 0.0;
        inspectionRouteRoamDwelling_ = true;
        inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
        inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Playing;
        refreshInspectionRouteOverlay();
        routeRoamUpdateSelectionState(0);
        osg::Vec3d position;
        osg::Vec3d forward;
        osg::Vec3d up;
        if (routeRoamComputeWaypointPose(0, inspectionRouteWaypoints_, &position, &forward, &up)) {
            routeRoamApplyPose(position, forward, up);
        }
        inspectionRouteRoamLastUpdateTime_ = std::chrono::steady_clock::now();
        if (routeRoamTimer_ != nullptr && !routeRoamTimer_->isActive()) {
            routeRoamTimer_->start();
        }
        updateFooter();
        emit inspectionRouteRoamStateChanged();
        return;
    }

    const int startIndex = std::clamp(startWaypointIndex, 0, inspectionRouteWaypoints_.size() - 1);
    if (startIndex >= inspectionRouteWaypoints_.size() - 1) {
        inspectionRouteRoamCurrentSegmentIndex_ = inspectionRouteWaypoints_.size() - 2;
        inspectionRouteRoamSegmentProgressMeters_ = routeSegmentLength(
            inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_),
            inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_ + 1));
    } else {
        inspectionRouteRoamCurrentSegmentIndex_ = startIndex;
        inspectionRouteRoamSegmentProgressMeters_ = 0.0;
    }

    inspectionRouteRoamDwelling_ = true;
    inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Playing;
    inspectionRouteRoamLastUpdateTime_ = std::chrono::steady_clock::now();

    refreshInspectionRouteOverlay();
    routeRoamUpdateSelectionState(startIndex);
    osg::Vec3d position;
    osg::Vec3d forward;
    osg::Vec3d up;
    if (routeRoamComputeWaypointPose(startIndex, inspectionRouteWaypoints_, &position, &forward, &up)) {
        routeRoamApplyPose(position, forward, up);
    }

    if (routeRoamTimer_ != nullptr && !routeRoamTimer_->isActive()) {
        routeRoamTimer_->start();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::pauseInspectionRouteRoam()
{
    if (inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Playing) {
        return;
    }

    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Paused;
    if (routeRoamTimer_ != nullptr) {
        routeRoamTimer_->stop();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::resumeInspectionRouteRoam()
{
    if (inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Paused) {
        return;
    }

    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Playing;
    inspectionRouteRoamLastUpdateTime_ = std::chrono::steady_clock::now();
    if (routeRoamTimer_ != nullptr && !routeRoamTimer_->isActive()) {
        routeRoamTimer_->start();
    }
    updateFooter();
    emit inspectionRouteRoamStateChanged();
}

void PointCloudViewer::stopInspectionRouteRoam(bool restoreManualView)
{
    routeRoamStopInternal(restoreManualView);
}

void PointCloudViewer::updateInspectionRouteRoam()
{
    if (inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Playing) {
        return;
    }
    if (!hasPointCloud() || !inspectionRouteVisible_ || inspectionRouteWaypoints_.isEmpty()) {
        routeRoamStopInternal(false);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (inspectionRouteRoamLastUpdateTime_.time_since_epoch().count() == 0) {
        inspectionRouteRoamLastUpdateTime_ = now;
        return;
    }

    double remainingSeconds = std::chrono::duration<double>(now - inspectionRouteRoamLastUpdateTime_).count();
    inspectionRouteRoamLastUpdateTime_ = now;
    remainingSeconds = std::clamp(remainingSeconds, 0.0, 0.25);
    if (inspectionRouteRoamCaptureFlashRemainingSeconds_ > 0.0) {
        const double previousFlashSeconds = inspectionRouteRoamCaptureFlashRemainingSeconds_;
        inspectionRouteRoamCaptureFlashRemainingSeconds_ =
            std::max(0.0, inspectionRouteRoamCaptureFlashRemainingSeconds_ - remainingSeconds);
        if (previousFlashSeconds > 0.0 && inspectionRouteRoamCaptureFlashRemainingSeconds_ <= 0.0) {
            updateRouteCameraPreviewOverlay();
        }
    }

    const int waypointCount = inspectionRouteWaypoints_.size();
    if (waypointCount == 1) {
        osg::Vec3d position;
        osg::Vec3d forward;
        osg::Vec3d up;
        if (routeRoamComputeWaypointPose(0, inspectionRouteWaypoints_, &position, &forward, &up)) {
            routeRoamApplyPose(position, forward, up);
        }
        routeRoamUpdateSelectionState(0);
        if (inspectionRouteRoamDwelling_) {
            inspectionRouteRoamDwellRemainingSeconds_ = std::max(0.0, inspectionRouteRoamDwellRemainingSeconds_ - remainingSeconds);
            if (inspectionRouteRoamDwellRemainingSeconds_ <= 0.0) {
                routeRoamStopInternal(false);
            }
        }
        return;
    }

    while (remainingSeconds > 0.0) {
        const int segmentIndex = std::clamp(inspectionRouteRoamCurrentSegmentIndex_, 0, waypointCount - 2);
        const PointRecord& startPoint = inspectionRouteWaypoints_.at(segmentIndex);
        const PointRecord& endPoint = inspectionRouteWaypoints_.at(segmentIndex + 1);
        const double segmentLength = routeSegmentLength(startPoint, endPoint);

        if (inspectionRouteRoamDwelling_) {
            const double consumed = std::min(remainingSeconds, inspectionRouteRoamDwellRemainingSeconds_);
            inspectionRouteRoamDwellRemainingSeconds_ -= consumed;
            remainingSeconds -= consumed;
            if (inspectionRouteRoamDwellRemainingSeconds_ <= 0.0) {
                inspectionRouteRoamDwelling_ = false;
            } else {
                break;
            }
            continue;
        }

        if (segmentLength <= 0.0001) {
            inspectionRouteRoamCurrentSegmentIndex_ = segmentIndex + 1;
            inspectionRouteRoamSegmentProgressMeters_ = 0.0;
            routeRoamUpdateSelectionState(inspectionRouteRoamCurrentSegmentIndex_);
            if (inspectionRouteRoamCurrentSegmentIndex_ >= waypointCount - 1) {
                routeRoamStopInternal(false);
                return;
            }
            inspectionRouteRoamDwelling_ = true;
            inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
            continue;
        }

        const double clampedSpeed = clampRouteRoamSpeed(inspectionRouteRoamSpeedMetersPerSecond_);
        const double remainOnSegment = std::max(0.0, segmentLength - inspectionRouteRoamSegmentProgressMeters_);
        const double travelDistance = clampedSpeed * remainingSeconds;
        if (travelDistance < remainOnSegment) {
            inspectionRouteRoamSegmentProgressMeters_ += travelDistance;
            remainingSeconds = 0.0;
            break;
        }

        const double timeUsed = remainOnSegment / clampedSpeed;
        remainingSeconds = std::max(0.0, remainingSeconds - timeUsed);
        inspectionRouteRoamCurrentSegmentIndex_ = segmentIndex + 1;
        inspectionRouteRoamSegmentProgressMeters_ = 0.0;
        routeRoamUpdateSelectionState(inspectionRouteRoamCurrentSegmentIndex_);
        if (inspectionRouteRoamCurrentSegmentIndex_ >= waypointCount - 1) {
            routeRoamStopInternal(false);
            return;
        }
        inspectionRouteRoamDwelling_ = true;
        inspectionRouteRoamDwellRemainingSeconds_ = kRouteRoamDwellSeconds;
    }

    int poseWaypointIndex = std::clamp(inspectionRouteRoamCurrentSegmentIndex_, 0, waypointCount - 1);
    const PointRecord& poseWaypoint = inspectionRouteWaypoints_.at(poseWaypointIndex);
    osg::Vec3d interpolatedPosition(
        static_cast<double>(poseWaypoint.x),
        static_cast<double>(poseWaypoint.y),
        static_cast<double>(poseWaypoint.z));
    double orientationBlendFactor = 0.0;
    if (inspectionRouteRoamCurrentSegmentIndex_ < waypointCount - 1) {
        const PointRecord& segmentStart = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_);
        const PointRecord& segmentEnd = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_ + 1);
        const double segmentLength = routeSegmentLength(segmentStart, segmentEnd);
        if (segmentLength > 0.0001) {
            const double t = std::clamp(inspectionRouteRoamSegmentProgressMeters_ / segmentLength, 0.0, 1.0);
            orientationBlendFactor = t;
            interpolatedPosition.set(
                static_cast<double>(segmentStart.x)
                    + (static_cast<double>(segmentEnd.x) - static_cast<double>(segmentStart.x)) * t,
                static_cast<double>(segmentStart.y)
                    + (static_cast<double>(segmentEnd.y) - static_cast<double>(segmentStart.y)) * t,
                static_cast<double>(segmentStart.z)
                    + (static_cast<double>(segmentEnd.z) - static_cast<double>(segmentStart.z)) * t);
        }
    }

    osg::Vec3d waypointPosition;
    osg::Vec3d forward;
    osg::Vec3d up;
    if (!routeRoamComputeWaypointPose(poseWaypointIndex, inspectionRouteWaypoints_, &waypointPosition, &forward, &up)) {
        return;
    }
    if (inspectionRouteRoamCurrentSegmentIndex_ < waypointCount - 1 && orientationBlendFactor > 0.0001) {
        if (inspectionRouteRoamViewMode_ == RouteRoamViewMode::ThirdPerson) {
            osg::Vec3d nextWaypointPosition;
            osg::Vec3d nextForward;
            osg::Vec3d nextUp;
            if (routeRoamComputeWaypointPose(
                    inspectionRouteRoamCurrentSegmentIndex_ + 1,
                    inspectionRouteWaypoints_,
                    &nextWaypointPosition,
                    &nextForward,
                    &nextUp)) {
                const double blend = std::clamp(orientationBlendFactor, 0.0, 1.0);
                osg::Vec3d blendedForward = forward * (1.0 - blend) + nextForward * blend;
                if (blendedForward.length2() > 0.000001) {
                    blendedForward.normalize();
                    forward = blendedForward;
                }

                osg::Vec3d blendedUp = up * (1.0 - blend) + nextUp * blend;
                if (blendedUp.length2() > 0.000001) {
                    blendedUp.normalize();
                    up = blendedUp;
                } else {
                    up = osg::Vec3d(0.0, 0.0, 1.0);
                }

                osg::Vec3d right = forward ^ up;
                if (right.length2() > 0.000001) {
                    right.normalize();
                    up = right ^ forward;
                    up.normalize();
                }
            }
        }
    }

    if (inspectionRouteRoamViewMode_ == RouteRoamViewMode::FirstPerson
        && !inspectionRouteRoamDwelling_
        && inspectionRouteRoamCurrentSegmentIndex_ < waypointCount - 1) {
        const PointRecord& segmentStart = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_);
        const PointRecord& segmentEnd = inspectionRouteWaypoints_.at(inspectionRouteRoamCurrentSegmentIndex_ + 1);
        osg::Vec3d segmentForward(
            static_cast<double>(segmentEnd.x - segmentStart.x),
            static_cast<double>(segmentEnd.y - segmentStart.y),
            static_cast<double>(segmentEnd.z - segmentStart.z));
        if (segmentForward.length2() > 0.000001) {
            segmentForward.normalize();
            forward = segmentForward;

            osg::Vec3d worldUp(0.0, 0.0, 1.0);
            osg::Vec3d right = forward ^ worldUp;
            if (right.length2() <= 0.000001) {
                worldUp = osg::Vec3d(0.0, 1.0, 0.0);
                right = forward ^ worldUp;
            }
            right.normalize();
            up = right ^ forward;
            up.normalize();
        }
    }

    routeRoamApplyPose(interpolatedPosition, forward, up);
}

void PointCloudViewer::routeRoamUpdateSelectionState(int waypointIndex)
{
    const int normalizedTargetIndex = normalizeInspectionRouteWaypointTargetIndex(waypointIndex, 0);
    setSelectedInspectionRouteWaypointIndex(waypointIndex);
    setSelectedInspectionRouteWaypointTargetIndex(normalizedTargetIndex);
    if (inspectionRouteRoamPlaybackState_ == InspectionRouteRoamPlaybackState::Playing) {
        routeRoamTriggerPhotoCapture(waypointIndex, normalizedTargetIndex);
    }
}

void PointCloudViewer::routeRoamTriggerPhotoCapture(int waypointIndex, int targetIndex)
{
    if (waypointIndex < 0
        || waypointIndex >= inspectionRouteWaypoints_.size()
        || waypointIndex == inspectionRouteRoamLastCaptureWaypointIndex_) {
        return;
    }

    inspectionRouteRoamLastCaptureWaypointIndex_ = waypointIndex;
    ++inspectionRouteRoamCaptureCount_;
    inspectionRouteRoamCaptureFlashRemainingSeconds_ = 0.35;

    QString targetLabel;
    if (waypointIndex < inspectionRouteWaypointAllTargetLabels_.size()
        && targetIndex >= 0
        && targetIndex < inspectionRouteWaypointAllTargetLabels_.at(waypointIndex).size()) {
        targetLabel = inspectionRouteWaypointAllTargetLabels_.at(waypointIndex).at(targetIndex).trimmed();
    }
    if (targetLabel.isEmpty() && targetIndex >= 0) {
        targetLabel = tr("Target %1").arg(QLocale().toString(targetIndex + 1));
    }
    if (targetLabel.isEmpty()) {
        targetLabel = tr("Unlinked");
    }

    updateRouteCameraPreviewOverlay();
    updateFooter();
    emit inspectionRouteRoamPhotoCaptured(
        waypointIndex,
        targetIndex,
        targetLabel,
        inspectionRouteRoamCaptureCount_);
}

bool PointCloudViewer::routeRoamComputeWaypointPose(
    int waypointIndex,
    const QList<PointRecord>& waypoints,
    osg::Vec3d* position,
    osg::Vec3d* forward,
    osg::Vec3d* up) const
{
    if (position == nullptr || forward == nullptr || up == nullptr) {
        return false;
    }
    if (waypointIndex < 0 || waypointIndex >= waypoints.size()) {
        return false;
    }

    const PointRecord& waypoint = waypoints.at(waypointIndex);
    *position = osg::Vec3d(
        static_cast<double>(waypoint.x),
        static_cast<double>(waypoint.y),
        static_cast<double>(waypoint.z));

    const double aircraftYawDeg = waypointIndex < inspectionRouteWaypointAircraftYawDegs_.size()
        ? inspectionRouteWaypointAircraftYawDegs_.at(waypointIndex)
        : 0.0;
    const double gimbalPitchDeg = waypointIndex < inspectionRouteWaypointGimbalPitchDegs_.size()
        ? inspectionRouteWaypointGimbalPitchDegs_.at(waypointIndex)
        : 0.0;
    const int targetIndex = normalizeInspectionRouteWaypointTargetIndex(waypointIndex, 0);
    const QList<double> cameraYawCandidates = waypointIndex < inspectionRouteWaypointAllCameraYawDegs_.size()
        ? inspectionRouteWaypointAllCameraYawDegs_.at(waypointIndex)
        : QList<double>();
    const QList<double> cameraPitchCandidates = waypointIndex < inspectionRouteWaypointAllCameraPitchDegs_.size()
        ? inspectionRouteWaypointAllCameraPitchDegs_.at(waypointIndex)
        : QList<double>();
    const double cameraYawDeg =
        (targetIndex >= 0 && targetIndex < cameraYawCandidates.size())
            ? cameraYawCandidates.at(targetIndex)
            : (waypointIndex < inspectionRouteWaypointCameraYawDegs_.size()
                ? inspectionRouteWaypointCameraYawDegs_.at(waypointIndex)
                : 0.0);
    const double cameraPitchDeg =
        (targetIndex >= 0 && targetIndex < cameraPitchCandidates.size())
            ? cameraPitchCandidates.at(targetIndex)
            : (waypointIndex < inspectionRouteWaypointCameraPitchDegs_.size()
                ? inspectionRouteWaypointCameraPitchDegs_.at(waypointIndex)
                : 0.0);

    const double yawRadians = qDegreesToRadians(aircraftYawDeg + cameraYawDeg);
    const double pitchRadians = qDegreesToRadians(gimbalPitchDeg + cameraPitchDeg);
    osg::Vec3d routeForward(
        std::sin(yawRadians) * std::cos(pitchRadians),
        std::cos(yawRadians) * std::cos(pitchRadians),
        std::sin(pitchRadians));

    if (routeForward.length2() <= 0.00001) {
        const bool hasNextWaypoint = waypointIndex + 1 < waypoints.size();
        const bool hasPrevWaypoint = waypointIndex - 1 >= 0;
        if (hasNextWaypoint) {
            const PointRecord& nextPoint = waypoints.at(waypointIndex + 1);
            routeForward = osg::Vec3d(
                static_cast<double>(nextPoint.x - waypoint.x),
                static_cast<double>(nextPoint.y - waypoint.y),
                static_cast<double>(nextPoint.z - waypoint.z));
        } else if (hasPrevWaypoint) {
            const PointRecord& prevPoint = waypoints.at(waypointIndex - 1);
            routeForward = osg::Vec3d(
                static_cast<double>(waypoint.x - prevPoint.x),
                static_cast<double>(waypoint.y - prevPoint.y),
                static_cast<double>(waypoint.z - prevPoint.z));
        }
        if (routeForward.length2() <= 0.00001) {
            routeForward = osg::Vec3d(0.0, 1.0, 0.0);
        }
    }
    routeForward.normalize();

    osg::Vec3d worldUp(0.0, 0.0, 1.0);
    osg::Vec3d right = routeForward ^ worldUp;
    if (right.length2() <= 0.00001) {
        worldUp = osg::Vec3d(0.0, 1.0, 0.0);
        right = routeForward ^ worldUp;
    }
    right.normalize();
    osg::Vec3d routeUp = right ^ routeForward;
    routeUp.normalize();

    *forward = routeForward;
    *up = routeUp;
    return true;
}

bool PointCloudViewer::routeRoamApplyPose(
    const osg::Vec3d& position,
    const osg::Vec3d& forward,
    const osg::Vec3d& up)
{
    if (osgWidget_ == nullptr) {
        return false;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCameraManipulator() == nullptr) {
        return false;
    }

    osgGA::CameraManipulator* manipulator = viewer->getCameraManipulator();

    const bool positionChanged = !inspectionRouteRoamCurrentPositionValid_
        || (position - inspectionRouteRoamCurrentPosition_).length2() > 0.000001;
    inspectionRouteRoamCurrentPosition_ = position;
    inspectionRouteRoamCurrentPositionValid_ = true;

    if (inspectionRouteRoamViewMode_ == RouteRoamViewMode::FirstPerson) {
        inspectionRouteRoamThirdPersonFollowInitialized_ = false;
        const osg::Vec3d eye = position;
        const osg::Vec3d center = position + forward * kRouteRoamFirstPersonLookAheadMeters;
        if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
            trackball->setTransformation(eye, center, up);
        } else {
            manipulator->setHomePosition(eye, center, up, false);
            manipulator->home(0.0);
        }
    } else {
        if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
            if (!inspectionRouteRoamThirdPersonFollowInitialized_) {
                const osg::Vec3d eye = position - forward * kRouteRoamThirdPersonDistanceMeters + up * kRouteRoamThirdPersonHeightMeters;
                const osg::Vec3d center = position + forward * kRouteRoamThirdPersonLookAheadMeters;
                trackball->setTransformation(eye, center, up);
                inspectionRouteRoamThirdPersonFollowInitialized_ = true;
                inspectionRouteRoamLastFollowPosition_ = position;
            } else {
                osg::Vec3d currentEye;
                osg::Vec3d currentCenter;
                osg::Vec3d currentUp;
                trackball->getTransformation(currentEye, currentCenter, currentUp);
                const osg::Vec3d followDelta = position - inspectionRouteRoamLastFollowPosition_;
                if (followDelta.length2() > 0.000001) {
                    trackball->setTransformation(currentEye + followDelta, currentCenter + followDelta, currentUp);
                }
                inspectionRouteRoamLastFollowPosition_ = position;
            }
        } else {
            const osg::Vec3d eye = position - forward * kRouteRoamThirdPersonDistanceMeters + up * kRouteRoamThirdPersonHeightMeters;
            const osg::Vec3d center = position + forward * kRouteRoamThirdPersonLookAheadMeters;
            manipulator->setHomePosition(eye, center, up, false);
            manipulator->home(0.0);
        }
    }

    if (positionChanged && inspectionRouteRoamPlaybackState_ != InspectionRouteRoamPlaybackState::Stopped) {
        refreshInspectionRouteOverlay();
    }

    osgWidget_->update();
    return true;
}

void PointCloudViewer::routeRoamCaptureManualView()
{
    if (inspectionRouteRoamManualViewCaptured_ || osgWidget_ == nullptr) {
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCameraManipulator() == nullptr) {
        return;
    }

    osgGA::CameraManipulator* manipulator = viewer->getCameraManipulator();
    osg::Vec3d eye;
    osg::Vec3d center;
    osg::Vec3d up;
    if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
        trackball->getTransformation(eye, center, up);
    } else if (viewer->getCamera() != nullptr) {
        viewer->getCamera()->getViewMatrixAsLookAt(eye, center, up);
    } else {
        return;
    }

    inspectionRouteRoamSavedEye_ = eye;
    inspectionRouteRoamSavedCenter_ = center;
    inspectionRouteRoamSavedUp_ = up;
    inspectionRouteRoamSavedManipulator_ = manipulator;
    inspectionRouteRoamManualViewCaptured_ = true;
}

void PointCloudViewer::routeRoamRestoreManualView()
{
    if (!inspectionRouteRoamManualViewCaptured_ || osgWidget_ == nullptr) {
        return;
    }

    osgViewer::Viewer* viewer = osgWidget_->getViewer();
    if (viewer == nullptr || viewer->getCameraManipulator() == nullptr) {
        inspectionRouteRoamManualViewCaptured_ = false;
        inspectionRouteRoamSavedManipulator_ = nullptr;
        return;
    }

    osgGA::CameraManipulator* manipulator = viewer->getCameraManipulator();
    if (auto* trackball = dynamic_cast<osgGA::TrackballManipulator*>(manipulator)) {
        trackball->setTransformation(
            inspectionRouteRoamSavedEye_,
            inspectionRouteRoamSavedCenter_,
            inspectionRouteRoamSavedUp_);
    } else {
        manipulator->setHomePosition(
            inspectionRouteRoamSavedEye_,
            inspectionRouteRoamSavedCenter_,
            inspectionRouteRoamSavedUp_,
            false);
        manipulator->home(0.0);
    }

    inspectionRouteRoamManualViewCaptured_ = false;
    inspectionRouteRoamSavedManipulator_ = nullptr;
    osgWidget_->update();
}

void PointCloudViewer::routeRoamStopInternal(bool restoreManualView)
{
    const bool wasActive = inspectionRouteRoamActive();
    if (routeRoamTimer_ != nullptr) {
        routeRoamTimer_->stop();
    }

    inspectionRouteRoamPlaybackState_ = InspectionRouteRoamPlaybackState::Stopped;
    inspectionRouteRoamCurrentSegmentIndex_ = 0;
    inspectionRouteRoamSegmentProgressMeters_ = 0.0;
    inspectionRouteRoamDwelling_ = false;
    inspectionRouteRoamDwellRemainingSeconds_ = 0.0;
    inspectionRouteRoamLastCaptureWaypointIndex_ = -1;
    inspectionRouteRoamCaptureCount_ = 0;
    inspectionRouteRoamCaptureFlashRemainingSeconds_ = 0.0;
    inspectionRouteRoamLastUpdateTime_ = {};
    inspectionRouteRoamCurrentPositionValid_ = false;
    inspectionRouteRoamThirdPersonFollowInitialized_ = false;

    if (restoreManualView) {
        routeRoamRestoreManualView();
    }

    if (wasActive) {
        refreshInspectionRouteOverlay();
        updateFooter();
        emit inspectionRouteRoamStateChanged();
    }
}
