#include "route/PowerlineRouteBridge.h"

#include <QDateTime>

#include <cmath>

namespace
{
float distance3d(const PointRecord& left, const PointRecord& right)
{
    const double dx = static_cast<double>(right.x) - static_cast<double>(left.x);
    const double dy = static_cast<double>(right.y) - static_cast<double>(left.y);
    const double dz = static_cast<double>(right.z) - static_cast<double>(left.z);
    return static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
}

QString routeLabelForWaypoint(
    const RouteWaypoint& waypoint,
    const QHash<int, QString>& partNames,
    int displayIndex)
{
    if (!waypoint.captureTargets.isEmpty()) {
        const QString firstName = waypoint.captureTargets.first().partName.trimmed();
        if (!firstName.isEmpty()) {
            return firstName;
        }
    }

    if (waypoint.primaryPartIndex > 0 && partNames.contains(waypoint.primaryPartIndex)) {
        const QString partName = partNames.value(waypoint.primaryPartIndex).trimmed();
        if (!partName.isEmpty()) {
            return partName;
        }
    }

    return waypoint.isHelperWaypoint
        ? QStringLiteral("AUX %1").arg(displayIndex + 1)
        : QStringLiteral("WP %1").arg(displayIndex + 1);
}
}

InspectionRoute toInspectionRouteExportView(const PowerlineRouteDocument& route)
{
    InspectionRoute exportRoute;
    exportRoute.name = route.taskName.trimmed();
    exportRoute.source = QStringLiteral("PowerlineRoute");
    exportRoute.generatedAtUtc = route.updatedAt.isValid()
        ? route.updatedAt
        : (route.createdAt.isValid() ? route.createdAt : QDateTime::currentDateTimeUtc());

    const QStringList labels = toRouteDisplayLabels(route);
    exportRoute.waypoints.reserve(route.waypoints.size());

    float cumulativeChainage = 0.0f;
    PointRecord previousPoint;
    bool hasPreviousPoint = false;
    for (int index = 0; index < route.waypoints.size(); ++index) {
        const RouteWaypoint& sourceWaypoint = route.waypoints.at(index);

        InspectionWaypoint waypoint;
        waypoint.id = index < labels.size() ? labels.at(index) : QStringLiteral("WP %1").arg(index + 1);
        waypoint.localPoint = sourceWaypoint.localPoint;
        waypoint.longitude = sourceWaypoint.longitude;
        waypoint.latitude = sourceWaypoint.latitude;
        waypoint.altitude = static_cast<float>(
            sourceWaypoint.height != 0.0 ? sourceWaypoint.height : sourceWaypoint.localPoint.z);
        waypoint.speedMps = static_cast<float>(sourceWaypoint.waypointSpeed);
        waypoint.yawDeg = static_cast<float>(sourceWaypoint.aircraftYawDeg);
        waypoint.gimbalPitchDeg = static_cast<float>(sourceWaypoint.gimbalPitchDeg);

        if (hasPreviousPoint) {
            cumulativeChainage += distance3d(previousPoint, sourceWaypoint.localPoint);
        }
        waypoint.chainage = cumulativeChainage;

        previousPoint = sourceWaypoint.localPoint;
        hasPreviousPoint = true;
        exportRoute.waypoints.append(waypoint);
    }

    return exportRoute;
}

QList<PointRecord> toRouteDisplayPoints(const PowerlineRouteDocument& route)
{
    QList<PointRecord> points;
    points.reserve(route.waypoints.size());
    for (const RouteWaypoint& waypoint : route.waypoints) {
        points.append(waypoint.localPoint);
    }
    return points;
}

QStringList toRouteDisplayLabels(const PowerlineRouteDocument& route)
{
    QHash<int, QString> partNames;
    for (const RoutePartPoint& partPoint : route.partPoints) {
        if (partPoint.partIndex > 0) {
            partNames.insert(partPoint.partIndex, partPoint.partName);
        }
    }

    QStringList labels;
    labels.reserve(route.waypoints.size());
    for (int index = 0; index < route.waypoints.size(); ++index) {
        labels.append(routeLabelForWaypoint(route.waypoints.at(index), partNames, index));
    }
    return labels;
}

PowerlineRouteDocument createPowerlineRouteFromInspectionRoute(
    const InspectionRoute& route,
    const QString& taskName)
{
    PowerlineRouteDocument document;
    document.taskName = taskName.trimmed().isEmpty() ? route.name.trimmed() : taskName.trimmed();
    document.createdAt = route.generatedAtUtc.isValid() ? route.generatedAtUtc : QDateTime::currentDateTimeUtc();
    document.updatedAt = document.createdAt;

    document.waypoints.reserve(route.waypoints.size());
    for (int index = 0; index < route.waypoints.size(); ++index) {
        const InspectionWaypoint& sourceWaypoint = route.waypoints.at(index);

        RouteWaypoint waypoint;
        waypoint.sequenceIndex = index;
        waypoint.primaryPartIndex = -1;
        waypoint.rawKeyId = -1;
        waypoint.isHelperWaypoint = true;
        waypoint.isStart = index == 0;
        waypoint.waypointSpeed = sourceWaypoint.speedMps;
        waypoint.cornerRadiusMeters = 0.0;
        waypoint.longitude = sourceWaypoint.longitude;
        waypoint.latitude = sourceWaypoint.latitude;
        waypoint.dh = sourceWaypoint.altitude;
        waypoint.height = sourceWaypoint.altitude;
        waypoint.aircraftYawDeg = sourceWaypoint.yawDeg;
        waypoint.gimbalPitchDeg = sourceWaypoint.gimbalPitchDeg;
        waypoint.localPoint = sourceWaypoint.localPoint;
        document.waypoints.append(waypoint);
    }

    return document;
}
