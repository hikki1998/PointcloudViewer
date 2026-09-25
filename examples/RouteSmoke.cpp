#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QOpenGLWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QSlider>
#include <QMouseEvent>
#include <QMenu>
#include <QSpinBox>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTranslator>
#include <QTimer>
#include <QLineEdit>

#include <osgGA/TrackballManipulator>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

#include "crs/CrsAuthorityService.h"
#include "domain/InspectionData.h"
#include "domain/TowerFileInterop.h"
#include "gui/ApplicationLogDock.h"
#include "gui/IssueController.h"
#define private public
#include "gui/MainWindow.h"
#include "gui/PointCloudViewer.h"
#undef private
#include "gui/MeasurementAnalysisController.h"
#include "gui/NavigationSettingsWidget.h"
#include "gui/ProfileClassificationController.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProfileClassificationWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteDetailsDock.h"
#include "gui/RouteController.h"
#include "gui/SceneInspectorDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/TowerController.h"
#include "gui/VisualizationPanelController.h"
#include "logging/ApplicationLogger.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"
#include "route/RouteInterop.h"

#include "QtnRibbonBackstageView.h"
#include "QtnRibbonBar.h"
#include "QtnRibbonSystemPopupBar.h"

#include "SmokeTestSupport.h"

bool runRouteControllerSmoke(const QStringList&)
{
    PointCloudViewer viewer;

    QAction generateInspectionRouteAction(QStringLiteral("Generate"), &viewer);
    QAction regenerateInspectionRouteAction(QStringLiteral("Regenerate"), &viewer);
    QAction clearInspectionRouteAction(QStringLiteral("Clear Route"), &viewer);
    QAction toggleRouteEditingAction(QStringLiteral("Edit"), &viewer);
    toggleRouteEditingAction.setCheckable(true);
    QAction startInspectionRouteRoamAction(QStringLiteral("Start Roam"), &viewer);
    QAction pauseInspectionRouteRoamAction(QStringLiteral("Pause Roam"), &viewer);
    QAction stopInspectionRouteRoamAction(QStringLiteral("Stop Roam"), &viewer);
    QAction focusRouteWaypointAction(QStringLiteral("Focus Waypoint"), &viewer);
    QAction importRouteFileAction(QStringLiteral("Import Route"), &viewer);
    QAction saveRouteFileAction(QStringLiteral("Save Route"), &viewer);
    QAction saveRouteFileAsAction(QStringLiteral("Save Route As"), &viewer);
    QAction reloadRouteFileAction(QStringLiteral("Reload Route"), &viewer);
    QAction importRouteKmlAction(QStringLiteral("Import KML"), &viewer);
    QAction exportRouteKmlAction(QStringLiteral("Export KML"), &viewer);
    QAction exportRouteDjiKmzAction(QStringLiteral("Export KMZ"), &viewer);

    QPushButton routeRoamStartButton(QStringLiteral("Start"));
    QPushButton routeRoamPauseResumeButton(QStringLiteral("Pause"));
    QPushButton routeRoamStopButton(QStringLiteral("Stop"));
    QDoubleSpinBox routeRoamSpeedSpinBox;
    QComboBox routeRoamViewModeComboBox;
    routeRoamViewModeComboBox.addItem(QStringLiteral("First"), 0);
    routeRoamViewModeComboBox.addItem(QStringLiteral("Third"), 1);

    int regenerateInspectionRouteCount = 0;
    int clearInspectionRouteCount = 0;
    int setRouteEditingEnabledCount = 0;
    bool latestRouteEditingEnabled = false;
    int startInspectionRouteRoamCount = 0;
    int pauseResumeInspectionRouteRoamCount = 0;
    int stopInspectionRouteRoamCount = 0;
    double latestRouteRoamSpeed = 0.0;
    int latestRouteRoamViewModeIndex = -1;
    int focusRouteWaypointCount = 0;
    int importRouteFileCount = 0;
    int saveRouteFileCount = 0;
    int saveRouteFileAsCount = 0;
    int reloadRouteFileCount = 0;
    int importRouteKmlCount = 0;
    int exportRouteKmlCount = 0;
    int exportRouteDjiKmzCount = 0;
    int inspectionRouteRoamStateChangedCount = 0;
    int inspectionRouteRoamPhotoCapturedCount = 0;

    QList<PointRecord> roamWaypoints;
    PointRecord first;
    first.x = 0.0f;
    first.y = 0.0f;
    first.z = 30.0f;
    roamWaypoints.append(first);
    PointRecord second;
    second.x = 120.0f;
    second.y = 40.0f;
    second.z = 32.0f;
    roamWaypoints.append(second);
    PointRecord third;
    third.x = 240.0f;
    third.y = 80.0f;
    third.z = 35.0f;
    roamWaypoints.append(third);

    RouteController controller(
        &viewer,
        &generateInspectionRouteAction,
        &regenerateInspectionRouteAction,
        &clearInspectionRouteAction,
        &toggleRouteEditingAction,
        &startInspectionRouteRoamAction,
        &pauseInspectionRouteRoamAction,
        &stopInspectionRouteRoamAction,
        &focusRouteWaypointAction,
        &importRouteFileAction,
        &saveRouteFileAction,
        &saveRouteFileAsAction,
        &reloadRouteFileAction,
        &importRouteKmlAction,
        &exportRouteKmlAction,
        &exportRouteDjiKmzAction,
        &routeRoamStartButton,
        &routeRoamPauseResumeButton,
        &routeRoamStopButton,
        &routeRoamSpeedSpinBox,
        &routeRoamViewModeComboBox,
        [&regenerateInspectionRouteCount]() { ++regenerateInspectionRouteCount; },
        [&clearInspectionRouteCount]() { ++clearInspectionRouteCount; },
        [&setRouteEditingEnabledCount, &latestRouteEditingEnabled](bool enabled) {
            ++setRouteEditingEnabledCount;
            latestRouteEditingEnabled = enabled;
        },
        [&startInspectionRouteRoamCount]() { ++startInspectionRouteRoamCount; },
        [&pauseResumeInspectionRouteRoamCount]() { ++pauseResumeInspectionRouteRoamCount; },
        [&stopInspectionRouteRoamCount]() { ++stopInspectionRouteRoamCount; },
        [&latestRouteRoamSpeed](double speed) { latestRouteRoamSpeed = speed; },
        [&latestRouteRoamViewModeIndex](int index) { latestRouteRoamViewModeIndex = index; },
        [&focusRouteWaypointCount]() { ++focusRouteWaypointCount; },
        [&importRouteFileCount]() { ++importRouteFileCount; },
        [&saveRouteFileCount]() { ++saveRouteFileCount; },
        [&saveRouteFileAsCount]() { ++saveRouteFileAsCount; },
        [&reloadRouteFileCount]() { ++reloadRouteFileCount; },
        [&importRouteKmlCount]() { ++importRouteKmlCount; },
        [&exportRouteKmlCount]() { ++exportRouteKmlCount; },
        [&exportRouteDjiKmzCount]() { ++exportRouteDjiKmzCount; },
        [&inspectionRouteRoamStateChangedCount]() { ++inspectionRouteRoamStateChangedCount; },
        [&inspectionRouteRoamPhotoCapturedCount](int, int, const QString&, int) {
            ++inspectionRouteRoamPhotoCapturedCount;
        });
    Q_UNUSED(controller);

    generateInspectionRouteAction.trigger();
    regenerateInspectionRouteAction.trigger();
    if (!verify(regenerateInspectionRouteCount == 2, "Route controller should route generate/regenerate actions")) {
        return false;
    }

    clearInspectionRouteAction.trigger();
    if (!verify(clearInspectionRouteCount == 1, "Route controller should route clear route action")) {
        return false;
    }

    toggleRouteEditingAction.setChecked(true);
    if (!verify(setRouteEditingEnabledCount == 1 && latestRouteEditingEnabled, "Route controller should route route-edit toggles")) {
        return false;
    }

    startInspectionRouteRoamAction.trigger();
    pauseInspectionRouteRoamAction.trigger();
    stopInspectionRouteRoamAction.trigger();
    if (!verify(startInspectionRouteRoamCount == 1, "Route controller should route start roam action")) {
        return false;
    }
    if (!verify(pauseResumeInspectionRouteRoamCount == 1, "Route controller should route pause/resume roam action")) {
        return false;
    }
    if (!verify(stopInspectionRouteRoamCount == 1, "Route controller should route stop roam action")) {
        return false;
    }

    routeRoamStartButton.click();
    routeRoamPauseResumeButton.click();
    routeRoamStopButton.click();
    if (!verify(startInspectionRouteRoamCount == 2, "Route controller should bridge start roam button")) {
        return false;
    }
    if (!verify(pauseResumeInspectionRouteRoamCount == 2, "Route controller should bridge pause/resume roam button")) {
        return false;
    }
    if (!verify(stopInspectionRouteRoamCount == 2, "Route controller should bridge stop roam button")) {
        return false;
    }

    routeRoamSpeedSpinBox.setValue(6.5);
    routeRoamViewModeComboBox.setCurrentIndex(1);
    if (!verifyClose(latestRouteRoamSpeed, 6.5, 0.001, "Route controller should route roam speed changes")) {
        return false;
    }
    if (!verify(latestRouteRoamViewModeIndex == 1, "Route controller should route roam view mode changes")) {
        return false;
    }

    focusRouteWaypointAction.trigger();
    importRouteFileAction.trigger();
    saveRouteFileAction.trigger();
    saveRouteFileAsAction.trigger();
    reloadRouteFileAction.trigger();
    importRouteKmlAction.trigger();
    exportRouteKmlAction.trigger();
    exportRouteDjiKmzAction.trigger();
    if (!verify(focusRouteWaypointCount == 1, "Route controller should route focus waypoint action")) {
        return false;
    }
    if (!verify(importRouteFileCount == 1, "Route controller should route import route action")) {
        return false;
    }
    if (!verify(saveRouteFileCount == 1, "Route controller should route save route action")) {
        return false;
    }
    if (!verify(saveRouteFileAsCount == 1, "Route controller should route save-as route action")) {
        return false;
    }
    if (!verify(reloadRouteFileCount == 1, "Route controller should route reload route action")) {
        return false;
    }
    if (!verify(importRouteKmlCount == 1, "Route controller should route import KML action")) {
        return false;
    }
    if (!verify(exportRouteKmlCount == 1, "Route controller should route export KML action")) {
        return false;
    }
    if (!verify(exportRouteDjiKmzCount == 1, "Route controller should route export KMZ action")) {
        return false;
    }

    if (!verify(
            inspectionRouteRoamStateChangedCount == 0,
            "Route controller roam state callback should remain idle without viewer roam transitions")) {
        return false;
    }
    if (!verify(
            inspectionRouteRoamPhotoCapturedCount == 0,
            "Route controller photo callback should remain idle when no viewer photo capture occurs")) {
        return false;
    }

    std::cout << "[PASS] Route controller smoke test completed." << std::endl;
    return true;
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
