#include "gui/MainWindow.h"

#include <QAction>
#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPointF>
#include <QSignalBlocker>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "crs/CrsAuthorityService.h"
#include "crs/CrsTransformService.h"
#include "domain/DataManager.h"
#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"
#include "gui/RouteDetailsDock.h"

#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"

using lasviewer::crs::CrsAuthorityService;
using lasviewer::crs::CrsTransformService;
using namespace mainwindow_internal;

namespace
{
QString routePartDisplayName(const RoutePartPoint& partPoint)
{
    return partPoint.partName.trimmed().isEmpty()
        ? QCoreApplication::translate("MainWindow", "Part %1").arg(QLocale().toString(partPoint.partIndex))
        : partPoint.partName.trimmed();
}

QString routeWaypointPartSummary(const RouteWaypoint& waypoint, const QHash<int, RoutePartPoint>& partPointByIndex)
{
    QStringList partNames;
    for (const RouteCaptureTarget& captureTarget : waypoint.captureTargets) {
        if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
            const QString partName = routePartDisplayName(partPointByIndex.value(captureTarget.partIndex));
            if (!partNames.contains(partName)) {
                partNames.append(partName);
            }
        } else if (!captureTarget.partName.trimmed().isEmpty() && !partNames.contains(captureTarget.partName.trimmed())) {
            partNames.append(captureTarget.partName.trimmed());
        }
    }

    if (partNames.isEmpty() && waypoint.primaryPartIndex > 0 && partPointByIndex.contains(waypoint.primaryPartIndex)) {
        partNames.append(routePartDisplayName(partPointByIndex.value(waypoint.primaryPartIndex)));
    }

    if (partNames.isEmpty()) {
        return waypoint.isHelperWaypoint
            ? QCoreApplication::translate("MainWindow", "Helper Waypoint")
            : QCoreApplication::translate("MainWindow", "Unlinked");
    }

    return partNames.join(QStringLiteral(", "));
}

int routeWaypointRepresentativePartIndex(const RouteWaypoint& waypoint)
{
    if (!waypoint.captureTargets.isEmpty() && waypoint.captureTargets.first().partIndex > 0) {
        return waypoint.captureTargets.first().partIndex;
    }
    return waypoint.primaryPartIndex;
}

bool captureTargetHasMeaningfulLocalPoint(const RouteCaptureTarget& captureTarget)
{
    return captureTarget.partIndex > 0
        || !captureTarget.partName.trimmed().isEmpty()
        || !qFuzzyIsNull(static_cast<double>(captureTarget.targetLocalPoint.x))
        || !qFuzzyIsNull(static_cast<double>(captureTarget.targetLocalPoint.y))
        || !qFuzzyIsNull(static_cast<double>(captureTarget.targetLocalPoint.z));
}

double normalizedRouteFocalLengthRatio(double ratio)
{
    if (!std::isfinite(ratio) || ratio <= 0.0) {
        return 1.0;
    }
    return std::clamp(ratio, 0.1, 64.0);
}

QString routeCaptureTargetDisplayName(const RouteCaptureTarget& captureTarget, const QHash<int, RoutePartPoint>& partPointByIndex, int targetSequence)
{
    if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
        return routePartDisplayName(partPointByIndex.value(captureTarget.partIndex));
    }
    if (!captureTarget.partName.trimmed().isEmpty()) {
        return captureTarget.partName.trimmed();
    }
    return QCoreApplication::translate("MainWindow", "Target %1").arg(QLocale().toString(targetSequence));
}

QColor routeQaSeverityColor(RouteQaSeverity severity)
{
    switch (severity) {
    case RouteQaSeverity::Blocking:
        return QColor(185, 28, 28);
    case RouteQaSeverity::Warning:
        return QColor(180, 83, 9);
    case RouteQaSeverity::Info:
    default:
        return QColor(30, 64, 175);
    }
}

QString routeQaIssueLocationText(const RouteQaIssue& issue)
{
    if (issue.relatedWaypointIndex >= 0 && issue.waypointIndex >= 0) {
        return QCoreApplication::translate("MainWindow", "WP %1 -> WP %2")
            .arg(QLocale().toString(issue.relatedWaypointIndex + 1), QLocale().toString(issue.waypointIndex + 1));
    }
    if (issue.waypointIndex >= 0) {
        return QCoreApplication::translate("MainWindow", "WP %1").arg(QLocale().toString(issue.waypointIndex + 1));
    }
    if (issue.partIndex > 0) {
        return QCoreApplication::translate("MainWindow", "Part %1").arg(QLocale().toString(issue.partIndex));
    }
    return QCoreApplication::translate("MainWindow", "Global");
}

QString routeQaSummaryText(const RouteQaReport& report)
{
    if (report.issues.isEmpty()) {
        return QCoreApplication::translate("MainWindow", "Route QA passed with no issues.");
    }
    return QCoreApplication::translate("MainWindow", "Blocking: %1 | Warning: %2 | Info: %3")
        .arg(QLocale().toString(report.blockingIssueCount), QLocale().toString(report.warningIssueCount), QLocale().toString(report.infoIssueCount));
}

InspectionRouteDisplayData buildInspectionRouteDisplayData(const PowerlineRouteDocument& route)
{
    InspectionRouteDisplayData displayData;
    displayData.waypoints = toRouteDisplayPoints(route);
    displayData.labels = toRouteDisplayLabels(route);
    displayData.partPoints.reserve(route.partPoints.size());
    displayData.partLabels.reserve(route.partPoints.size());
    displayData.partPointIndices.reserve(route.partPoints.size());
    displayData.waypointTargetPoints.reserve(route.waypoints.size());
    displayData.waypointHasTargetPoints.reserve(route.waypoints.size());
    displayData.waypointAircraftYawDegs.reserve(route.waypoints.size());
    displayData.waypointGimbalPitchDegs.reserve(route.waypoints.size());
    displayData.waypointCameraYawDegs.reserve(route.waypoints.size());
    displayData.waypointCameraPitchDegs.reserve(route.waypoints.size());
    displayData.waypointFocalLengthRatios.reserve(route.waypoints.size());
    displayData.waypointTargetLabels.reserve(route.waypoints.size());
    displayData.waypointAllTargetPoints.reserve(route.waypoints.size());
    displayData.waypointAllTargetPartIndices.reserve(route.waypoints.size());
    displayData.waypointAllCameraYawDegs.reserve(route.waypoints.size());
    displayData.waypointAllCameraPitchDegs.reserve(route.waypoints.size());
    displayData.waypointAllFocalLengthRatios.reserve(route.waypoints.size());
    displayData.waypointAllTargetLabels.reserve(route.waypoints.size());

    QHash<int, PointRecord> partPointByIndex;
    QHash<int, RoutePartPoint> routePartPointByIndex;
    for (const RoutePartPoint& partPoint : route.partPoints) {
        displayData.partPoints.append(partPoint.localPoint);
        displayData.partLabels.append(routePartDisplayName(partPoint));
        displayData.partPointIndices.append(partPoint.partIndex);
        if (partPoint.partIndex > 0) {
            partPointByIndex.insert(partPoint.partIndex, partPoint.localPoint);
            routePartPointByIndex.insert(partPoint.partIndex, partPoint);
        }
    }

    for (const RouteWaypoint& waypoint : route.waypoints) {
        const int targetPartIndex = routeWaypointRepresentativePartIndex(waypoint);
        const RouteCaptureTarget primaryTarget = waypoint.captureTargets.isEmpty() ? RouteCaptureTarget() : waypoint.captureTargets.first();
        QList<PointRecord> allTargetPoints;
        QList<int> allTargetPartIndices;
        QList<double> allCameraYawDegs;
        QList<double> allCameraPitchDegs;
        QList<double> allFocalLengthRatios;
        QStringList allTargetLabels;

        for (const RouteCaptureTarget& captureTarget : waypoint.captureTargets) {
            PointRecord resolvedTargetPoint;
            bool hasTargetPoint = false;
            if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
                resolvedTargetPoint = partPointByIndex.value(captureTarget.partIndex);
                hasTargetPoint = true;
            } else if (captureTargetHasMeaningfulLocalPoint(captureTarget)) {
                resolvedTargetPoint = captureTarget.targetLocalPoint;
                hasTargetPoint = true;
            }
            if (!hasTargetPoint) {
                continue;
            }
            allTargetPoints.append(resolvedTargetPoint);
            allTargetPartIndices.append(captureTarget.partIndex);
            allCameraYawDegs.append(captureTarget.cameraYawDeg);
            allCameraPitchDegs.append(captureTarget.cameraPitchDeg);
            allFocalLengthRatios.append(normalizedRouteFocalLengthRatio(captureTarget.focalLengthRatio));
            allTargetLabels.append(routeCaptureTargetDisplayName(captureTarget, routePartPointByIndex, allTargetLabels.size() + 1));
        }

        displayData.waypointAllTargetPoints.append(allTargetPoints);
        displayData.waypointAllTargetPartIndices.append(allTargetPartIndices);
        displayData.waypointAllCameraYawDegs.append(allCameraYawDegs);
        displayData.waypointAllCameraPitchDegs.append(allCameraPitchDegs);
        displayData.waypointAllFocalLengthRatios.append(allFocalLengthRatios);
        displayData.waypointAllTargetLabels.append(allTargetLabels);

        if (!allTargetPoints.isEmpty()) {
            displayData.waypointHasTargetPoints.append(true);
            displayData.waypointTargetPoints.append(allTargetPoints.constFirst());
        } else if (targetPartIndex > 0 && partPointByIndex.contains(targetPartIndex)) {
            displayData.waypointHasTargetPoints.append(true);
            displayData.waypointTargetPoints.append(partPointByIndex.value(targetPartIndex));
        } else {
            displayData.waypointHasTargetPoints.append(false);
            displayData.waypointTargetPoints.append(PointRecord());
        }
        displayData.waypointAircraftYawDegs.append(waypoint.aircraftYawDeg);
        displayData.waypointGimbalPitchDegs.append(waypoint.gimbalPitchDeg);
        displayData.waypointCameraYawDegs.append(allCameraYawDegs.isEmpty() ? primaryTarget.cameraYawDeg : allCameraYawDegs.constFirst());
        displayData.waypointCameraPitchDegs.append(allCameraPitchDegs.isEmpty() ? primaryTarget.cameraPitchDeg : allCameraPitchDegs.constFirst());
        displayData.waypointFocalLengthRatios.append(allFocalLengthRatios.isEmpty() ? normalizedRouteFocalLengthRatio(primaryTarget.focalLengthRatio) : allFocalLengthRatios.constFirst());
        displayData.waypointTargetLabels.append(routeWaypointPartSummary(waypoint, routePartPointByIndex));
    }

    return displayData;
}
}

void MainWindow::syncRoutePlanningFromProjectCoordinateSystems()
{
    if (projectCoordinateSystems_.pointCloudCrs.authName.trimmed().isEmpty()) {
        projectCoordinateSystems_.pointCloudCrs.authName = QStringLiteral("EPSG");
    }
    projectCoordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;

    if (projectCoordinateSystems_.geographicCrs.authName.trimmed().isEmpty()) {
        projectCoordinateSystems_.geographicCrs.authName = QStringLiteral("EPSG");
    }

    if (projectCoordinateSystems_.geographicCrs.code <= 0) {
        projectCoordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
    }
    if (projectCoordinateSystems_.pointCloudCrs.code > 0) {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.pointCloudCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.pointCloudCrs = normalized;
        }
    }
    if (projectCoordinateSystems_.geographicCrs.code > 0) {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.geographicCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.geographicCrs = normalized;
        }
    }
    projectCoordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;
    projectCoordinateSystems_.geographicCrs.kind = CoordinateSystemKind::Geographic;

    routePlanningOptions_.crs.sourceEpsg = projectCoordinateSystems_.pointCloudCrs.code;
    routePlanningOptions_.crs.targetEpsg = projectCoordinateSystems_.geographicCrs.code;
}

void MainWindow::chooseRouteWaypointColor()
{
    if (viewer_ == nullptr) {
        return;
    }

    const QColor initialColor = viewer_->inspectionRouteWaypointColor();
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Waypoint Color"));
    if (!chosenColor.isValid()) {
        return;
    }

    viewer_->setInspectionRouteWaypointColor(chosenColor);
    setColorButtonAppearance(routeWaypointColorButton_, chosenColor, tr("Waypoint Color"));
    persistVisualizationSettings();
}

void MainWindow::chooseRoutePartPointColor()
{
    if (viewer_ == nullptr) {
        return;
    }

    const QColor initialColor = viewer_->inspectionRoutePartPointColor();
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Part Point Color"));
    if (!chosenColor.isValid()) {
        return;
    }

    viewer_->setInspectionRoutePartPointColor(chosenColor);
    setColorButtonAppearance(routePartPointColorButton_, chosenColor, tr("Part Point Color"));
    persistVisualizationSettings();
}

void MainWindow::chooseRouteTrajectoryColor()
{
    if (viewer_ == nullptr) {
        return;
    }

    const QColor initialColor = viewer_->inspectionRouteTrajectoryColor();
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Trajectory Color"));
    if (!chosenColor.isValid()) {
        return;
    }

    viewer_->setInspectionRouteTrajectoryColor(chosenColor);
    setColorButtonAppearance(routeTrajectoryColorButton_, chosenColor, tr("Trajectory Color"));
    persistVisualizationSettings();
}

void MainWindow::focusRoutePartPoint(int partIndex)
{
    if (viewer_ == nullptr || partIndex <= 0) {
        return;
    }

    for (const RoutePartPoint& partPoint : currentPowerlineRoute_.partPoints) {
        if (partPoint.partIndex == partIndex) {
            viewer_->focusOnPoint(partPoint.localPoint, 0.18);
            return;
        }
    }
}

void MainWindow::focusRouteWaypoint(int waypointIndex)
{
    if (viewer_ == nullptr
        || waypointIndex < 0
        || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
        return;
    }

    viewer_->focusOnPoint(currentPowerlineRoute_.waypoints.at(waypointIndex).localPoint, 0.22);
}

void MainWindow::focusRouteQaIssue(int issueIndex)
{
    if (issueIndex < 0 || issueIndex >= routeQaReport_.issues.size()) {
        return;
    }

    selectedRouteQaIssueIndex_ = issueIndex;
    const RouteQaIssue& issue = routeQaReport_.issues.at(issueIndex);
    if (issue.waypointIndex >= 0 && issue.waypointIndex < currentPowerlineRoute_.waypoints.size()) {
        focusRouteWaypoint(issue.waypointIndex);
        return;
    }

    if (issue.partIndex > 0) {
        focusRoutePartPoint(issue.partIndex);
        return;
    }

    showUserMessage(LogLevel::Info, tr("Selected QA issue has no spatial anchor."), 2600);
}

bool MainWindow::ensureRouteEditingEnabled(bool notify)
{
    if (routeEditingEnabled_) {
        return true;
    }

    if (notify) {
        showUserMessage(
            LogLevel::Warning,
            tr("Enable \"Edit Route\" in the Route ribbon page before modifying waypoints."),
            3600);
    }
    return false;
}

void MainWindow::setRouteEditingEnabled(bool enabled, bool notify)
{
    if (toggleRouteEditingAction_ != nullptr && toggleRouteEditingAction_->isChecked() != enabled) {
        const QSignalBlocker blocker(toggleRouteEditingAction_);
        toggleRouteEditingAction_->setChecked(enabled);
    }

    if (routeEditingEnabled_ == enabled) {
        if (viewer_ != nullptr) {
            viewer_->setInspectionRouteEditingEnabled(enabled);
        }
        return;
    }

    routeEditingEnabled_ = enabled;
    if (viewer_ != nullptr) {
        viewer_->setInspectionRouteEditingEnabled(enabled);
    }

    updateRoutePlanningPanel();
    updateActionState();
    persistWindowSettings();

    if (notify) {
        showUserMessage(
            LogLevel::Info,
            routeEditingEnabled_ ? tr("Route editing enabled.") : tr("Route editing locked."),
            2600);
    }
}

void MainWindow::handleRouteRoamPhotoCaptured(int waypointIndex, int targetIndex, const QString& targetLabel, int captureCount)
{
    const QString targetIndexText =
        targetIndex >= 0 ? QLocale().toString(targetIndex + 1) : tr("-");
    routeRoamLastCaptureSummary_ = tr("Photo %1 | WP %2 | Target %3 (%4)")
        .arg(
            QLocale().toString(captureCount),
            QLocale().toString(waypointIndex + 1),
            targetIndexText,
            targetLabel.trimmed().isEmpty() ? tr("Unlinked") : targetLabel);
    if (routeRoamFloatingCaptureLabel_ != nullptr) {
        routeRoamFloatingCaptureLabel_->setText(routeRoamLastCaptureSummary_);
    }
    showUserMessage(LogLevel::Info, routeRoamLastCaptureSummary_, 1800);
}

void MainWindow::showRouteDetailsDock(int preferredTab)
{
    if (routeDetailsDock_ == nullptr) {
        return;
    }

    if (routeDetailsTabWidget_ != nullptr && routeDetailsTabWidget_->count() > 0) {
        const int normalizedTab = std::clamp(preferredTab, 0, routeDetailsTabWidget_->count() - 1);
        routeDetailsTabWidget_->setCurrentIndex(normalizedTab);
    }

    routeDetailsDock_->show();
    routeDetailsDock_->raise();
}

bool MainWindow::importRouteFile(const QString& filePath, bool updateLink, bool notify)
{
    if (viewer_ == nullptr) {
        return false;
    }

    PowerlineRouteDocument importedRoute;
    QString errorMessage;
    if (!importPowerlineRouteJson(filePath, &importedRoute, &errorMessage)) {
        if (notify) {
            showUserMessage(
                LogLevel::Error,
                errorMessage.isEmpty() ? tr("Failed to import route file.") : errorMessage,
                5000);
        }
        return false;
    }

    currentPowerlineRoute_ = importedRoute;
    selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
    selectedRouteWaypointTargetIndex_ = -1;
    if (updateLink) {
        linkedRouteFilePath_ = QFileInfo(filePath).absoluteFilePath();
    }
    applyCurrentRouteToViewer();
    updateRoutePlanningPanel();
    rebuildProjectTree();
    updateActionState();
    showRouteDetailsDock(0);
    if (notify) {
        showUserMessage(
            LogLevel::Info,
            tr("Imported route file: %1").arg(QFileInfo(filePath).fileName()),
            3500);
    }
    return true;
}

bool MainWindow::exportRouteFile(const QString& filePath, bool updateLink, bool notify)
{
    if (viewer_ == nullptr) {
        return false;
    }

    QString normalizedPath = filePath;
    if (QFileInfo(normalizedPath).suffix().isEmpty()) {
        normalizedPath += QStringLiteral(".json");
    }

    PowerlineRouteDocument routeToSave = currentPowerlineRoute_;
    routeToSave.updatedAt = QDateTime::currentDateTimeUtc();

    QString errorMessage;
    if (!exportPowerlineRouteJson(normalizedPath, routeToSave, &errorMessage)) {
        if (notify) {
            showUserMessage(
                LogLevel::Error,
                errorMessage.isEmpty() ? tr("Failed to save route file.") : errorMessage,
                5000);
        }
        return false;
    }

    currentPowerlineRoute_ = routeToSave;
    if (updateLink) {
        linkedRouteFilePath_ = QFileInfo(normalizedPath).absoluteFilePath();
    }
    if (notify) {
        showUserMessage(
            LogLevel::Info,
            tr("Route file saved: %1").arg(QFileInfo(normalizedPath).fileName()),
            3000);
    }
    updateActionState();
    return true;
}

bool MainWindow::reloadLinkedRouteFile(bool notify)
{
    const QString linkedPath = linkedRouteFilePath_.trimmed();
    if (linkedPath.isEmpty()) {
        if (notify) {
            showUserMessage(LogLevel::Warning, tr("No linked route file to reload."), 3000);
        }
        return false;
    }

    if (!QFileInfo::exists(linkedPath)) {
        if (notify) {
            showUserMessage(LogLevel::Error, tr("Linked route file was not found."), 4000);
        }
        return false;
    }

    if (!importRouteFile(linkedPath, false, false)) {
        return false;
    }
    if (notify) {
        showUserMessage(
            LogLevel::Info,
            tr("Reloaded route file: %1").arg(QFileInfo(linkedPath).fileName()),
            3000);
    }
    return true;
}

bool MainWindow::removeRoutePartPoint(int partIndex, bool notify)
{
    if (partIndex <= 0) {
        return false;
    }
    if (!ensureRouteEditingEnabled(notify)) {
        return false;
    }

    int removeIndex = -1;
    RoutePartPoint removedPartPoint;
    for (int index = 0; index < currentPowerlineRoute_.partPoints.size(); ++index) {
        if (currentPowerlineRoute_.partPoints.at(index).partIndex == partIndex) {
            removeIndex = index;
            removedPartPoint = currentPowerlineRoute_.partPoints.at(index);
            break;
        }
    }
    if (removeIndex < 0) {
        return false;
    }

    currentPowerlineRoute_.partPoints.removeAt(removeIndex);
    for (RouteWaypoint& waypoint : currentPowerlineRoute_.waypoints) {
        for (int captureIndex = waypoint.captureTargets.size() - 1; captureIndex >= 0; --captureIndex) {
            if (waypoint.captureTargets.at(captureIndex).partIndex == partIndex) {
                waypoint.captureTargets.removeAt(captureIndex);
            }
        }
        if (waypoint.primaryPartIndex == partIndex) {
            waypoint.primaryPartIndex = waypoint.captureTargets.isEmpty() ? -1 : waypoint.captureTargets.first().partIndex;
        }
        if (waypoint.primaryPartIndex > 0 && !waypoint.captureTargets.isEmpty()) {
            const int candidateFileId = waypoint.captureTargets.first().partFileId;
            waypoint.rawKeyId = candidateFileId > 0 ? candidateFileId : waypoint.rawKeyId;
        } else if (waypoint.rawKeyId > 0) {
            waypoint.rawKeyId = -1;
        }
        waypoint.isHelperWaypoint = waypoint.rawKeyId < 0 || waypoint.captureTargets.isEmpty();
    }

    selectedRoutePartIndex_ = -1;
    applyCurrentRouteToViewer();
    updateRoutePlanningPanel();
    rebuildProjectTree();
    updateActionState();
    if (notify) {
        showUserMessage(LogLevel::Info, tr("Deleted route part point: %1").arg(routePartDisplayName(removedPartPoint)), 3000);
    }
    return true;
}

bool MainWindow::removeRouteWaypoint(int waypointIndex, bool notify)
{
    if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
        return false;
    }
    if (!ensureRouteEditingEnabled(notify)) {
        return false;
    }

    currentPowerlineRoute_.waypoints.removeAt(waypointIndex);
    for (int index = 0; index < currentPowerlineRoute_.waypoints.size(); ++index) {
        currentPowerlineRoute_.waypoints[index].sequenceIndex = index;
    }

    selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : std::clamp(waypointIndex, 0, currentPowerlineRoute_.waypoints.size() - 1);
    applyCurrentRouteToViewer();
    updateRoutePlanningPanel();
    rebuildProjectTree();
    updateActionState();
    if (notify) {
        showUserMessage(LogLevel::Info, tr("Deleted route waypoint #%1.").arg(QLocale().toString(waypointIndex + 1)), 3000);
    }
    return true;
}

void MainWindow::applyCurrentRouteToViewer()
{
    if (viewer_ == nullptr) {
        return;
    }
    if (currentPowerlineRoute_.waypoints.isEmpty()) {
        viewer_->clearInspectionRouteWaypoints();
        selectedRoutePartIndex_ = -1;
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        return;
    }

    viewer_->setInspectionRouteDisplayData(buildInspectionRouteDisplayData(currentPowerlineRoute_));
    selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : std::clamp(selectedRouteWaypointIndex_, 0, currentPowerlineRoute_.waypoints.size() - 1);
    viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
    if (selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()) {
        const int targetCount = currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.size();
        selectedRouteWaypointTargetIndex_ = targetCount > 0 ? std::clamp(selectedRouteWaypointTargetIndex_, 0, targetCount - 1) : -1;
    } else {
        selectedRouteWaypointTargetIndex_ = -1;
    }
    viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
}

void MainWindow::updateRoutePlanningPanel()
{
    if (routeStatusValueLabel_ == nullptr || routeSummaryValueLabel_ == nullptr || routeQaSummaryValueLabel_ == nullptr || routePartPointsTableWidget_ == nullptr || routeWaypointsTableWidget_ == nullptr || routeWaypointTargetsTableWidget_ == nullptr || routeQaIssuesTableWidget_ == nullptr) {
        return;
    }

    routeQaReport_ = currentPowerlineRoute_.waypoints.isEmpty() ? RouteQaReport() : evaluatePowerlineRouteQa(currentPowerlineRoute_, routePlanningOptions_.aircraftProfile);
    const QString routeQaStateText = currentPowerlineRoute_.waypoints.isEmpty() ? tr("waiting for route data") : routeQaSummaryText(routeQaReport_);
    const QString routeEditStateText = routeEditingEnabled_ ? tr("Enabled") : tr("Locked");
    const bool routeRoamActive = viewer_ != nullptr && viewer_->inspectionRouteRoamActive();
    const bool routeRoamPaused = viewer_ != nullptr && viewer_->inspectionRouteRoamPaused();
    const QString routeRoamStateText = !routeRoamActive ? tr("Stopped") : (routeRoamPaused ? tr("Paused") : tr("Playing"));
    const QString routeRoamModeText = viewer_ != nullptr && viewer_->inspectionRouteRoamViewMode() == RouteRoamViewMode::FirstPerson ? tr("First Person") : tr("Third Person");
    const QString routeRoamStatusText = tr("Roam: %1 (%2, %3 m/s)").arg(routeRoamStateText, routeRoamModeText, formatCoordinate(static_cast<float>(viewer_ != nullptr ? viewer_->inspectionRouteRoamSpeedMetersPerSecond() : 0.0)));
    const QString routeRoamVisibilityHint = viewer_ != nullptr && !viewer_->inspectionRouteVisible() ? QStringLiteral("\n") + tr("Route visibility is off. Enable route display before starting roam.") : QString();
    routeStatusValueLabel_->setText(currentPowerlineRoute_.waypoints.isEmpty()
        ? tr("No route generated. Analyze vegetation risks first, then generate inspection route.") + QStringLiteral("\n") + tr("Edit Route: %1").arg(routeEditStateText) + QStringLiteral("\n") + tr("Route QA: %1").arg(routeQaStateText) + QStringLiteral("\n") + routeRoamStatusText + routeRoamVisibilityHint
        : tr("%1 waypoint(s), %2 part point(s) ready for scene review and KML/KMZ interoperability.").arg(QLocale().toString(currentPowerlineRoute_.waypoints.size())).arg(QLocale().toString(currentPowerlineRoute_.partPoints.size())) + QStringLiteral("\n") + tr("Edit Route: %1").arg(routeEditStateText) + QStringLiteral("\n") + tr("Route QA: %1").arg(routeQaStateText) + QStringLiteral("\n") + routeRoamStatusText + routeRoamVisibilityHint);
    routeSummaryValueLabel_->setText(tr("%1 -> %2 | DJI profile: %3 | Safety %4 m | Speed %5 m/s | Spacing %6 m | Smoothing %7%% | Height offset %8 m")
        .arg(formatCoordinateSystemCode(projectCoordinateSystems_.pointCloudCrs, tr("Unset")))
        .arg(formatCoordinateSystemCode(projectCoordinateSystems_.geographicCrs, QStringLiteral("EPSG:4326")))
        .arg(djiAircraftProfileDisplayName(routePlanningOptions_.aircraftProfile))
        .arg(formatCoordinate(routePlanningOptions_.safety.safetyHeightMeters))
        .arg(formatCoordinate(routePlanningOptions_.safety.defaultWaypointSpeedMps))
        .arg(formatCoordinate(routePlanningOptions_.generation.waypointSpacingMeters))
        .arg(formatCoordinate(routePlanningOptions_.generation.smoothingStrengthPercent))
        .arg(formatCoordinate(routePlanningOptions_.safety.heightOffsetMeters)));

    const QColor routeQaSummaryColor = routeQaReport_.hasBlockingIssues() ? routeQaSeverityColor(RouteQaSeverity::Blocking) : (routeQaReport_.hasWarnings() ? routeQaSeverityColor(RouteQaSeverity::Warning) : QColor(22, 101, 52));
    routeQaSummaryValueLabel_->setText(currentPowerlineRoute_.waypoints.isEmpty() ? tr("Route QA is waiting for route data.") : tr("Route QA summary: %1").arg(routeQaSummaryText(routeQaReport_)));
    routeQaSummaryValueLabel_->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(routeQaSummaryColor.name()));

    const QSignalBlocker partBlocker(routePartPointsTableWidget_);
    const QSignalBlocker waypointBlocker(routeWaypointsTableWidget_);
    const QSignalBlocker targetBlocker(routeWaypointTargetsTableWidget_);
    const QSignalBlocker qaIssueBlocker(routeQaIssuesTableWidget_);
    updatingRouteTables_ = true;
    routePartPointsTableWidget_->setRowCount(0);
    routeWaypointsTableWidget_->setRowCount(0);
    routeWaypointTargetsTableWidget_->setRowCount(0);
    routeQaIssuesTableWidget_->setRowCount(0);

    auto createReadOnlyItem = [](const QString& text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* item = new QTableWidgetItem(text);
        item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        item->setTextAlignment(alignment);
        return item;
    };

    QHash<int, RoutePartPoint> partPointByIndex;
    for (int partRow = 0; partRow < currentPowerlineRoute_.partPoints.size(); ++partRow) {
        const RoutePartPoint& partPoint = currentPowerlineRoute_.partPoints.at(partRow);
        partPointByIndex.insert(partPoint.partIndex, partPoint);
        routePartPointsTableWidget_->insertRow(partRow);
        QTableWidgetItem* indexItem = createReadOnlyItem(QLocale().toString(partPoint.partIndex), Qt::AlignCenter);
        indexItem->setData(Qt::UserRole, partPoint.partIndex);
        routePartPointsTableWidget_->setItem(partRow, 0, indexItem);
        routePartPointsTableWidget_->setItem(partRow, kRoutePartColumnPartName, createReadOnlyItem(routePartDisplayName(partPoint)));
        routePartPointsTableWidget_->setItem(partRow, kRoutePartColumnHardware, createReadOnlyItem(partPoint.hardwareType));
        routePartPointsTableWidget_->setItem(partRow, kRoutePartColumnPhase, createReadOnlyItem(partPoint.phaseSequence, Qt::AlignCenter));
        routePartPointsTableWidget_->setItem(partRow, kRoutePartColumnCameraAngle, createReadOnlyItem(QLocale().toString(partPoint.cameraAngle), Qt::AlignCenter));
        routePartPointsTableWidget_->setItem(partRow, kRoutePartColumnX, createReadOnlyItem(formatCoordinate(partPoint.localPoint.x), Qt::AlignRight | Qt::AlignVCenter));
        routePartPointsTableWidget_->setItem(partRow, kRoutePartColumnY, createReadOnlyItem(formatCoordinate(partPoint.localPoint.y), Qt::AlignRight | Qt::AlignVCenter));
        routePartPointsTableWidget_->setItem(partRow, kRoutePartColumnZ, createReadOnlyItem(formatCoordinate(partPoint.localPoint.z), Qt::AlignRight | Qt::AlignVCenter));
    }

    for (int waypointIndex = 0; waypointIndex < currentPowerlineRoute_.waypoints.size(); ++waypointIndex) {
        const RouteWaypoint& waypoint = currentPowerlineRoute_.waypoints.at(waypointIndex);
        const RouteCaptureTarget firstTarget = waypoint.captureTargets.isEmpty() ? RouteCaptureTarget() : waypoint.captureTargets.first();
        routeWaypointsTableWidget_->insertRow(waypointIndex);
        routeWaypointsTableWidget_->setItem(waypointIndex, 0, createReadOnlyItem(QLocale().toString(waypointIndex + 1), Qt::AlignCenter));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnPart, createReadOnlyItem(routeWaypointPartSummary(waypoint, partPointByIndex)));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnX, createReadOnlyItem(formatCoordinate(waypoint.localPoint.x), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnY, createReadOnlyItem(formatCoordinate(waypoint.localPoint.y), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnZ, createReadOnlyItem(formatCoordinate(waypoint.localPoint.z), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnAircraftYaw, createReadOnlyItem(formatCoordinate(waypoint.aircraftYawDeg), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnGimbalPitch, createReadOnlyItem(formatCoordinate(waypoint.gimbalPitchDeg), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnCameraYaw, createReadOnlyItem(formatCoordinate(firstTarget.cameraYawDeg), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(waypointIndex, kRouteWaypointColumnCameraPitch, createReadOnlyItem(formatCoordinate(firstTarget.cameraPitchDeg), Qt::AlignRight | Qt::AlignVCenter));
    }

    const int linkedPartIndex = selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size() ? routeWaypointRepresentativePartIndex(currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_)) : -1;
    const int targetPartIndex = linkedPartIndex > 0 ? linkedPartIndex : selectedRoutePartIndex_;
    int selectedPartRow = -1;
    if (targetPartIndex > 0) {
        for (int row = 0; row < routePartPointsTableWidget_->rowCount(); ++row) {
            QTableWidgetItem* item = routePartPointsTableWidget_->item(row, 0);
            if (item != nullptr && item->data(Qt::UserRole).toInt() == targetPartIndex) {
                selectedPartRow = row;
                break;
            }
        }
    }
    if (selectedPartRow >= 0) {
        selectedRoutePartIndex_ = targetPartIndex;
        routePartPointsTableWidget_->setCurrentCell(selectedPartRow, kRoutePartColumnPartName);
    } else {
        routePartPointsTableWidget_->clearSelection();
        selectedRoutePartIndex_ = -1;
    }

    if (selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < routeWaypointsTableWidget_->rowCount()) {
        routeWaypointsTableWidget_->setCurrentCell(selectedRouteWaypointIndex_, kRouteWaypointColumnPart);
    } else {
        routeWaypointsTableWidget_->clearSelection();
        selectedRouteWaypointTargetIndex_ = -1;
    }

    if (selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()) {
        const RouteWaypoint& selectedWaypoint = currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_);
        selectedRouteWaypointTargetIndex_ = selectedWaypoint.captureTargets.isEmpty() ? -1 : std::clamp(selectedRouteWaypointTargetIndex_, 0, selectedWaypoint.captureTargets.size() - 1);
        for (int targetIndex = 0; targetIndex < selectedWaypoint.captureTargets.size(); ++targetIndex) {
            const RouteCaptureTarget& captureTarget = selectedWaypoint.captureTargets.at(targetIndex);
            routeWaypointTargetsTableWidget_->insertRow(targetIndex);
            PointRecord resolvedTargetPoint = captureTarget.targetLocalPoint;
            if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
                resolvedTargetPoint = partPointByIndex.value(captureTarget.partIndex).localPoint;
            }
            const QString targetPointText = tr("%1 / %2 / %3").arg(formatCoordinate(resolvedTargetPoint.x)).arg(formatCoordinate(resolvedTargetPoint.y)).arg(formatCoordinate(resolvedTargetPoint.z));
            const QString partDisplayName = routeCaptureTargetDisplayName(captureTarget, partPointByIndex, targetIndex + 1);
            QTableWidgetItem* indexItem = createReadOnlyItem(QLocale().toString(targetIndex + 1), Qt::AlignCenter);
            indexItem->setData(Qt::UserRole, targetIndex);
            routeWaypointTargetsTableWidget_->setItem(targetIndex, 0, indexItem);
            routeWaypointTargetsTableWidget_->setItem(targetIndex, kRouteWaypointTargetColumnPart, createReadOnlyItem(partDisplayName));
            routeWaypointTargetsTableWidget_->setItem(targetIndex, kRouteWaypointTargetColumnFocalRatio, createReadOnlyItem(formatCoordinate(captureTarget.focalLengthRatio), Qt::AlignRight | Qt::AlignVCenter));
            routeWaypointTargetsTableWidget_->setItem(targetIndex, kRouteWaypointTargetColumnCameraYaw, createReadOnlyItem(formatCoordinate(captureTarget.cameraYawDeg), Qt::AlignRight | Qt::AlignVCenter));
            routeWaypointTargetsTableWidget_->setItem(targetIndex, kRouteWaypointTargetColumnCameraPitch, createReadOnlyItem(formatCoordinate(captureTarget.cameraPitchDeg), Qt::AlignRight | Qt::AlignVCenter));
            routeWaypointTargetsTableWidget_->setItem(targetIndex, kRouteWaypointTargetColumnTargetPoint, createReadOnlyItem(targetPointText, Qt::AlignRight | Qt::AlignVCenter));
        }
        if (selectedRouteWaypointTargetIndex_ >= 0 && selectedRouteWaypointTargetIndex_ < routeWaypointTargetsTableWidget_->rowCount()) {
            routeWaypointTargetsTableWidget_->setCurrentCell(selectedRouteWaypointTargetIndex_, kRouteWaypointTargetColumnPart);
        } else {
            routeWaypointTargetsTableWidget_->clearSelection();
        }
    } else {
        selectedRouteWaypointTargetIndex_ = -1;
        routeWaypointTargetsTableWidget_->clearSelection();
    }

    for (int qaIssueIndex = 0; qaIssueIndex < routeQaReport_.issues.size(); ++qaIssueIndex) {
        const RouteQaIssue& qaIssue = routeQaReport_.issues.at(qaIssueIndex);
        routeQaIssuesTableWidget_->insertRow(qaIssueIndex);
        const QString partNameText = qaIssue.partIndex > 0 ? (partPointByIndex.contains(qaIssue.partIndex) ? routePartDisplayName(partPointByIndex.value(qaIssue.partIndex)) : tr("Part %1").arg(QLocale().toString(qaIssue.partIndex))) : tr("-");
        QString descriptionText = qaIssue.message;
        if (!qaIssue.detail.trimmed().isEmpty()) {
            descriptionText = tr("%1 (%2)").arg(qaIssue.message, qaIssue.detail);
        }
        QTableWidgetItem* severityItem = createReadOnlyItem(routeQaSeverityDisplayName(qaIssue.severity), Qt::AlignCenter);
        severityItem->setForeground(routeQaSeverityColor(qaIssue.severity));
        routeQaIssuesTableWidget_->setItem(qaIssueIndex, kRouteQaColumnSeverity, severityItem);
        routeQaIssuesTableWidget_->setItem(qaIssueIndex, kRouteQaColumnType, createReadOnlyItem(routeQaIssueTypeDisplayName(qaIssue.type), Qt::AlignCenter));
        routeQaIssuesTableWidget_->setItem(qaIssueIndex, kRouteQaColumnLocation, createReadOnlyItem(routeQaIssueLocationText(qaIssue), Qt::AlignCenter));
        routeQaIssuesTableWidget_->setItem(qaIssueIndex, kRouteQaColumnPart, createReadOnlyItem(partNameText));
        routeQaIssuesTableWidget_->setItem(qaIssueIndex, kRouteQaColumnDescription, createReadOnlyItem(descriptionText));
    }

    if (selectedRouteQaIssueIndex_ >= 0 && selectedRouteQaIssueIndex_ < routeQaIssuesTableWidget_->rowCount()) {
        routeQaIssuesTableWidget_->setCurrentCell(selectedRouteQaIssueIndex_, kRouteQaColumnSeverity);
    } else {
        selectedRouteQaIssueIndex_ = -1;
        routeQaIssuesTableWidget_->clearSelection();
    }

    if (viewer_ != nullptr) {
        viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
    }
    applyRouteWaypointTableColumnVisibility();
    applyRoutePartTableColumnVisibility();
    updatingRouteTables_ = false;
}

void MainWindow::applyRouteWaypointTableColumnVisibility()
{
    if (routeWaypointsTableWidget_ == nullptr) {
        return;
    }
    const bool showCoordinates = routeWaypointShowCoordinatesCheckBox_ == nullptr || routeWaypointShowCoordinatesCheckBox_->isChecked();
    const bool showCaptureAngles = routeWaypointShowCaptureAnglesCheckBox_ == nullptr || routeWaypointShowCaptureAnglesCheckBox_->isChecked();
    routeWaypointsTableWidget_->setColumnHidden(kRouteWaypointColumnX, !showCoordinates);
    routeWaypointsTableWidget_->setColumnHidden(kRouteWaypointColumnY, !showCoordinates);
    routeWaypointsTableWidget_->setColumnHidden(kRouteWaypointColumnZ, !showCoordinates);
    routeWaypointsTableWidget_->setColumnHidden(kRouteWaypointColumnAircraftYaw, !showCaptureAngles);
    routeWaypointsTableWidget_->setColumnHidden(kRouteWaypointColumnGimbalPitch, !showCaptureAngles);
    routeWaypointsTableWidget_->setColumnHidden(kRouteWaypointColumnCameraYaw, !showCaptureAngles);
    routeWaypointsTableWidget_->setColumnHidden(kRouteWaypointColumnCameraPitch, !showCaptureAngles);
}

void MainWindow::applyRoutePartTableColumnVisibility()
{
    if (routePartPointsTableWidget_ == nullptr) {
        return;
    }
    const bool showCoordinates = routePartShowCoordinatesCheckBox_ == nullptr || routePartShowCoordinatesCheckBox_->isChecked();
    const bool showCaptureAngles = routePartShowCaptureAnglesCheckBox_ == nullptr || routePartShowCaptureAnglesCheckBox_->isChecked();
    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnCameraAngle, !showCaptureAngles);
    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnX, !showCoordinates);
    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnY, !showCoordinates);
    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnZ, !showCoordinates);
}
