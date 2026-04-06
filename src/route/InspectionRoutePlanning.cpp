#include "route/InspectionRoutePlanning.h"

#include <QHash>
#include <QPointF>
#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <limits>

#include "crs/CrsAuthorityService.h"
#include "crs/CrsTransformService.h"
#include "crs/CrsTypes.h"

namespace
{
struct RouteSamplePoint
{
    PointRecord point;
    int sourceRiskIndex = -1;
    float chainage = 0.0f;
};

float distance3d(const PointRecord& left, const PointRecord& right)
{
    const double dx = static_cast<double>(right.x) - static_cast<double>(left.x);
    const double dy = static_cast<double>(right.y) - static_cast<double>(left.y);
    const double dz = static_cast<double>(right.z) - static_cast<double>(left.z);
    return static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
}

PointRecord interpolatePoint(const PointRecord& start, const PointRecord& end, float t)
{
    const float clampedT = std::clamp(t, 0.0f, 1.0f);
    PointRecord point = start;
    point.x = start.x + (end.x - start.x) * clampedT;
    point.y = start.y + (end.y - start.y) * clampedT;
    point.z = start.z + (end.z - start.z) * clampedT;
    return point;
}

float nearestTowerDistance(const PointRecord& point, const QList<TowerRecord>& towers)
{
    if (towers.isEmpty()) {
        return std::numeric_limits<float>::max();
    }

    float minDistance = std::numeric_limits<float>::max();
    for (const TowerRecord& tower : towers) {
        minDistance = std::min(minDistance, distance3d(point, tower.point));
    }
    return minDistance;
}

bool hasMeaningfulPoint(const PointRecord& point)
{
    return !qFuzzyIsNull(static_cast<double>(point.x))
        || !qFuzzyIsNull(static_cast<double>(point.y))
        || !qFuzzyIsNull(static_cast<double>(point.z));
}

double wrappedAngleDeltaDeg(double leftDeg, double rightDeg)
{
    double delta = std::fmod(rightDeg - leftDeg, 360.0);
    if (delta > 180.0) {
        delta -= 360.0;
    } else if (delta < -180.0) {
        delta += 360.0;
    }
    return std::abs(delta);
}

double maxWaypointSpeedForProfile(DjiAircraftProfile profile)
{
    switch (profile) {
    case DjiAircraftProfile::M3ESeries:
        return 12.0;
    case DjiAircraftProfile::M300Series:
        return 17.0;
    case DjiAircraftProfile::M30Series:
    default:
        return 15.0;
    }
}

bool resolveTargetPoint(
    const RouteCaptureTarget& target,
    const QHash<int, RoutePartPoint>& partPointByIndex,
    PointRecord* outputPoint)
{
    if (outputPoint == nullptr) {
        return false;
    }

    if (target.partIndex > 0 && partPointByIndex.contains(target.partIndex)) {
        *outputPoint = partPointByIndex.value(target.partIndex).localPoint;
        return true;
    }

    if (hasMeaningfulPoint(target.targetLocalPoint)) {
        *outputPoint = target.targetLocalPoint;
        return true;
    }

    return false;
}

int routeQaSeverityRank(RouteQaSeverity severity)
{
    switch (severity) {
    case RouteQaSeverity::Blocking:
        return 3;
    case RouteQaSeverity::Warning:
        return 2;
    case RouteQaSeverity::Info:
    default:
        return 1;
    }
}

void appendRouteQaIssue(
    RouteQaReport* report,
    RouteQaSeverity severity,
    RouteQaIssueType issueType,
    int waypointIndex,
    int relatedWaypointIndex,
    int partIndex,
    int targetIndex,
    const QString& message,
    const QString& detail)
{
    if (report == nullptr) {
        return;
    }

    RouteQaIssue issue;
    issue.severity = severity;
    issue.type = issueType;
    issue.waypointIndex = waypointIndex;
    issue.relatedWaypointIndex = relatedWaypointIndex;
    issue.partIndex = partIndex;
    issue.targetIndex = targetIndex;
    issue.message = message;
    issue.detail = detail;
    report->issues.append(issue);

    switch (severity) {
    case RouteQaSeverity::Blocking:
        ++report->blockingIssueCount;
        break;
    case RouteQaSeverity::Warning:
        ++report->warningIssueCount;
        break;
    case RouteQaSeverity::Info:
    default:
        ++report->infoIssueCount;
        break;
    }
}

QList<RouteSamplePoint> smoothPolyline(const QList<RouteSamplePoint>& points, float smoothingStrengthPercent)
{
    if (points.size() < 3 || smoothingStrengthPercent <= 0.0f) {
        return points;
    }

    const float alpha = std::clamp(smoothingStrengthPercent / 100.0f, 0.0f, 1.0f);
    QList<RouteSamplePoint> output = points;
    for (int index = 1; index < points.size() - 1; ++index) {
        const PointRecord& previous = points.at(index - 1).point;
        const PointRecord& current = points.at(index).point;
        const PointRecord& next = points.at(index + 1).point;

        const float averageX = (previous.x + current.x + next.x) / 3.0f;
        const float averageY = (previous.y + current.y + next.y) / 3.0f;
        const float averageZ = (previous.z + current.z + next.z) / 3.0f;

        output[index].point.x = current.x + (averageX - current.x) * alpha;
        output[index].point.y = current.y + (averageY - current.y) * alpha;
        output[index].point.z = current.z + (averageZ - current.z) * alpha;
    }
    return output;
}

QList<RouteSamplePoint> resampleBySpacing(const QList<RouteSamplePoint>& points, float spacingMeters)
{
    if (points.size() < 2 || spacingMeters <= 0.01f) {
        return points;
    }

    QList<RouteSamplePoint> sampled;
    sampled.reserve(points.size());
    sampled.append(points.first());

    float carriedDistance = 0.0f;
    RouteSamplePoint segmentStart = points.first();
    for (int index = 1; index < points.size(); ++index) {
        RouteSamplePoint segmentEnd = points.at(index);
        float segmentLength = distance3d(segmentStart.point, segmentEnd.point);

        while (segmentLength > 0.0f && (carriedDistance + segmentLength) >= spacingMeters) {
            const float remain = spacingMeters - carriedDistance;
            const float t = remain / segmentLength;

            RouteSamplePoint sampledPoint;
            sampledPoint.point = interpolatePoint(segmentStart.point, segmentEnd.point, t);
            sampledPoint.sourceRiskIndex = segmentStart.sourceRiskIndex;
            sampledPoint.chainage = sampled.last().chainage + spacingMeters;
            sampled.append(sampledPoint);

            segmentStart.point = sampledPoint.point;
            segmentLength = distance3d(segmentStart.point, segmentEnd.point);
            carriedDistance = 0.0f;
        }

        carriedDistance += segmentLength;
        segmentStart = segmentEnd;
    }

    if (distance3d(sampled.last().point, points.last().point) > 0.01f) {
        RouteSamplePoint lastPoint = points.last();
        lastPoint.chainage = sampled.last().chainage + distance3d(sampled.last().point, points.last().point);
        sampled.append(lastPoint);
    }

    return sampled;
}

bool transformPointEpsg(
    double x,
    double y,
    int sourceEpsg,
    int targetEpsg,
    double* outX,
    double* outY,
    QString* errorMessage)
{
    if (outX == nullptr || outY == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Coordinate output pointer is null.");
        }
        return false;
    }

    if (sourceEpsg <= 0 || targetEpsg <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Invalid EPSG code. Please set source/target EPSG.");
        }
        return false;
    }

    if (sourceEpsg == targetEpsg) {
        *outX = x;
        *outY = y;
        return true;
    }

    lasviewer::crs::CoordinateSystemRef source;
    source.authName = QStringLiteral("EPSG");
    source.code = sourceEpsg;
    source.kind = lasviewer::crs::CoordinateSystemKind::Projected;
    lasviewer::crs::CoordinateSystemRef normalizedSource;
    if (lasviewer::crs::CrsAuthorityService::normalizeCoordinateSystem(source, &normalizedSource, nullptr)) {
        source = normalizedSource;
    }

    lasviewer::crs::CoordinateSystemRef target;
    target.authName = QStringLiteral("EPSG");
    target.code = targetEpsg;
    target.kind = targetEpsg == 4326
        ? lasviewer::crs::CoordinateSystemKind::Geographic
        : lasviewer::crs::CoordinateSystemKind::Projected;
    lasviewer::crs::CoordinateSystemRef normalizedTarget;
    if (lasviewer::crs::CrsAuthorityService::normalizeCoordinateSystem(target, &normalizedTarget, nullptr)) {
        target = normalizedTarget;
    }

    QPointF output;
    if (!lasviewer::crs::CrsTransformService::transformPoint(
            source,
            target,
            QPointF(x, y),
            &output,
            errorMessage)) {
        return false;
    }

    *outX = output.x();
    *outY = output.y();
    return true;
}

QJsonObject generationOptionsToJson(const RouteGenerationOptions& options)
{
    return QJsonObject {
        { QStringLiteral("waypointSpacingMeters"), options.waypointSpacingMeters },
        { QStringLiteral("smoothingStrengthPercent"), options.smoothingStrengthPercent }
    };
}

RouteGenerationOptions generationOptionsFromJson(const QJsonObject& object)
{
    RouteGenerationOptions options;
    options.waypointSpacingMeters = static_cast<float>(
        object.value(QStringLiteral("waypointSpacingMeters")).toDouble(options.waypointSpacingMeters));
    options.smoothingStrengthPercent = static_cast<float>(
        object.value(QStringLiteral("smoothingStrengthPercent")).toDouble(options.smoothingStrengthPercent));
    return options;
}

QJsonObject safetyOptionsToJson(const RouteSafetyOptions& options)
{
    return QJsonObject {
        { QStringLiteral("safetyHeightMeters"), options.safetyHeightMeters },
        { QStringLiteral("heightOffsetMeters"), options.heightOffsetMeters },
        { QStringLiteral("defaultWaypointSpeedMps"), options.defaultWaypointSpeedMps },
        { QStringLiteral("globalTransitionalSpeedMps"), options.globalTransitionalSpeedMps },
        { QStringLiteral("globalRthHeightMeters"), options.globalRthHeightMeters },
        { QStringLiteral("defaultGimbalPitchDeg"), options.defaultGimbalPitchDeg }
    };
}

RouteSafetyOptions safetyOptionsFromJson(const QJsonObject& object)
{
    RouteSafetyOptions options;
    options.safetyHeightMeters = static_cast<float>(
        object.value(QStringLiteral("safetyHeightMeters")).toDouble(options.safetyHeightMeters));
    options.heightOffsetMeters = static_cast<float>(
        object.value(QStringLiteral("heightOffsetMeters")).toDouble(options.heightOffsetMeters));
    options.defaultWaypointSpeedMps = static_cast<float>(
        object.value(QStringLiteral("defaultWaypointSpeedMps")).toDouble(options.defaultWaypointSpeedMps));
    options.globalTransitionalSpeedMps = static_cast<float>(
        object.value(QStringLiteral("globalTransitionalSpeedMps")).toDouble(options.globalTransitionalSpeedMps));
    options.globalRthHeightMeters = static_cast<float>(
        object.value(QStringLiteral("globalRthHeightMeters")).toDouble(options.globalRthHeightMeters));
    options.defaultGimbalPitchDeg = static_cast<float>(
        object.value(QStringLiteral("defaultGimbalPitchDeg")).toDouble(options.defaultGimbalPitchDeg));
    return options;
}

QJsonObject crsOptionsToJson(const CrsTransformOptions& options)
{
    return QJsonObject {
        { QStringLiteral("sourceEpsg"), options.sourceEpsg },
        { QStringLiteral("targetEpsg"), options.targetEpsg }
    };
}

CrsTransformOptions crsOptionsFromJson(const QJsonObject& object)
{
    CrsTransformOptions options;
    options.sourceEpsg = object.value(QStringLiteral("sourceEpsg")).toInt(options.sourceEpsg);
    options.targetEpsg = object.value(QStringLiteral("targetEpsg")).toInt(options.targetEpsg);
    return options;
}
}

QString djiAircraftProfileDisplayName(DjiAircraftProfile profile)
{
    switch (profile) {
    case DjiAircraftProfile::M3ESeries:
        return QCoreApplication::translate("InspectionRoutePlanning", "DJI M3E/M3T");
    case DjiAircraftProfile::M300Series:
        return QCoreApplication::translate("InspectionRoutePlanning", "DJI M300 RTK");
    case DjiAircraftProfile::M30Series:
    default:
        return QCoreApplication::translate("InspectionRoutePlanning", "DJI M30/M30T");
    }
}

DjiAircraftProfileMapping djiAircraftProfileMapping(DjiAircraftProfile profile)
{
    switch (profile) {
    case DjiAircraftProfile::M3ESeries:
        return { 77, 0, 66, 0 };
    case DjiAircraftProfile::M300Series:
        return { 60, 0, 43, 0 };
    case DjiAircraftProfile::M30Series:
    default:
        return { 67, 0, 52, 0 };
    }
}

QList<DjiAircraftProfile> supportedDjiAircraftProfiles()
{
    return { DjiAircraftProfile::M30Series, DjiAircraftProfile::M3ESeries, DjiAircraftProfile::M300Series };
}

bool RouteQaReport::hasBlockingIssues() const
{
    return blockingIssueCount > 0;
}

bool RouteQaReport::hasWarnings() const
{
    return warningIssueCount > 0;
}

RouteQaThresholds defaultRouteQaThresholds()
{
    return RouteQaThresholds();
}

QString routeQaSeverityDisplayName(RouteQaSeverity severity)
{
    switch (severity) {
    case RouteQaSeverity::Blocking:
        return QCoreApplication::translate("InspectionRoutePlanning", "Blocking");
    case RouteQaSeverity::Warning:
        return QCoreApplication::translate("InspectionRoutePlanning", "Warning");
    case RouteQaSeverity::Info:
    default:
        return QCoreApplication::translate("InspectionRoutePlanning", "Info");
    }
}

QString routeQaIssueTypeDisplayName(RouteQaIssueType issueType)
{
    switch (issueType) {
    case RouteQaIssueType::WaypointCountInsufficient:
        return QCoreApplication::translate("InspectionRoutePlanning", "Waypoint Count");
    case RouteQaIssueType::WaypointSpacingTooSmall:
        return QCoreApplication::translate("InspectionRoutePlanning", "Spacing Too Small");
    case RouteQaIssueType::WaypointSpacingTooLarge:
        return QCoreApplication::translate("InspectionRoutePlanning", "Spacing Too Large");
    case RouteQaIssueType::TargetDistanceTooNear:
        return QCoreApplication::translate("InspectionRoutePlanning", "Target Too Near");
    case RouteQaIssueType::TargetDistanceTooFar:
        return QCoreApplication::translate("InspectionRoutePlanning", "Target Too Far");
    case RouteQaIssueType::AttitudeJumpTooLarge:
        return QCoreApplication::translate("InspectionRoutePlanning", "Attitude Jump");
    case RouteQaIssueType::HelperWaypointMissing:
        return QCoreApplication::translate("InspectionRoutePlanning", "Helper Waypoint");
    case RouteQaIssueType::MissingPartCoverage:
        return QCoreApplication::translate("InspectionRoutePlanning", "Missing Coverage");
    case RouteQaIssueType::DuplicatePartCoverage:
        return QCoreApplication::translate("InspectionRoutePlanning", "Duplicate Coverage");
    case RouteQaIssueType::UnsupportedActionCombination:
    default:
        return QCoreApplication::translate("InspectionRoutePlanning", "Unsupported Combination");
    }
}

RouteQaReport evaluatePowerlineRouteQa(
    const PowerlineRouteDocument& route,
    DjiAircraftProfile aircraftProfile,
    const RouteQaThresholds& thresholds)
{
    RouteQaReport report;

    if (route.waypoints.size() < 2) {
        appendRouteQaIssue(
            &report,
            RouteQaSeverity::Blocking,
            RouteQaIssueType::WaypointCountInsufficient,
            -1,
            -1,
            -1,
            -1,
            QCoreApplication::translate("InspectionRoutePlanning", "Route has fewer than 2 waypoints."),
            QCoreApplication::translate("InspectionRoutePlanning", "DJI KMZ export requires at least 2 waypoints."));
    }

    const double maxWaypointSpeed = maxWaypointSpeedForProfile(aircraftProfile);
    QHash<int, RoutePartPoint> partPointByIndex;
    QHash<int, int> partCoverageCount;
    partCoverageCount.reserve(route.partPoints.size());
    for (const RoutePartPoint& partPoint : route.partPoints) {
        if (partPoint.partIndex > 0) {
            partPointByIndex.insert(partPoint.partIndex, partPoint);
            partCoverageCount.insert(partPoint.partIndex, 0);
        }
    }

    for (int waypointIndex = 0; waypointIndex < route.waypoints.size(); ++waypointIndex) {
        const RouteWaypoint& waypoint = route.waypoints.at(waypointIndex);
        if (waypoint.waypointSpeed > maxWaypointSpeed + 0.01) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Blocking,
                RouteQaIssueType::UnsupportedActionCombination,
                waypointIndex,
                -1,
                -1,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint speed exceeds aircraft profile limit."),
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 speed %2 m/s exceeds %3 m/s.")
                    .arg(QString::number(waypointIndex + 1), QString::number(waypoint.waypointSpeed, 'f', 2), QString::number(maxWaypointSpeed, 'f', 2)));
        }

        if (waypoint.gimbalPitchDeg < -120.0 || waypoint.gimbalPitchDeg > 30.0) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Blocking,
                RouteQaIssueType::UnsupportedActionCombination,
                waypointIndex,
                -1,
                -1,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Gimbal pitch is outside supported range."),
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 gimbal pitch %2 deg is outside [-120, 30].")
                    .arg(QString::number(waypointIndex + 1), QString::number(waypoint.gimbalPitchDeg, 'f', 2)));
        }

        if (waypoint.captureTargets.isEmpty() && !waypoint.isHelperWaypoint) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Warning,
                RouteQaIssueType::MissingPartCoverage,
                waypointIndex,
                -1,
                -1,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint has no capture targets."),
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 is not helper type but has no capture target.")
                    .arg(QString::number(waypointIndex + 1)));
        }

        for (int targetIndex = 0; targetIndex < waypoint.captureTargets.size(); ++targetIndex) {
            const RouteCaptureTarget& captureTarget = waypoint.captureTargets.at(targetIndex);

            if (captureTarget.partIndex > 0) {
                const int previousCount = partCoverageCount.value(captureTarget.partIndex, 0);
                partCoverageCount.insert(captureTarget.partIndex, previousCount + 1);
            }

            if (captureTarget.cameraPitchDeg < -90.0 || captureTarget.cameraPitchDeg > 90.0) {
                appendRouteQaIssue(
                    &report,
                    RouteQaSeverity::Warning,
                    RouteQaIssueType::UnsupportedActionCombination,
                    waypointIndex,
                    -1,
                    captureTarget.partIndex,
                    targetIndex,
                    QCoreApplication::translate("InspectionRoutePlanning", "Camera pitch is outside recommended range."),
                    QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 target %2 camera pitch %3 deg is outside [-90, 90].")
                        .arg(QString::number(waypointIndex + 1), QString::number(targetIndex + 1), QString::number(captureTarget.cameraPitchDeg, 'f', 2)));
            }

            PointRecord targetPoint;
            if (!resolveTargetPoint(captureTarget, partPointByIndex, &targetPoint)) {
                continue;
            }

            const double targetDistance = static_cast<double>(distance3d(waypoint.localPoint, targetPoint));
            if (targetDistance < thresholds.minTargetDistanceMeters) {
                appendRouteQaIssue(
                    &report,
                    RouteQaSeverity::Warning,
                    RouteQaIssueType::TargetDistanceTooNear,
                    waypointIndex,
                    -1,
                    captureTarget.partIndex,
                    targetIndex,
                    QCoreApplication::translate("InspectionRoutePlanning", "Waypoint is too close to capture target."),
                    QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 target %2 distance %3 m is below %4 m.")
                        .arg(
                            QString::number(waypointIndex + 1),
                            QString::number(targetIndex + 1),
                            QString::number(targetDistance, 'f', 2),
                            QString::number(thresholds.minTargetDistanceMeters, 'f', 2)));
            } else if (targetDistance > thresholds.maxTargetDistanceMeters) {
                appendRouteQaIssue(
                    &report,
                    RouteQaSeverity::Warning,
                    RouteQaIssueType::TargetDistanceTooFar,
                    waypointIndex,
                    -1,
                    captureTarget.partIndex,
                    targetIndex,
                    QCoreApplication::translate("InspectionRoutePlanning", "Waypoint is too far from capture target."),
                    QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 target %2 distance %3 m exceeds %4 m.")
                        .arg(
                            QString::number(waypointIndex + 1),
                            QString::number(targetIndex + 1),
                            QString::number(targetDistance, 'f', 2),
                            QString::number(thresholds.maxTargetDistanceMeters, 'f', 2)));
            }
        }
    }

    for (int waypointIndex = 1; waypointIndex < route.waypoints.size(); ++waypointIndex) {
        const RouteWaypoint& previousWaypoint = route.waypoints.at(waypointIndex - 1);
        const RouteWaypoint& currentWaypoint = route.waypoints.at(waypointIndex);
        const double spacingMeters = static_cast<double>(distance3d(previousWaypoint.localPoint, currentWaypoint.localPoint));
        if (spacingMeters < thresholds.minWaypointSpacingMeters) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Blocking,
                RouteQaIssueType::WaypointSpacingTooSmall,
                waypointIndex,
                waypointIndex - 1,
                -1,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Adjacent waypoints are too close."),
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 -> %2 spacing %3 m is below %4 m.")
                    .arg(
                        QString::number(waypointIndex),
                        QString::number(waypointIndex + 1),
                        QString::number(spacingMeters, 'f', 2),
                        QString::number(thresholds.minWaypointSpacingMeters, 'f', 2)));
        } else if (spacingMeters > thresholds.maxWaypointSpacingMeters) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Warning,
                RouteQaIssueType::WaypointSpacingTooLarge,
                waypointIndex,
                waypointIndex - 1,
                -1,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Adjacent waypoints are too far apart."),
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 -> %2 spacing %3 m exceeds %4 m.")
                    .arg(
                        QString::number(waypointIndex),
                        QString::number(waypointIndex + 1),
                        QString::number(spacingMeters, 'f', 2),
                        QString::number(thresholds.maxWaypointSpacingMeters, 'f', 2)));
        }

        const RouteCaptureTarget previousTarget = previousWaypoint.captureTargets.isEmpty()
            ? RouteCaptureTarget()
            : previousWaypoint.captureTargets.first();
        const RouteCaptureTarget currentTarget = currentWaypoint.captureTargets.isEmpty()
            ? RouteCaptureTarget()
            : currentWaypoint.captureTargets.first();

        const double yawDelta = wrappedAngleDeltaDeg(previousWaypoint.aircraftYawDeg, currentWaypoint.aircraftYawDeg);
        const double gimbalPitchDelta = std::abs(currentWaypoint.gimbalPitchDeg - previousWaypoint.gimbalPitchDeg);
        const double cameraYawDelta = wrappedAngleDeltaDeg(previousTarget.cameraYawDeg, currentTarget.cameraYawDeg);
        const double cameraPitchDelta = std::abs(currentTarget.cameraPitchDeg - previousTarget.cameraPitchDeg);
        if (yawDelta > thresholds.maxYawDeltaDeg
            || gimbalPitchDelta > thresholds.maxGimbalPitchDeltaDeg
            || cameraYawDelta > thresholds.maxCameraYawDeltaDeg
            || cameraPitchDelta > thresholds.maxCameraPitchDeltaDeg) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Warning,
                RouteQaIssueType::AttitudeJumpTooLarge,
                waypointIndex,
                waypointIndex - 1,
                -1,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Attitude jump between adjacent waypoints is too large."),
                QCoreApplication::translate("InspectionRoutePlanning", "Yaw %1 deg, gimbal %2 deg, camera yaw %3 deg, camera pitch %4 deg.")
                    .arg(
                        QString::number(yawDelta, 'f', 2),
                        QString::number(gimbalPitchDelta, 'f', 2),
                        QString::number(cameraYawDelta, 'f', 2),
                        QString::number(cameraPitchDelta, 'f', 2)));
        }

        if (yawDelta >= thresholds.helperWaypointYawThresholdDeg
            && !previousWaypoint.isHelperWaypoint
            && !currentWaypoint.isHelperWaypoint) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Warning,
                RouteQaIssueType::HelperWaypointMissing,
                waypointIndex,
                waypointIndex - 1,
                -1,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Large heading change without helper waypoint."),
                QCoreApplication::translate("InspectionRoutePlanning", "Waypoint %1 -> %2 yaw delta %3 deg. Consider adding helper waypoint.")
                    .arg(
                        QString::number(waypointIndex),
                        QString::number(waypointIndex + 1),
                        QString::number(yawDelta, 'f', 2)));
        }
    }

    for (const RoutePartPoint& partPoint : route.partPoints) {
        if (partPoint.partIndex <= 0 || !partPoint.isUsed) {
            continue;
        }

        const int coverageCount = partCoverageCount.value(partPoint.partIndex, 0);
        if (coverageCount <= 0) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Blocking,
                RouteQaIssueType::MissingPartCoverage,
                -1,
                -1,
                partPoint.partIndex,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Part point is not covered by any waypoint target."),
                QCoreApplication::translate("InspectionRoutePlanning", "Part %1 has no linked capture target.")
                    .arg(QString::number(partPoint.partIndex)));
        } else if (coverageCount > 3) {
            appendRouteQaIssue(
                &report,
                RouteQaSeverity::Warning,
                RouteQaIssueType::DuplicatePartCoverage,
                -1,
                -1,
                partPoint.partIndex,
                -1,
                QCoreApplication::translate("InspectionRoutePlanning", "Part point is captured too many times."),
                QCoreApplication::translate("InspectionRoutePlanning", "Part %1 is linked by %2 capture targets.")
                    .arg(QString::number(partPoint.partIndex), QString::number(coverageCount)));
        }
    }

    std::sort(report.issues.begin(), report.issues.end(), [](const RouteQaIssue& left, const RouteQaIssue& right) {
        const int leftRank = routeQaSeverityRank(left.severity);
        const int rightRank = routeQaSeverityRank(right.severity);
        if (leftRank != rightRank) {
            return leftRank > rightRank;
        }
        if (left.waypointIndex != right.waypointIndex) {
            return left.waypointIndex < right.waypointIndex;
        }
        if (left.partIndex != right.partIndex) {
            return left.partIndex < right.partIndex;
        }
        if (left.targetIndex != right.targetIndex) {
            return left.targetIndex < right.targetIndex;
        }
        return static_cast<int>(left.type) < static_cast<int>(right.type);
    });

    return report;
}

InspectionRoute generateInspectionRouteFromRisks(
    const QList<VegetationRiskRecord>& risks,
    const QList<TowerRecord>& towers,
    const RouteGenerationOptions& generationOptions,
    const RouteSafetyOptions& safetyOptions)
{
    InspectionRoute route;
    route.name = QCoreApplication::translate("InspectionRoutePlanning", "Generated Inspection Route");
    route.source = QStringLiteral("VegetationRisks");
    route.generatedAtUtc = QDateTime::currentDateTimeUtc();

    if (risks.isEmpty()) {
        return route;
    }

    QList<int> sortedIndices;
    sortedIndices.reserve(risks.size());
    for (int index = 0; index < risks.size(); ++index) {
        sortedIndices.append(index);
    }

    std::sort(sortedIndices.begin(), sortedIndices.end(), [&risks](int left, int right) {
        const VegetationRiskRecord& leftRisk = risks.at(left);
        const VegetationRiskRecord& rightRisk = risks.at(right);
        if (std::abs(leftRisk.representativeChainage - rightRisk.representativeChainage) > 0.001f) {
            return leftRisk.representativeChainage < rightRisk.representativeChainage;
        }
        if (std::abs(leftRisk.point.x - rightRisk.point.x) > 0.001f) {
            return leftRisk.point.x < rightRisk.point.x;
        }
        return leftRisk.point.y < rightRisk.point.y;
    });

    QList<RouteSamplePoint> controlPoints;
    controlPoints.reserve(sortedIndices.size());
    float cumulative = 0.0f;
    PointRecord previousPoint;
    bool hasPrevious = false;
    for (int orderIndex = 0; orderIndex < sortedIndices.size(); ++orderIndex) {
        const int riskIndex = sortedIndices.at(orderIndex);
        const VegetationRiskRecord& risk = risks.at(riskIndex);

        RouteSamplePoint samplePoint;
        samplePoint.point = risk.point;
        samplePoint.point.z = risk.point.z + safetyOptions.heightOffsetMeters;
        samplePoint.sourceRiskIndex = riskIndex;

        const float nearTowerDistance = nearestTowerDistance(risk.point, towers);
        if (nearTowerDistance < 25.0f) {
            samplePoint.point.z = std::max(samplePoint.point.z, risk.point.z + safetyOptions.heightOffsetMeters + 3.0f);
        }

        if (hasPrevious) {
            cumulative += distance3d(previousPoint, samplePoint.point);
        }
        samplePoint.chainage = cumulative;
        controlPoints.append(samplePoint);
        previousPoint = samplePoint.point;
        hasPrevious = true;
    }

    controlPoints = smoothPolyline(controlPoints, generationOptions.smoothingStrengthPercent);
    QList<RouteSamplePoint> sampledPoints = resampleBySpacing(controlPoints, generationOptions.waypointSpacingMeters);
    if (sampledPoints.isEmpty()) {
        sampledPoints = controlPoints;
    }

    route.waypoints.reserve(sampledPoints.size());
    for (int index = 0; index < sampledPoints.size(); ++index) {
        const RouteSamplePoint& sample = sampledPoints.at(index);
        InspectionWaypoint waypoint;
        waypoint.id = QStringLiteral("wp_%1").arg(index + 1);
        waypoint.localPoint = sample.point;
        waypoint.altitude = sample.point.z;
        waypoint.speedMps = safetyOptions.defaultWaypointSpeedMps;
        waypoint.gimbalPitchDeg = safetyOptions.defaultGimbalPitchDeg;
        waypoint.sourceRiskIndex = sample.sourceRiskIndex;
        waypoint.chainage = sample.chainage;
        route.waypoints.append(waypoint);
    }

    return route;
}

bool transformRouteToWgs84(
    const InspectionRoute& localRoute,
    const ProjectCoordinateSystems& coordinateSystems,
    InspectionRoute* outputRouteWgs84,
    QString* errorMessage)
{
    if (outputRouteWgs84 == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Output route pointer is null.");
        }
        return false;
    }

    if (coordinateSystems.pointCloudCrs.code <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Project point cloud CRS is required before route export.");
        }
        return false;
    }

    if (coordinateSystems.geographicCrs.code <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Project geographic CRS is required before route export.");
        }
        return false;
    }

    InspectionRoute transformed = localRoute;
    for (InspectionWaypoint& waypoint : transformed.waypoints) {
        double longitude = 0.0;
        double latitude = 0.0;
        if (!transformPointEpsg(
                static_cast<double>(waypoint.localPoint.x),
                static_cast<double>(waypoint.localPoint.y),
                coordinateSystems.pointCloudCrs.code,
                coordinateSystems.geographicCrs.code,
                &longitude,
                &latitude,
                errorMessage)) {
            return false;
        }
        waypoint.longitude = longitude;
        waypoint.latitude = latitude;
    }

    *outputRouteWgs84 = transformed;
    return true;
}

bool transformRouteFromWgs84(
    const InspectionRoute& routeWgs84,
    const ProjectCoordinateSystems& coordinateSystems,
    InspectionRoute* outputLocalRoute,
    QString* errorMessage)
{
    if (outputLocalRoute == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Output route pointer is null.");
        }
        return false;
    }

    if (coordinateSystems.pointCloudCrs.code <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Project point cloud CRS is required before route import.");
        }
        return false;
    }

    if (coordinateSystems.geographicCrs.code <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Project geographic CRS is required before route import.");
        }
        return false;
    }

    InspectionRoute transformed = routeWgs84;
    for (InspectionWaypoint& waypoint : transformed.waypoints) {
        double x = 0.0;
        double y = 0.0;
        if (!transformPointEpsg(
                waypoint.longitude,
                waypoint.latitude,
                coordinateSystems.geographicCrs.code,
                coordinateSystems.pointCloudCrs.code,
                &x,
                &y,
                errorMessage)) {
            return false;
        }
        waypoint.localPoint.x = x;
        waypoint.localPoint.y = y;
        waypoint.localPoint.z = waypoint.altitude;
    }

    *outputLocalRoute = transformed;
    return true;
}

QJsonObject inspectionWaypointToJson(const InspectionWaypoint& waypoint)
{
    return QJsonObject {
        { QStringLiteral("id"), waypoint.id },
        { QStringLiteral("x"), waypoint.localPoint.x },
        { QStringLiteral("y"), waypoint.localPoint.y },
        { QStringLiteral("z"), waypoint.localPoint.z },
        { QStringLiteral("longitude"), waypoint.longitude },
        { QStringLiteral("latitude"), waypoint.latitude },
        { QStringLiteral("altitude"), waypoint.altitude },
        { QStringLiteral("speedMps"), waypoint.speedMps },
        { QStringLiteral("yawDeg"), waypoint.yawDeg },
        { QStringLiteral("gimbalPitchDeg"), waypoint.gimbalPitchDeg },
        { QStringLiteral("sourceRiskIndex"), waypoint.sourceRiskIndex },
        { QStringLiteral("chainage"), waypoint.chainage }
    };
}

InspectionWaypoint inspectionWaypointFromJson(const QJsonObject& object)
{
    InspectionWaypoint waypoint;
    waypoint.id = object.value(QStringLiteral("id")).toString().trimmed();
    waypoint.localPoint.x = object.value(QStringLiteral("x")).toDouble();
    waypoint.localPoint.y = object.value(QStringLiteral("y")).toDouble();
    waypoint.localPoint.z = object.value(QStringLiteral("z")).toDouble();
    waypoint.longitude = object.value(QStringLiteral("longitude")).toDouble();
    waypoint.latitude = object.value(QStringLiteral("latitude")).toDouble();
    waypoint.altitude = static_cast<float>(object.value(QStringLiteral("altitude")).toDouble(waypoint.localPoint.z));
    waypoint.speedMps = static_cast<float>(object.value(QStringLiteral("speedMps")).toDouble(waypoint.speedMps));
    waypoint.yawDeg = static_cast<float>(object.value(QStringLiteral("yawDeg")).toDouble(waypoint.yawDeg));
    waypoint.gimbalPitchDeg = static_cast<float>(object.value(QStringLiteral("gimbalPitchDeg")).toDouble(waypoint.gimbalPitchDeg));
    waypoint.sourceRiskIndex = object.value(QStringLiteral("sourceRiskIndex")).toInt(-1);
    waypoint.chainage = static_cast<float>(object.value(QStringLiteral("chainage")).toDouble());
    return waypoint;
}

QJsonObject inspectionRouteToJson(const InspectionRoute& route)
{
    QJsonArray waypointArray;
    for (const InspectionWaypoint& waypoint : route.waypoints) {
        waypointArray.append(inspectionWaypointToJson(waypoint));
    }

    return QJsonObject {
        { QStringLiteral("name"), route.name },
        { QStringLiteral("source"), route.source },
        { QStringLiteral("generatedAtUtc"), route.generatedAtUtc.toString(Qt::ISODate) },
        { QStringLiteral("waypoints"), waypointArray }
    };
}

InspectionRoute inspectionRouteFromJson(const QJsonObject& object)
{
    InspectionRoute route;
    route.name = object.value(QStringLiteral("name")).toString().trimmed();
    route.source = object.value(QStringLiteral("source")).toString().trimmed();
    route.generatedAtUtc = QDateTime::fromString(
        object.value(QStringLiteral("generatedAtUtc")).toString().trimmed(),
        Qt::ISODate);

    const QJsonArray waypointArray = object.value(QStringLiteral("waypoints")).toArray();
    for (const QJsonValue& waypointValue : waypointArray) {
        const InspectionWaypoint waypoint = inspectionWaypointFromJson(waypointValue.toObject());
        if (waypoint.id.isEmpty()) {
            continue;
        }
        route.waypoints.append(waypoint);
    }
    return route;
}

QJsonObject routePlanningOptionsToJson(const RoutePlanningOptions& options)
{
    return QJsonObject {
        { QStringLiteral("generation"), generationOptionsToJson(options.generation) },
        { QStringLiteral("safety"), safetyOptionsToJson(options.safety) },
        { QStringLiteral("aircraftProfile"), static_cast<int>(options.aircraftProfile) }
    };
}

RoutePlanningOptions routePlanningOptionsFromJson(const QJsonObject& object)
{
    RoutePlanningOptions options;
    options.generation = generationOptionsFromJson(object.value(QStringLiteral("generation")).toObject());
    options.safety = safetyOptionsFromJson(object.value(QStringLiteral("safety")).toObject());
    options.crs = crsOptionsFromJson(object.value(QStringLiteral("crs")).toObject());
    options.aircraftProfile = static_cast<DjiAircraftProfile>(
        object.value(QStringLiteral("aircraftProfile")).toInt(static_cast<int>(options.aircraftProfile)));
    return options;
}
