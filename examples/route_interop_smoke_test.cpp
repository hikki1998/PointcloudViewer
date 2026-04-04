#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <iostream>

#include "crs/CrsAuthorityService.h"
#include "domain/InspectionRoutePlanning.h"
#include "domain/RouteInterop.h"

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
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const lasviewer::crs::CrsResolveResult authorityResult =
        lasviewer::crs::CrsAuthorityService::resolveFromAuthority(QStringLiteral("EPSG"), 4326);
    if (!verify(authorityResult.ok, "EPSG:4326 should resolve from authority database")) {
        return 1;
    }
    if (!verify(!authorityResult.definition.reference.wkt.trimmed().isEmpty(), "Resolved EPSG:4326 should provide WKT")) {
        return 1;
    }

    const lasviewer::crs::CrsResolveResult wktResult =
        lasviewer::crs::CrsAuthorityService::resolveFromWkt(authorityResult.definition.reference.wkt);
    if (!verify(wktResult.ok, "WKT should resolve back to a CRS definition")) {
        return 1;
    }
    if (!verify(
            wktResult.definition.reference.authName.compare(QStringLiteral("EPSG"), Qt::CaseInsensitive) == 0
                && wktResult.definition.reference.code == 4326,
            "WKT should identify back to EPSG:4326")) {
        return 1;
    }

    const QList<lasviewer::crs::CoordinateSystemDefinition> nameMatches =
        lasviewer::crs::CrsAuthorityService::findByName(
            QStringLiteral("WGS 84"),
            lasviewer::crs::CoordinateSystemKindFilter::Geographic,
            5);
    if (!verify(!nameMatches.isEmpty(), "Name lookup for WGS 84 should return candidates")) {
        return 1;
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
        return 1;
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
    if (!transformRouteToWgs84(localRoute, coordinateSystems, &routeWgs84, &errorMessage)) {
        std::cerr << "[FAIL] transformRouteToWgs84: " << errorMessage.toStdString() << std::endl;
        return 1;
    }
    if (!verify(routeWgs84.waypoints.size() == localRoute.waypoints.size(), "Transformed route size mismatch")) {
        return 1;
    }

    QTemporaryDir tempDir;
    if (!verify(tempDir.isValid(), "Failed to create temporary directory")) {
        return 1;
    }

    const QString kmlPath = QDir(tempDir.path()).filePath(QStringLiteral("route_test.kml"));
    if (!exportRouteKml(kmlPath, routeWgs84, &errorMessage)) {
        std::cerr << "[FAIL] exportRouteKml: " << errorMessage.toStdString() << std::endl;
        return 1;
    }
    if (!verify(QFile::exists(kmlPath), "KML file should exist")) {
        return 1;
    }

    InspectionRoute importedRouteWgs84;
    if (!importRouteKml(kmlPath, &importedRouteWgs84, &errorMessage)) {
        std::cerr << "[FAIL] importRouteKml: " << errorMessage.toStdString() << std::endl;
        return 1;
    }
    if (!verify(
            importedRouteWgs84.waypoints.size() == routeWgs84.waypoints.size(),
            "KML roundtrip waypoint size mismatch")) {
        return 1;
    }

    const QString kmzPath = QDir(tempDir.path()).filePath(QStringLiteral("route_test.kmz"));
    if (!exportRouteDjiKmz(kmzPath, routeWgs84, planningOptions, &errorMessage)) {
        std::cerr << "[FAIL] exportRouteDjiKmz: " << errorMessage.toStdString() << std::endl;
        return 1;
    }
    if (!verify(QFile::exists(kmzPath), "KMZ file should exist")) {
        return 1;
    }

    QFile kmzFile(kmzPath);
    if (!verify(kmzFile.open(QIODevice::ReadOnly), "KMZ file should be readable")) {
        return 1;
    }
    const QByteArray kmzData = kmzFile.readAll();
    kmzFile.close();
    if (!verify(kmzData.contains("wpmz/template.kml"), "KMZ should contain template.kml entry")) {
        return 1;
    }
    if (!verify(kmzData.contains("wpmz/waylines.wpml"), "KMZ should contain waylines.wpml entry")) {
        return 1;
    }

    std::cout << "[PASS] Route interop smoke test completed." << std::endl;
    return 0;
}
