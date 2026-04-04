#include "domain/InspectionRoutePlanning.h"

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
        waypoint.localPoint.x = static_cast<float>(x);
        waypoint.localPoint.y = static_cast<float>(y);
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
    waypoint.localPoint.x = static_cast<float>(object.value(QStringLiteral("x")).toDouble());
    waypoint.localPoint.y = static_cast<float>(object.value(QStringLiteral("y")).toDouble());
    waypoint.localPoint.z = static_cast<float>(object.value(QStringLiteral("z")).toDouble());
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
