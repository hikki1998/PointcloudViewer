#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>

#include <iostream>

#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"

namespace
{
bool verify(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << std::endl;
        return false;
    }
    return true;
}

bool verifyRoundTripShape(
    const PowerlineRouteDocument& original,
    const PowerlineRouteDocument& roundTripped)
{
    if (!verify(original.partPoints.size() == roundTripped.partPoints.size(), "Part point count mismatch after roundtrip")) {
        return false;
    }
    if (!verify(original.waypoints.size() == roundTripped.waypoints.size(), "Waypoint count mismatch after roundtrip")) {
        return false;
    }

    for (int index = 0; index < original.partPoints.size(); ++index) {
        const RoutePartPoint& left = original.partPoints.at(index);
        const RoutePartPoint& right = roundTripped.partPoints.at(index);
        if (!verify(left.partIndex == right.partIndex, "Part index mismatch after roundtrip")) {
            return false;
        }
        if (!verify(left.fileId == right.fileId, "Part file ID mismatch after roundtrip")) {
            return false;
        }
    }

    for (int index = 0; index < original.waypoints.size(); ++index) {
        const RouteWaypoint& left = original.waypoints.at(index);
        const RouteWaypoint& right = roundTripped.waypoints.at(index);
        if (!verify(left.primaryPartIndex == right.primaryPartIndex, "Primary part index mismatch after roundtrip")) {
            return false;
        }
        if (!verify(left.isHelperWaypoint == right.isHelperWaypoint, "Helper waypoint flag mismatch after roundtrip")) {
            return false;
        }
        if (!verify(left.captureTargets.size() == right.captureTargets.size(), "Capture target count mismatch after roundtrip")) {
            return false;
        }
    }

    return true;
}

PowerlineRouteDocument buildSyntheticRoute()
{
    PowerlineRouteDocument route;
    route.taskName = QStringLiteral("Synthetic Route");
    route.createdAt = QDateTime::currentDateTimeUtc();
    route.updatedAt = route.createdAt;

    RoutePartPoint leftInsulator;
    leftInsulator.partIndex = 1;
    leftInsulator.fileId = 101;
    leftInsulator.partName = QStringLiteral("Left Insulator");
    leftInsulator.longitude = 114.1001;
    leftInsulator.latitude = 30.2001;
    leftInsulator.dh = 55.0;
    leftInsulator.localPoint = PointRecord { 10.0f, 20.0f, 55.0f };
    route.partPoints.append(leftInsulator);

    RoutePartPoint rightInsulator;
    rightInsulator.partIndex = 2;
    rightInsulator.fileId = 102;
    rightInsulator.partName = QStringLiteral("Right Insulator");
    rightInsulator.longitude = 114.1002;
    rightInsulator.latitude = 30.2002;
    rightInsulator.dh = 55.5;
    rightInsulator.localPoint = PointRecord { 11.0f, 21.0f, 55.5f };
    route.partPoints.append(rightInsulator);

    RouteWaypoint captureWaypoint;
    captureWaypoint.sequenceIndex = 0;
    captureWaypoint.primaryPartIndex = 1;
    captureWaypoint.rawKeyId = 101;
    captureWaypoint.isHelperWaypoint = false;
    captureWaypoint.towerName = QStringLiteral("Tower 45");
    captureWaypoint.phaseSequence = QStringLiteral("ABC");
    captureWaypoint.isStart = true;
    captureWaypoint.turnMode = 0;
    captureWaypoint.waypointSpeed = 6.0;
    captureWaypoint.cornerRadiusMeters = 2.0;
    captureWaypoint.longitude = 114.10015;
    captureWaypoint.latitude = 30.20015;
    captureWaypoint.dh = 56.0;
    captureWaypoint.height = 56.0;
    captureWaypoint.aircraftYawDeg = 90.0;
    captureWaypoint.gimbalPitchDeg = -35.0;
    captureWaypoint.localPoint = PointRecord { 10.5f, 20.5f, 56.0f };

    RouteCaptureTarget leftTarget;
    leftTarget.partIndex = 1;
    leftTarget.partFileId = 101;
    leftTarget.partName = leftInsulator.partName;
    leftTarget.captureCount = 1;
    leftTarget.aircraftYawDeg = 90.0;
    leftTarget.gimbalPitchDeg = -35.0;
    leftTarget.targetLocalPoint = leftInsulator.localPoint;
    captureWaypoint.captureTargets.append(leftTarget);

    RouteCaptureTarget rightTarget;
    rightTarget.partIndex = 2;
    rightTarget.partFileId = 102;
    rightTarget.partName = rightInsulator.partName;
    rightTarget.captureCount = 1;
    rightTarget.aircraftYawDeg = 90.0;
    rightTarget.gimbalPitchDeg = -40.0;
    rightTarget.targetLocalPoint = rightInsulator.localPoint;
    captureWaypoint.captureTargets.append(rightTarget);

    route.waypoints.append(captureWaypoint);

    RouteWaypoint helperWaypoint;
    helperWaypoint.sequenceIndex = 1;
    helperWaypoint.primaryPartIndex = -1;
    helperWaypoint.rawKeyId = -7;
    helperWaypoint.isHelperWaypoint = true;
    helperWaypoint.turnMode = 1;
    helperWaypoint.waypointSpeed = 5.0;
    helperWaypoint.longitude = 114.1003;
    helperWaypoint.latitude = 30.2003;
    helperWaypoint.dh = 57.0;
    helperWaypoint.height = 57.0;
    helperWaypoint.localPoint = PointRecord { 12.0f, 22.0f, 57.0f };
    helperWaypoint.rotationCenter = PointRecord { 11.5f, 21.5f, 56.5f };
    route.waypoints.append(helperWaypoint);

    return route;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString templatePath = QDir::current().absoluteFilePath(QStringLiteral("templates/N#045.json"));
    PowerlineRouteDocument importedRoute;
    QString errorMessage;
    if (!importPowerlineRouteJson(templatePath, &importedRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(template): " << errorMessage.toStdString() << std::endl;
        return 1;
    }

    if (!verify(importedRoute.partPoints.size() == 25, "Template should contain 25 part points")) {
        return 1;
    }
    if (!verify(importedRoute.waypoints.size() == 37, "Template should contain 37 waypoints")) {
        return 1;
    }
    if (!verify(toRouteDisplayPoints(importedRoute).size() == importedRoute.waypoints.size(), "Display point count mismatch")) {
        return 1;
    }
    if (!verify(toRouteDisplayLabels(importedRoute).size() == importedRoute.waypoints.size(), "Display label count mismatch")) {
        return 1;
    }

    for (const RouteWaypoint& waypoint : importedRoute.waypoints) {
        if (waypoint.rawKeyId > 0 && !verify(waypoint.primaryPartIndex > 0, "Positive keyID should map to partIndex")) {
            return 1;
        }
        for (const RouteCaptureTarget& captureTarget : waypoint.captureTargets) {
            if (captureTarget.partFileId > 0 && !verify(captureTarget.partIndex > 0, "Capture target should resolve to partIndex")) {
                return 1;
            }
        }
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return 1;
    }

    const QString roundTripPath = QDir(tempDir.path()).filePath(QStringLiteral("roundtrip_route.json"));
    if (!exportPowerlineRouteJson(roundTripPath, importedRoute, &errorMessage)) {
        std::cerr << "[FAIL] exportPowerlineRouteJson(template): " << errorMessage.toStdString() << std::endl;
        return 1;
    }

    PowerlineRouteDocument roundTrippedRoute;
    if (!importPowerlineRouteJson(roundTripPath, &roundTrippedRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(roundtrip): " << errorMessage.toStdString() << std::endl;
        return 1;
    }
    if (!verifyRoundTripShape(importedRoute, roundTrippedRoute)) {
        return 1;
    }

    const PowerlineRouteDocument syntheticRoute = buildSyntheticRoute();
    const QString syntheticPath = QDir(tempDir.path()).filePath(QStringLiteral("synthetic_route.json"));
    if (!exportPowerlineRouteJson(syntheticPath, syntheticRoute, &errorMessage)) {
        std::cerr << "[FAIL] exportPowerlineRouteJson(synthetic): " << errorMessage.toStdString() << std::endl;
        return 1;
    }

    PowerlineRouteDocument importedSyntheticRoute;
    if (!importPowerlineRouteJson(syntheticPath, &importedSyntheticRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(synthetic): " << errorMessage.toStdString() << std::endl;
        return 1;
    }

    if (!verify(importedSyntheticRoute.waypoints.size() == 2, "Synthetic route should contain 2 waypoints")) {
        return 1;
    }
    if (!verify(importedSyntheticRoute.waypoints.first().captureTargets.size() == 2, "Synthetic capture waypoint should keep 2 targets")) {
        return 1;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().isHelperWaypoint, "Synthetic helper waypoint flag should persist")) {
        return 1;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().rawKeyId < 0, "Synthetic helper waypoint should keep negative keyID")) {
        return 1;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().rotationCenter.has_value(), "Synthetic helper waypoint rotation center should persist")) {
        return 1;
    }

    std::cout << "[PASS] Route JSON smoke test completed." << std::endl;
    return 0;
}
