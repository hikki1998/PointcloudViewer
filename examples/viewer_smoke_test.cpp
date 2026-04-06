#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLWidget>
#include <QSet>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <cmath>
#include <functional>
#include <iostream>

#include "crs/CrsAuthorityService.h"
#include "domain/InspectionData.h"
#include "domain/TowerFileInterop.h"
#include "gui/PointCloudViewer.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"
#include "route/RouteInterop.h"

namespace
{
struct SmokeCase
{
    QString mode;
    QString category;
    QString displayName;
    bool requiresLas = false;
    std::function<bool(const QStringList&)> run;
};

void pumpEvents(int durationMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < durationMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(25);
    }
}

bool verify(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "[FAIL] " << message << std::endl;
        return false;
    }
    return true;
}

bool verifyClose(double left, double right, double tolerance, const std::string& message)
{
    if (std::abs(left - right) > tolerance) {
        std::cerr << "[FAIL] " << message << " left=" << left << " right=" << right << std::endl;
        return false;
    }
    return true;
}

bool hasVisiblePixels(const QImage& image, int* nonBackgroundPixelCount)
{
    if (image.isNull()) {
        if (nonBackgroundPixelCount != nullptr) {
            *nonBackgroundPixelCount = 0;
        }
        return false;
    }

    const QRgb background = image.pixel(0, 0);
    int count = 0;

    for (int y = 0; y < image.height(); ++y) {
        const QRgb* scanLine = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = scanLine[x];
            const int redDelta = std::abs(qRed(pixel) - qRed(background));
            const int greenDelta = std::abs(qGreen(pixel) - qGreen(background));
            const int blueDelta = std::abs(qBlue(pixel) - qBlue(background));
            if (redDelta > 2 || greenDelta > 2 || blueDelta > 2) {
                ++count;
            }
        }
    }

    if (nonBackgroundPixelCount != nullptr) {
        *nonBackgroundPixelCount = count;
    }

    return count > 100;
}

bool runViewerRenderSmoke(const QStringList& filePaths)
{
    bool allPassed = true;

    for (const QString& filePath : filePaths) {
        PointCloudViewer viewer;
        viewer.resize(1024, 768);
        viewer.show();

        pumpEvents(500);

        QString errorMessage;
        if (!viewer.loadPointCloud(filePath, &errorMessage)) {
            std::cerr << "Load failed for " << filePath.toStdString() << ": "
                      << errorMessage.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        pumpEvents(1000);

        QOpenGLWidget* glWidget = viewer.findChild<QOpenGLWidget*>();
        if (glWidget == nullptr) {
            std::cerr << "No QOpenGLWidget found for " << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        const QImage frame = glWidget->grabFramebuffer();
        if (frame.isNull()) {
            std::cerr << "grabFramebuffer() returned a null image for "
                      << filePath.toStdString() << std::endl;
            allPassed = false;
            continue;
        }

        int nonBackgroundPixelCount = 0;
        const bool visiblePixels = hasVisiblePixels(frame, &nonBackgroundPixelCount);

        std::cout << "Loaded " << filePath.toStdString()
                  << " framebuffer=" << frame.width() << "x" << frame.height()
                  << " nonBackgroundPixels=" << nonBackgroundPixelCount << std::endl;

        if (!visiblePixels) {
            std::cerr << "Rendered framebuffer appears empty for "
                      << filePath.toStdString() << std::endl;
            allPassed = false;
        }
    }

    return allPassed;
}

QList<PointRecord> buildSyntheticWaypoints()
{
    QList<PointRecord> waypoints;

    PointRecord first;
    first.x = 0.0f;
    first.y = 0.0f;
    first.z = 30.0f;
    waypoints.append(first);

    PointRecord second;
    second.x = 120.0f;
    second.y = 40.0f;
    second.z = 32.0f;
    waypoints.append(second);

    PointRecord third;
    third.x = 240.0f;
    third.y = 80.0f;
    third.z = 35.0f;
    waypoints.append(third);

    return waypoints;
}

bool runRouteRoamStateSmoke(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) {
        std::cerr << "[FAIL] Route roam smoke requires at least one LAS/LAZ file." << std::endl;
        return false;
    }

    PointCloudViewer viewer;
    viewer.resize(1280, 800);
    viewer.show();
    pumpEvents(500);

    QString errorMessage;
    if (!viewer.loadPointCloud(filePaths.first(), &errorMessage)) {
        std::cerr << "[FAIL] loadPointCloud: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    pumpEvents(1000);

    int stateChangedCount = 0;
    int photoCapturedCount = 0;
    QObject::connect(&viewer, &PointCloudViewer::inspectionRouteRoamStateChanged, &viewer, [&stateChangedCount]() {
        ++stateChangedCount;
    });
    QObject::connect(
        &viewer,
        &PointCloudViewer::inspectionRouteRoamPhotoCaptured,
        &viewer,
        [&photoCapturedCount](int, int, const QString&, int) {
            ++photoCapturedCount;
        });

    viewer.setInspectionRouteWaypoints(buildSyntheticWaypoints());
    viewer.setInspectionRouteVisible(true);
    pumpEvents(200);

    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should be inactive before start")) {
        return false;
    }

    viewer.setInspectionRouteRoamSpeedMetersPerSecond(-5.0);
    if (!verifyClose(
            viewer.inspectionRouteRoamSpeedMetersPerSecond(),
            0.1,
            1e-6,
            "Roam speed should clamp to lower bound")) {
        return false;
    }

    viewer.setInspectionRouteRoamSpeedMetersPerSecond(500.0);
    if (!verifyClose(
            viewer.inspectionRouteRoamSpeedMetersPerSecond(),
            80.0,
            1e-6,
            "Roam speed should clamp to upper bound")) {
        return false;
    }

    viewer.setInspectionRouteRoamSpeedMetersPerSecond(6.0);
    viewer.setInspectionRouteRoamViewMode(RouteRoamViewMode::FirstPerson);
    if (!verify(
            viewer.inspectionRouteRoamViewMode() == RouteRoamViewMode::FirstPerson,
            "Roam view mode should switch to first-person")) {
        return false;
    }

    viewer.startInspectionRouteRoam(0);
    pumpEvents(250);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should become active after start")) {
        return false;
    }
    if (!verify(viewer.inspectionRouteRoamPlaying(), "Roam should be playing after start")) {
        return false;
    }
    if (!verify(photoCapturedCount >= 1, "Roam should emit at least one photo capture signal during start")) {
        return false;
    }

    viewer.pauseInspectionRouteRoam();
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamPaused(), "Roam should be paused after pause")) {
        return false;
    }

    viewer.resumeInspectionRouteRoam();
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamPlaying(), "Roam should resume to playing state")) {
        return false;
    }

    viewer.stopInspectionRouteRoam(true);
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should stop after explicit stop")) {
        return false;
    }

    viewer.startInspectionRouteRoam(0);
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should start again before visibility-stop check")) {
        return false;
    }
    viewer.setInspectionRouteVisible(false);
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should auto-stop when route is hidden")) {
        return false;
    }

    viewer.setInspectionRouteVisible(true);
    viewer.startInspectionRouteRoam(0);
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should start before clear-waypoints stop check")) {
        return false;
    }
    viewer.clearInspectionRouteWaypoints();
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should auto-stop when waypoints are cleared")) {
        return false;
    }

    viewer.setInspectionRouteWaypoints(buildSyntheticWaypoints());
    viewer.setInspectionRouteVisible(true);
    viewer.startInspectionRouteRoam(0);
    pumpEvents(120);
    if (!verify(viewer.inspectionRouteRoamActive(), "Roam should start before clear-pointcloud stop check")) {
        return false;
    }
    viewer.clearPointCloud();
    pumpEvents(120);
    if (!verify(!viewer.inspectionRouteRoamActive(), "Roam should auto-stop when point cloud is cleared")) {
        return false;
    }

    if (!verify(stateChangedCount >= 8, "Roam state-changed signal count is unexpectedly low")) {
        return false;
    }

    std::cout << "StateChangedSignals=" << stateChangedCount
              << " PhotoCapturedSignals=" << photoCapturedCount << std::endl;
    std::cout << "[PASS] Route roam state smoke test completed." << std::endl;
    return true;
}

bool verifyRouteRoundTripShape(
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

bool runRouteJsonSmoke(const QStringList&)
{
    const QString templatePath = QDir::current().absoluteFilePath(QStringLiteral("templates/N#045.json"));
    PowerlineRouteDocument importedRoute;
    QString errorMessage;
    if (!importPowerlineRouteJson(templatePath, &importedRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(template): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    if (!verify(importedRoute.partPoints.size() == 25, "Template should contain 25 part points")) {
        return false;
    }
    if (!verify(importedRoute.waypoints.size() == 37, "Template should contain 37 waypoints")) {
        return false;
    }
    if (!verify(toRouteDisplayPoints(importedRoute).size() == importedRoute.waypoints.size(), "Display point count mismatch")) {
        return false;
    }
    if (!verify(toRouteDisplayLabels(importedRoute).size() == importedRoute.waypoints.size(), "Display label count mismatch")) {
        return false;
    }

    for (const RouteWaypoint& waypoint : importedRoute.waypoints) {
        if (waypoint.rawKeyId > 0 && !verify(waypoint.primaryPartIndex > 0, "Positive keyID should map to partIndex")) {
            return false;
        }
        for (const RouteCaptureTarget& captureTarget : waypoint.captureTargets) {
            if (captureTarget.partFileId > 0
                && !verify(captureTarget.partIndex > 0, "Capture target should resolve to partIndex")) {
                return false;
            }
        }
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString roundTripPath = QDir(tempDir.path()).filePath(QStringLiteral("roundtrip_route.json"));
    if (!exportPowerlineRouteJson(roundTripPath, importedRoute, &errorMessage)) {
        std::cerr << "[FAIL] exportPowerlineRouteJson(template): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    PowerlineRouteDocument roundTrippedRoute;
    if (!importPowerlineRouteJson(roundTripPath, &roundTrippedRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(roundtrip): " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verifyRouteRoundTripShape(importedRoute, roundTrippedRoute)) {
        return false;
    }

    const PowerlineRouteDocument syntheticRoute = buildSyntheticRoute();
    const QString syntheticPath = QDir(tempDir.path()).filePath(QStringLiteral("synthetic_route.json"));
    if (!exportPowerlineRouteJson(syntheticPath, syntheticRoute, &errorMessage)) {
        std::cerr << "[FAIL] exportPowerlineRouteJson(synthetic): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    PowerlineRouteDocument importedSyntheticRoute;
    if (!importPowerlineRouteJson(syntheticPath, &importedSyntheticRoute, &errorMessage)) {
        std::cerr << "[FAIL] importPowerlineRouteJson(synthetic): " << errorMessage.toStdString() << std::endl;
        return false;
    }

    if (!verify(importedSyntheticRoute.waypoints.size() == 2, "Synthetic route should contain 2 waypoints")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.first().captureTargets.size() == 2, "Synthetic capture waypoint should keep 2 targets")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().isHelperWaypoint, "Synthetic helper waypoint flag should persist")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().rawKeyId < 0, "Synthetic helper waypoint should keep negative keyID")) {
        return false;
    }
    if (!verify(importedSyntheticRoute.waypoints.last().rotationCenter.has_value(), "Synthetic helper waypoint rotation center should persist")) {
        return false;
    }

    std::cout << "[PASS] Route JSON smoke test completed." << std::endl;
    return true;
}

bool runRouteInteropSmoke(const QStringList&)
{
    const lasviewer::crs::CrsResolveResult authorityResult =
        lasviewer::crs::CrsAuthorityService::resolveFromAuthority(QStringLiteral("EPSG"), 4326);
    if (!verify(authorityResult.ok, "EPSG:4326 should resolve from authority database")) {
        return false;
    }
    if (!verify(!authorityResult.definition.reference.wkt.trimmed().isEmpty(), "Resolved EPSG:4326 should provide WKT")) {
        return false;
    }

    const lasviewer::crs::CrsResolveResult wktResult =
        lasviewer::crs::CrsAuthorityService::resolveFromWkt(authorityResult.definition.reference.wkt);
    if (!verify(wktResult.ok, "WKT should resolve back to a CRS definition")) {
        return false;
    }
    if (!verify(
            wktResult.definition.reference.authName.compare(QStringLiteral("EPSG"), Qt::CaseInsensitive) == 0
                && wktResult.definition.reference.code == 4326,
            "WKT should identify back to EPSG:4326")) {
        return false;
    }

    const QList<lasviewer::crs::CoordinateSystemDefinition> nameMatches =
        lasviewer::crs::CrsAuthorityService::findByName(
            QStringLiteral("WGS 84"),
            lasviewer::crs::CoordinateSystemKindFilter::Geographic,
            5);
    if (!verify(!nameMatches.isEmpty(), "Name lookup for WGS 84 should return candidates")) {
        return false;
    }

    QList<VegetationRiskRecord> risks;
    for (int index = 0; index < 4; ++index) {
        VegetationRiskRecord risk;
        risk.id = QStringLiteral("risk_%1").arg(index + 1);
        risk.title = QStringLiteral("Risk %1").arg(index + 1);
        risk.point.x = static_cast<float>(120.0 + index * 60.0);
        risk.point.y = static_cast<float>(240.0 + index * 25.0);
        risk.point.z = static_cast<float>(30.0 + index * 2.0);
        risk.representativeChainage = static_cast<float>(index * 65.0);
        risks.append(risk);
    }

    QList<TowerRecord> towers;
    TowerRecord towerA;
    towerA.name = QStringLiteral("Tower A");
    towerA.point.x = 140.0f;
    towerA.point.y = 245.0f;
    towerA.point.z = 35.0f;
    towers.append(towerA);

    RouteGenerationOptions generationOptions;
    generationOptions.waypointSpacingMeters = 25.0f;
    generationOptions.smoothingStrengthPercent = 20.0f;

    RouteSafetyOptions safetyOptions;
    safetyOptions.heightOffsetMeters = 18.0f;
    safetyOptions.defaultWaypointSpeedMps = 6.0f;

    const InspectionRoute localRoute = generateInspectionRouteFromRisks(
        risks, towers, generationOptions, safetyOptions);
    if (!verify(localRoute.waypoints.size() >= 3, "Route generation should produce at least 3 waypoints")) {
        return false;
    }

    const PowerlineRouteDocument routeDocument =
        createPowerlineRouteFromInspectionRoute(localRoute, QStringLiteral("Smoke Route"));
    if (!verify(routeDocument.waypoints.size() == localRoute.waypoints.size(), "Bridge document size mismatch")) {
        return false;
    }

    const InspectionRoute bridgedLocalRoute = toInspectionRouteExportView(routeDocument);
    if (!verify(bridgedLocalRoute.waypoints.size() == localRoute.waypoints.size(), "Bridge export size mismatch")) {
        return false;
    }
    if (!verify(
            !bridgedLocalRoute.waypoints.isEmpty()
                && bridgedLocalRoute.waypoints.first().localPoint.x == localRoute.waypoints.first().localPoint.x
                && bridgedLocalRoute.waypoints.first().localPoint.y == localRoute.waypoints.first().localPoint.y,
            "Bridge export should preserve waypoint coordinates")) {
        return false;
    }

    RoutePlanningOptions planningOptions;
    planningOptions.generation = generationOptions;
    planningOptions.safety = safetyOptions;
    planningOptions.crs.sourceEpsg = 4326;
    planningOptions.crs.targetEpsg = 4326;
    planningOptions.aircraftProfile = DjiAircraftProfile::M30Series;

    ProjectCoordinateSystems coordinateSystems;
    coordinateSystems.pointCloudCrs.authName = QStringLiteral("EPSG");
    coordinateSystems.pointCloudCrs.code = 4326;
    coordinateSystems.pointCloudCrs.displayName = QStringLiteral("WGS 84");
    coordinateSystems.pointCloudCrs.kind = CoordinateSystemKind::Projected;
    coordinateSystems.geographicCrs = defaultGeographicCoordinateSystem();

    InspectionRoute routeWgs84;
    QString errorMessage;
    if (!transformRouteToWgs84(bridgedLocalRoute, coordinateSystems, &routeWgs84, &errorMessage)) {
        std::cerr << "[FAIL] transformRouteToWgs84: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(routeWgs84.waypoints.size() == bridgedLocalRoute.waypoints.size(), "Transformed route size mismatch")) {
        return false;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString kmlPath = QDir(tempDir.path()).filePath(QStringLiteral("route_test.kml"));
    if (!exportRouteKml(kmlPath, routeWgs84, &errorMessage)) {
        std::cerr << "[FAIL] exportRouteKml: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(QFile::exists(kmlPath), "KML file should exist")) {
        return false;
    }

    InspectionRoute importedRouteWgs84;
    if (!importRouteKml(kmlPath, &importedRouteWgs84, &errorMessage)) {
        std::cerr << "[FAIL] importRouteKml: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(
            importedRouteWgs84.waypoints.size() == routeWgs84.waypoints.size(),
            "KML roundtrip waypoint size mismatch")) {
        return false;
    }

    const QString kmzPath = QDir(tempDir.path()).filePath(QStringLiteral("route_test.kmz"));
    if (!exportRouteDjiKmz(kmzPath, routeWgs84, planningOptions, &errorMessage)) {
        std::cerr << "[FAIL] exportRouteDjiKmz: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(QFile::exists(kmzPath), "KMZ file should exist")) {
        return false;
    }

    QFile kmzFile(kmzPath);
    if (!verify(kmzFile.open(QIODevice::ReadOnly), "KMZ file should be readable")) {
        return false;
    }
    const QByteArray kmzData = kmzFile.readAll();
    kmzFile.close();
    if (!verify(kmzData.contains("wpmz/template.kml"), "KMZ should contain template.kml entry")) {
        return false;
    }
    if (!verify(kmzData.contains("wpmz/waylines.wpml"), "KMZ should contain waylines.wpml entry")) {
        return false;
    }

    std::cout << "[PASS] Route interop smoke test completed." << std::endl;
    return true;
}

void normalizeTowerIndices(QList<TowerRecord>* towers)
{
    if (towers == nullptr) {
        return;
    }
    for (int index = 0; index < towers->size(); ++index) {
        (*towers)[index].index = index;
    }
}

QString resolveProjectPath(const QString& projectFilePath, const QString& storedPath)
{
    if (storedPath.isEmpty()) {
        return QString();
    }
    const QFileInfo storedInfo(storedPath);
    if (storedInfo.isAbsolute()) {
        return storedPath;
    }
    return QFileInfo(QFileInfo(projectFilePath).absoluteDir(), storedPath).absoluteFilePath();
}

bool runTowerFileInteropSmoke(const QStringList&)
{
    QList<TowerRecord> expectedTowers;
    TowerRecord tower0;
    tower0.index = 0;
    tower0.name = QStringLiteral("#001");
    tower0.point.x = 100.5f;
    tower0.point.y = 200.25f;
    tower0.point.z = 300.125f;
    tower0.towerType = TowerType::Unknown;
    expectedTowers.append(tower0);

    TowerRecord tower1;
    tower1.index = 1;
    tower1.name = QStringLiteral("#002");
    tower1.point.x = 110.5f;
    tower1.point.y = 210.25f;
    tower1.point.z = 310.125f;
    tower1.towerType = TowerType::Tangent;
    expectedTowers.append(tower1);

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString towerPath = QDir(tempDir.path()).filePath(QStringLiteral("tower.LiTower"));
    QString errorMessage;
    if (!exportTowerLiTowerFile(towerPath, expectedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(QFile::exists(towerPath), "Exported tower file should exist")) {
        return false;
    }

    QFile file(towerPath);
    if (!verify(file.open(QIODevice::ReadOnly | QIODevice::Text), "Exported tower file should be readable")) {
        return false;
    }
    const QString firstLine = QString::fromUtf8(file.readLine()).trimmed();
    file.close();
    if (!verify(firstLine == QStringLiteral("Index,X,Y,Z,Type,Name"), "Tower file header should match LiTower format")) {
        return false;
    }

    QList<TowerRecord> importedTowers;
    if (!importTowerLiTowerFile(towerPath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    if (!verify(importedTowers.size() == expectedTowers.size(), "Imported tower count mismatch")) {
        return false;
    }

    for (int index = 0; index < expectedTowers.size(); ++index) {
        const TowerRecord& expected = expectedTowers.at(index);
        const TowerRecord& actual = importedTowers.at(index);
        if (!verify(actual.index == expected.index, "Tower index mismatch")) {
            return false;
        }
        if (!verify(actual.name == expected.name, "Tower name mismatch")) {
            return false;
        }
        if (!verify(actual.towerType == expected.towerType, "Tower type mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.x - expected.point.x) < 1e-4f, "Tower X mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.y - expected.point.y) < 1e-4f, "Tower Y mismatch")) {
            return false;
        }
        if (!verify(std::fabs(actual.point.z - expected.point.z) < 1e-4f, "Tower Z mismatch")) {
            return false;
        }
    }

    std::cout << "[PASS] Tower file interop smoke test completed." << std::endl;
    return true;
}

bool runTowerProjectLinkSmoke(const QStringList&)
{
    const QString sourceTowerFilePath = QFileInfo(
        QDir::current().absoluteFilePath(QStringLiteral("templates/tower.LiTower"))).absoluteFilePath();
    if (!verify(QFileInfo::exists(sourceTowerFilePath), "templates/tower.LiTower should exist")) {
        return false;
    }

    QList<TowerRecord> importedTowers;
    QString errorMessage;
    if (!importTowerLiTowerFile(sourceTowerFilePath, &importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] importTowerLiTowerFile: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    if (!verify(importedTowers.size() >= 4, "Expected at least 4 towers from template")) {
        return false;
    }
    if (!verify(importedTowers.first().index == 44, "Template first index should be 44 before editing")) {
        return false;
    }

    normalizeTowerIndices(&importedTowers);
    if (!verify(importedTowers.first().index == 0, "After edit normalization, first index should be 0")) {
        return false;
    }
    if (!verify(importedTowers.last().index == importedTowers.size() - 1, "After edit normalization, last index should be N-1")) {
        return false;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return false;
    }

    const QString projectFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_project.lpproj"));
    const QString linkedTowerFilePath = QDir(tempDir.path()).filePath(QStringLiteral("tower_linked.LiTower"));
    if (!exportTowerLiTowerFile(linkedTowerFilePath, importedTowers, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile initial: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    QJsonArray towersArray;
    for (const TowerRecord& towerRecord : importedTowers) {
        towersArray.append(towerRecordToJson(towerRecord));
    }

    QJsonArray pointCloudFilesArray;
    pointCloudFilesArray.append(QStringLiteral("./test_data/ezhou_powerline_sample.las"));
    QJsonObject towerFileObject {
        { QStringLiteral("format"), QStringLiteral("LiTower") },
        { QStringLiteral("relativePath"), QStringLiteral("./tower_linked.LiTower") }
    };

    QJsonObject projectObject {
        { QStringLiteral("version"), 8 },
        { QStringLiteral("pointCloudFilePaths"), pointCloudFilesArray },
        { QStringLiteral("towerFile"), towerFileObject },
        { QStringLiteral("towerMarkers"), towersArray }
    };

    QFile projectFile(projectFilePath);
    if (!verify(projectFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "Project file should be writable")) {
        return false;
    }
    projectFile.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    projectFile.close();

    QFile projectFileRead(projectFilePath);
    if (!verify(projectFileRead.open(QIODevice::ReadOnly), "Project file should be readable")) {
        return false;
    }
    const QJsonDocument loadedDocument = QJsonDocument::fromJson(projectFileRead.readAll());
    projectFileRead.close();
    if (!verify(loadedDocument.isObject(), "Loaded project JSON must be an object")) {
        return false;
    }

    const QJsonObject loadedProject = loadedDocument.object();
    const QString loadedRelativeTowerPath = loadedProject.value(QStringLiteral("towerFile")).toObject().value(QStringLiteral("relativePath")).toString();
    const QString resolvedTowerPath = resolveProjectPath(projectFilePath, loadedRelativeTowerPath);
    if (!verify(QFileInfo::exists(resolvedTowerPath), "Resolved linked tower file should exist")) {
        return false;
    }

    QList<TowerRecord> loadedTowerRecords;
    const QJsonArray loadedTowersArray = loadedProject.value(QStringLiteral("towerMarkers")).toArray();
    for (const QJsonValue& towerValue : loadedTowersArray) {
        loadedTowerRecords.append(towerRecordFromJson(towerValue.toObject()));
    }
    if (!verify(loadedTowerRecords.size() == importedTowers.size(), "Loaded tower record count mismatch")) {
        return false;
    }
    if (!verify(loadedTowerRecords.first().index == 0, "Loaded project first index should be 0")) {
        return false;
    }

    if (!exportTowerLiTowerFile(resolvedTowerPath, loadedTowerRecords, &errorMessage)) {
        std::cerr << "[FAIL] exportTowerLiTowerFile sync: " << errorMessage.toStdString() << std::endl;
        return false;
    }

    QFile linkedTowerFile(resolvedTowerPath);
    if (!verify(linkedTowerFile.open(QIODevice::ReadOnly | QIODevice::Text), "Linked tower file should be readable")) {
        return false;
    }
    QTextStream stream(&linkedTowerFile);
    stream.setCodec("UTF-8");
    const QString header = stream.readLine().trimmed();
    const QString firstRow = stream.readLine().trimmed();
    linkedTowerFile.close();

    if (!verify(header == QStringLiteral("Index,X,Y,Z,Type,Name"), "Linked tower header should match LiTower format")) {
        return false;
    }
    if (!verify(firstRow.startsWith(QStringLiteral("0,")), "First linked tower row should start with index 0")) {
        return false;
    }

    std::cout << "[PASS] Tower project link smoke test completed." << std::endl;
    return true;
}

QSet<QString> parseCsvValues(const QStringList& rawValues)
{
    QSet<QString> values;
    for (const QString& raw : rawValues) {
        const QStringList split = raw.split(',', Qt::SkipEmptyParts);
        for (const QString& item : split) {
            const QString normalized = item.trimmed().toLower();
            if (!normalized.isEmpty()) {
                values.insert(normalized);
            }
        }
    }
    return values;
}

void printUsageSummary()
{
    std::cout
        << "Modes: viewer-render, route-json, route-interop, route-roam, tower-file, tower-project-link, all" << std::endl
        << "Categories: render, route, tower, all" << std::endl
        << "Examples:" << std::endl
        << "  LASViewerSmokeTest --mode route-roam --las .\\test_data\\ezhou_powerline_sample.las" << std::endl
        << "  LASViewerSmokeTest --category route --las .\\test_data\\ezhou_powerline_sample.las" << std::endl
        << "  LASViewerSmokeTest --mode all --las .\\test_data\\ezhou_powerline_sample.las" << std::endl;
}

QStringList resolveLasInputs(const QCommandLineParser& parser)
{
    QStringList lasFiles = parser.values(QStringLiteral("las"));
    const QStringList positional = parser.positionalArguments();
    for (const QString& argument : positional) {
        if (!argument.startsWith('-')) {
            lasFiles.append(argument);
        }
    }

    if (lasFiles.isEmpty()) {
        lasFiles.append(QStringLiteral("./test_data/ezhou_powerline_sample.las"));
    }

    for (QString& lasFile : lasFiles) {
        lasFile = QDir::fromNativeSeparators(lasFile.trimmed());
    }

    return lasFiles;
}

bool validateSelections(const QSet<QString>& modeSet, const QSet<QString>& categorySet)
{
    const QSet<QString> validModes {
        QStringLiteral("viewer-render"),
        QStringLiteral("route-json"),
        QStringLiteral("route-interop"),
        QStringLiteral("route-roam"),
        QStringLiteral("tower-file"),
        QStringLiteral("tower-project-link"),
        QStringLiteral("all")
    };
    const QSet<QString> validCategories {
        QStringLiteral("render"),
        QStringLiteral("route"),
        QStringLiteral("tower"),
        QStringLiteral("all")
    };

    for (const QString& mode : modeSet) {
        if (!validModes.contains(mode)) {
            std::cerr << "Invalid mode: " << mode.toStdString() << std::endl;
            return false;
        }
    }
    for (const QString& category : categorySet) {
        if (!validCategories.contains(category)) {
            std::cerr << "Invalid category: " << category.toStdString() << std::endl;
            return false;
        }
    }

    if ((modeSet.size() > 1 && modeSet.contains(QStringLiteral("all")))
        || (categorySet.size() > 1 && categorySet.contains(QStringLiteral("all")))) {
        std::cerr << "Invalid argument: all cannot be combined with other values." << std::endl;
        return false;
    }

    return true;
}

bool runSelectedSmokes(
    const QList<SmokeCase>& cases,
    const QSet<QString>& modeSet,
    const QSet<QString>& categorySet,
    const QStringList& lasFiles)
{
    QList<SmokeCase> selectedCases;
    const bool selectAllByDefault = modeSet.isEmpty() && categorySet.isEmpty();

    for (const SmokeCase& smokeCase : cases) {
        const bool modeMatched = modeSet.contains(QStringLiteral("all")) || modeSet.contains(smokeCase.mode);
        const bool categoryMatched = categorySet.contains(QStringLiteral("all")) || categorySet.contains(smokeCase.category);
        if (selectAllByDefault || modeMatched || categoryMatched) {
            selectedCases.append(smokeCase);
        }
    }

    if (selectedCases.isEmpty()) {
        std::cerr << "No smoke test selected. Please provide valid --mode or --category." << std::endl;
        printUsageSummary();
        return false;
    }

    bool allPassed = true;
    int passCount = 0;
    int failCount = 0;

    for (const SmokeCase& smokeCase : selectedCases) {
        std::cout << "[RUN] " << smokeCase.displayName.toStdString()
                  << " (mode=" << smokeCase.mode.toStdString()
                  << ", category=" << smokeCase.category.toStdString() << ")" << std::endl;

        if (smokeCase.requiresLas) {
            for (const QString& lasFile : lasFiles) {
                if (!QFileInfo::exists(lasFile)) {
                    std::cerr << "[FAIL] Required LAS/LAZ file not found: " << lasFile.toStdString() << std::endl;
                    return false;
                }
            }
        }

        const bool casePassed = smokeCase.run(lasFiles);
        if (casePassed) {
            ++passCount;
            std::cout << "[PASS] " << smokeCase.displayName.toStdString() << std::endl;
        } else {
            ++failCount;
            allPassed = false;
            std::cout << "[FAIL] " << smokeCase.displayName.toStdString() << std::endl;
        }
    }

    std::cout << "Smoke summary: selected=" << selectedCases.size()
              << " passed=" << passCount
              << " failed=" << failCount << std::endl;
    return allPassed;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setVersion(2, 1);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("LASViewerSmokeTest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Unified smoke test runner for LAS Point Cloud Viewer"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(
        QStringList() << QStringLiteral("m") << QStringLiteral("mode"),
        QStringLiteral("Run by mode. Supports comma-separated values."),
        QStringLiteral("mode")));
    parser.addOption(QCommandLineOption(
        QStringList() << QStringLiteral("c") << QStringLiteral("category"),
        QStringLiteral("Run by category. Supports comma-separated values."),
        QStringLiteral("category")));
    parser.addOption(QCommandLineOption(
        QStringList() << QStringLiteral("l") << QStringLiteral("las"),
        QStringLiteral("LAS/LAZ input path. Repeatable for multiple files."),
        QStringLiteral("path")));
    parser.addPositionalArgument(
        QStringLiteral("las_files"),
        QStringLiteral("Optional LAS/LAZ file list used by rendering and route-roam modes."));
    parser.process(app);

    const QSet<QString> modeSet = parseCsvValues(parser.values(QStringLiteral("mode")));
    const QSet<QString> categorySet = parseCsvValues(parser.values(QStringLiteral("category")));
    if (!validateSelections(modeSet, categorySet)) {
        printUsageSummary();
        return 2;
    }

    const QStringList lasFiles = resolveLasInputs(parser);

    const QList<SmokeCase> smokeCases {
        SmokeCase {
            QStringLiteral("viewer-render"),
            QStringLiteral("render"),
            QStringLiteral("Viewer Render Smoke"),
            true,
            runViewerRenderSmoke },
        SmokeCase {
            QStringLiteral("route-json"),
            QStringLiteral("route"),
            QStringLiteral("Route Json Smoke"),
            false,
            runRouteJsonSmoke },
        SmokeCase {
            QStringLiteral("route-interop"),
            QStringLiteral("route"),
            QStringLiteral("Route Interop Smoke"),
            false,
            runRouteInteropSmoke },
        SmokeCase {
            QStringLiteral("route-roam"),
            QStringLiteral("route"),
            QStringLiteral("Route Roam State Smoke"),
            true,
            runRouteRoamStateSmoke },
        SmokeCase {
            QStringLiteral("tower-file"),
            QStringLiteral("tower"),
            QStringLiteral("Tower File Interop Smoke"),
            false,
            runTowerFileInteropSmoke },
        SmokeCase {
            QStringLiteral("tower-project-link"),
            QStringLiteral("tower"),
            QStringLiteral("Tower Project Link Smoke"),
            false,
            runTowerProjectLinkSmoke }
    };

    const bool allPassed = runSelectedSmokes(smokeCases, modeSet, categorySet, lasFiles);
    return allPassed ? 0 : 1;
}
