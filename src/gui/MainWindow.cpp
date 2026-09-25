#include "gui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QEventLoop>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLinearGradient>
#include <QLibraryInfo>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QProgressBar>
#include <QProcess>
#include <QPushButton>
#include <QDropEvent>
#include <QFile>
#include <QPalette>
#include <QPlainTextEdit>
#include <QGuiApplication>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyle>
#include <QSlider>
#include <QTabBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QToolButton>
#include <QToolBar>
#include <QToolTip>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <QMouseEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <set>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "QtnRibbonBar.h"
#include "QtnRibbonBackstageView.h"
#include "QtnRibbonGroup.h"
#include "QtnRibbonPage.h"
#include "QtnRibbonQuickAccessBar.h"
#include "QtnRibbonSystemPopupBar.h"
#include "QtnRibbonToolTip.h"

#include "crs/CrsAuthorityService.h"
#include "crs/CrsTransformService.h"
#include "crs/ProjectCoordinateSystemsDialog.h"
#include "crs/CrsTypes.h"
#include "domain/ClearanceAnalysis.h"
#include "domain/ClearanceReportExporter.h"
#include "domain/ClassificationEditStore.h"
#include "domain/DataManager.h"
#include "domain/InspectionData.h"
#include "domain/InspectionReportExporter.h"
#include "domain/ProfileMarkerProjection.h"
#include "domain/RuleBasedClearanceEngine.h"
#include "domain/TowerFileInterop.h"
#include "domain/VegetationRiskAnalysis.h"
#include "gui/ApplicationLogDock.h"
#include "gui/BackstageAboutWidget.h"
#include "gui/BackstageApplicationSettingsWidget.h"
#include "gui/BackstageOpenActionsWidget.h"
#include "gui/BackstageOpenProjectWidget.h"
#include "gui/BackstagePageHeaderWidget.h"
#include "gui/BackstageProjectPropertiesWidget.h"
#include "gui/DatasetSummaryWidget.h"
#include "gui/IssueController.h"
#include "gui/IssueEditorWidget.h"
#include "gui/MainWindowInternal.h"
#include "gui/MeasurementAnalysisController.h"
#include "gui/MeasurementPanelWidget.h"
#include "gui/NavigationSettingsWidget.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationController.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProfileClassificationWidget.h"
#include "gui/ProfilePlotWidget.h"
#include "gui/ProjectExplorerController.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteController.h"
#include "gui/RouteDetailsDock.h"
#include "gui/SceneInspectorDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/TowerController.h"
#include "gui/TowerEditorWidget.h"
#include "gui/UiHistoryStore.h"
#include "gui/VisualizationPanelController.h"
#include "gui/WebPageDock.h"
#include "gui/support/RibbonIconFactory.h"
#include "gui/support/SettingsKeys.h"
#include "gui/support/UiHelpers.h"
#include "logging/ApplicationLogger.h"
#include "osg/PointCloudVisualization.h"
#include "pointcloud/LasReader.h"
#include "pointcloud/PointCloudData.h"
#include "route/InspectionRoutePlanning.h"
#include "route/PowerlineRouteBridge.h"
#include "route/PowerlineRouteJson.h"
#include "route/RouteInterop.h"

#ifdef LAS_VIEWER_HAS_LASLIB
#include "lasreader.hpp"
#include "laswriter.hpp"
#endif

using lasviewer::crs::CoordinateSystemKind;
using lasviewer::crs::CoordinateSystemRef;
using lasviewer::crs::CrsAuthorityService;
using lasviewer::crs::CrsTransformService;
using lasviewer::crs::ProjectCoordinateSystemsDialog;
using lasviewer::gui::RibbonGlyph;
using lasviewer::gui::WindowControlGlyph;
using lasviewer::gui::applyStyledDialogPalette;
using lasviewer::gui::createResourceIconOrFallback;
using lasviewer::gui::createRibbonIcon;
using lasviewer::gui::createWindowControlIcon;
using lasviewer::gui::enforceLightDialogButtonStyles;
using lasviewer::gui::setFormFieldLabel;
using lasviewer::gui::showLightStyledMessageBox;
using lasviewer::gui::showStyledOpenFileNameDialog;
using lasviewer::gui::showStyledOpenFileNamesDialog;
using lasviewer::gui::showStyledSaveFileNameDialog;
namespace settingskeys = lasviewer::gui::settingskeys;
using namespace mainwindow_internal;

namespace
{
const QColor kWindowChromeLight(243, 246, 251);
const QColor kWindowChromeDark(51, 65, 85);
constexpr int kMainWindowStateVersion = 1;
bool boundsFromPoints(const QList<PointRecord>& points, PointRecord* minBounds, PointRecord* maxBounds)
{
    if (points.isEmpty() || minBounds == nullptr || maxBounds == nullptr) {
        return false;
    }

    *minBounds = points.constFirst();
    *maxBounds = points.constFirst();
    for (int index = 1; index < points.size(); ++index) {
        const PointRecord& point = points.at(index);
        minBounds->x = std::min(minBounds->x, point.x);
        minBounds->y = std::min(minBounds->y, point.y);
        minBounds->z = std::min(minBounds->z, point.z);
        maxBounds->x = std::max(maxBounds->x, point.x);
        maxBounds->y = std::max(maxBounds->y, point.y);
        maxBounds->z = std::max(maxBounds->z, point.z);
    }
    return true;
}

QString colorModeName(PointCloudColorMode colorMode)
{
    switch (colorMode) {
    case PointCloudColorMode::Elevation:
        return QCoreApplication::translate("MainWindow", "Elevation ramp");
    case PointCloudColorMode::SingleColor:
        return QCoreApplication::translate("MainWindow", "Single color");
    case PointCloudColorMode::Classification:
        return QCoreApplication::translate("MainWindow", "Classification");
    case PointCloudColorMode::Rgb:
    default:
        return QCoreApplication::translate("MainWindow", "RGB");
    }
}

QString measurementPointText(const MeasurementResult& measurementResult, bool useStartPoint)
{
    const bool hasPoint = useStartPoint ? measurementResult.hasStartPoint : measurementResult.hasEndPoint;
    if (!hasPoint) {
        return QCoreApplication::translate("MainWindow", "Not set");
    }

    const PointRecord& point = useStartPoint ? measurementResult.startPoint : measurementResult.endPoint;
    return formatTriplet(point.x, point.y, point.z);
}

enum class OpenFileKind
{
    Unknown,
    PointCloud,
    Project,
    Route
};

OpenFileKind detectOpenFileKind(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return OpenFileKind::Unknown;
    }

    const QString suffix = fileInfo.suffix().toLower();
    if (suffix == QStringLiteral("las") || suffix == QStringLiteral("laz") || suffix == QStringLiteral("ply")) {
        return OpenFileKind::PointCloud;
    }
    if (suffix == QStringLiteral("lpproj")) {
        return OpenFileKind::Project;
    }
    if (suffix != QStringLiteral("json")) {
        return OpenFileKind::Unknown;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return OpenFileKind::Unknown;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        return OpenFileKind::Unknown;
    }

    const QJsonObject rootObject = document.object();
    if (rootObject.contains(QStringLiteral("pointCloudFilePaths"))
        || rootObject.contains(QStringLiteral("pointCloudFilePath"))
        || rootObject.contains(QStringLiteral("visualization"))) {
        return OpenFileKind::Project;
    }
    if (rootObject.contains(QStringLiteral("powerline"))
        && rootObject.contains(QStringLiteral("points"))) {
        return OpenFileKind::Route;
    }

    return OpenFileKind::Unknown;
}

QString datasetPathSummary(const QStringList& filePaths)
{
    if (filePaths.isEmpty()) {
        return QString();
    }

    QStringList lines;
    const int visibleCount = std::min(4, filePaths.size());
    for (int index = 0; index < visibleCount; ++index) {
        lines.append(filePaths.at(index));
    }
    if (filePaths.size() > visibleCount) {
        lines.append(
            QCoreApplication::translate("MainWindow", "... and %1 more")
                .arg(QLocale().toString(filePaths.size() - visibleCount)));
    }
    return lines.join(QLatin1Char('\n'));
}

QString routePartDisplayName(const RoutePartPoint& partPoint)
{
    return partPoint.partName.trimmed().isEmpty()
        ? QCoreApplication::translate("MainWindow", "Part %1").arg(QLocale().toString(partPoint.partIndex))
        : partPoint.partName.trimmed();
}

QString routeWaypointPartSummary(
    const RouteWaypoint& waypoint,
    const QHash<int, RoutePartPoint>& partPointByIndex)
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

const char kRouteHistoryIdDisplayWaypointLabelMode[] = "route.display.waypointLabelMode";
const char kRouteHistoryIdDisplayPartLabelMode[] = "route.display.partLabelMode";
const char kRouteHistoryIdDisplayWaypointShowCoordinates[] = "route.display.waypointShowCoordinates";
const char kRouteHistoryIdDisplayWaypointShowCaptureAngles[] = "route.display.waypointShowCaptureAngles";
const char kRouteHistoryIdDisplayPartShowCoordinates[] = "route.display.partShowCoordinates";
const char kRouteHistoryIdDisplayPartShowCaptureAngles[] = "route.display.partShowCaptureAngles";
const char kRouteHistoryIdEditingEnabled[] = "route.editingEnabled";
const char kRouteHistoryIdPlanningAircraftProfile[] = "route.planning.aircraftProfile";
const char kRouteHistoryIdPlanningSafetyHeightMeters[] = "route.planning.safetyHeightMeters";
const char kRouteHistoryIdPlanningWaypointSpeedMps[] = "route.planning.waypointSpeedMps";
const char kRouteHistoryIdPlanningWaypointSpacingMeters[] = "route.planning.waypointSpacingMeters";
const char kRouteHistoryIdPlanningSmoothingStrengthPercent[] = "route.planning.smoothingStrengthPercent";
const char kRouteHistoryIdPlanningHeightOffsetMeters[] = "route.planning.heightOffsetMeters";
const char kRouteHistoryIdRoamSpeedMps[] = "route.roam.speedMps";
const char kRouteHistoryIdRoamViewMode[] = "route.roam.viewMode";

QString routeCaptureTargetDisplayName(
    const RouteCaptureTarget& captureTarget,
    const QHash<int, RoutePartPoint>& partPointByIndex,
    int targetSequence)
{
    if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
        return routePartDisplayName(partPointByIndex.value(captureTarget.partIndex));
    }

    if (!captureTarget.partName.trimmed().isEmpty()) {
        return captureTarget.partName.trimmed();
    }

    return QCoreApplication::translate("MainWindow", "Target %1")
        .arg(QLocale().toString(targetSequence));
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
        return QCoreApplication::translate("MainWindow", "WP %1")
            .arg(QLocale().toString(issue.waypointIndex + 1));
    }
    if (issue.partIndex > 0) {
        return QCoreApplication::translate("MainWindow", "Part %1")
            .arg(QLocale().toString(issue.partIndex));
    }
    return QCoreApplication::translate("MainWindow", "Global");
}

QString routeQaSummaryText(const RouteQaReport& report)
{
    if (report.issues.isEmpty()) {
        return QCoreApplication::translate("MainWindow", "Route QA passed with no issues.");
    }

    return QCoreApplication::translate("MainWindow", "Blocking: %1 | Warning: %2 | Info: %3")
        .arg(
            QLocale().toString(report.blockingIssueCount),
            QLocale().toString(report.warningIssueCount),
            QLocale().toString(report.infoIssueCount));
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
        const RouteCaptureTarget primaryTarget = waypoint.captureTargets.isEmpty()
            ? RouteCaptureTarget()
            : waypoint.captureTargets.first();

        QList<PointRecord> allTargetPoints;
        QList<int> allTargetPartIndices;
        QList<double> allCameraYawDegs;
        QList<double> allCameraPitchDegs;
        QList<double> allFocalLengthRatios;
        QStringList allTargetLabels;
        allTargetPoints.reserve(waypoint.captureTargets.size());
        allTargetPartIndices.reserve(waypoint.captureTargets.size());
        allCameraYawDegs.reserve(waypoint.captureTargets.size());
        allCameraPitchDegs.reserve(waypoint.captureTargets.size());
        allFocalLengthRatios.reserve(waypoint.captureTargets.size());
        allTargetLabels.reserve(waypoint.captureTargets.size());

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
        displayData.waypointFocalLengthRatios.append(
            allFocalLengthRatios.isEmpty()
                ? normalizedRouteFocalLengthRatio(primaryTarget.focalLengthRatio)
                : allFocalLengthRatios.constFirst());
        displayData.waypointTargetLabels.append(routeWaypointPartSummary(waypoint, routePartPointByIndex));
    }

    return displayData;
}

QColor analysisSeverityColor(AnalysisSeverity severity)
{
    switch (severity) {
    case AnalysisSeverity::Advisory:
        return QColor(217, 119, 6);
    case AnalysisSeverity::Warning:
        return QColor(220, 38, 38);
    case AnalysisSeverity::Critical:
        return QColor(127, 29, 29);
    case AnalysisSeverity::None:
    default:
        return QColor(22, 101, 52);
    }
}

IssueSeverity issueSeverityFromAnalysisSeverity(AnalysisSeverity severity)
{
    switch (severity) {
    case AnalysisSeverity::Advisory:
        return IssueSeverity::Minor;
    case AnalysisSeverity::Warning:
        return IssueSeverity::Major;
    case AnalysisSeverity::Critical:
        return IssueSeverity::Critical;
    case AnalysisSeverity::None:
    default:
        return IssueSeverity::Info;
    }
}

QJsonObject colorToJson(const QColor& color)
{
    return QJsonObject {
        { QStringLiteral("r"), color.red() },
        { QStringLiteral("g"), color.green() },
        { QStringLiteral("b"), color.blue() },
        { QStringLiteral("a"), color.alpha() }
    };
}

QColor colorFromJson(const QJsonObject& object, const QColor& fallback)
{
    if (object.isEmpty()) {
        return fallback;
    }

    return QColor(
        object.value(QStringLiteral("r")).toInt(fallback.red()),
        object.value(QStringLiteral("g")).toInt(fallback.green()),
        object.value(QStringLiteral("b")).toInt(fallback.blue()),
        object.value(QStringLiteral("a")).toInt(fallback.alpha()));
}

QLabel* createDetailsValueLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("detailsValueLabel"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

QFrame* createDetailsStatCard(const QString& labelText, const QString& valueText, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("detailsStatCard"));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(4);

    auto* valueLabel = new QLabel(valueText, card);
    valueLabel->setObjectName(QStringLiteral("detailsStatValue"));
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(valueLabel);

    auto* label = new QLabel(labelText, card);
    label->setObjectName(QStringLiteral("detailsStatLabel"));
    label->setWordWrap(true);
    layout->addWidget(label);

    return card;
}

void showStyledDetailsDialog(
    QWidget* parent,
    const QString& title,
    const QString& subtitle,
    const QList<QPair<QString, QString>>& detailRows,
    const QList<QPair<QString, QString>>& statRows = {})
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.resize(760, 560);
    dialog.setStyleSheet(QStringLiteral(
        "QDialog {"
        "background-color: #f3f7fb;"
        "}"
        "QFrame#detailsHeaderCard, QFrame#detailsBodyCard, QFrame#detailsStatCard {"
        "background-color: rgba(255, 255, 255, 0.96);"
        "border: 1px solid rgba(148, 163, 184, 0.20);"
        "border-radius: 16px;"
        "}"
        "QLabel#detailsTitleLabel {"
        "color: #0f172a;"
        "font-size: 22px;"
        "font-weight: 700;"
        "}"
        "QLabel#detailsSubtitleLabel {"
        "color: #475569;"
        "font-size: 13px;"
        "line-height: 1.35em;"
        "}"
        "QLabel#detailsSectionLabel {"
        "color: #0f172a;"
        "font-size: 14px;"
        "font-weight: 600;"
        "}"
        "QLabel#detailsKeyLabel {"
        "color: #64748b;"
        "font-size: 12px;"
        "font-weight: 600;"
        "letter-spacing: 0.04em;"
        "text-transform: uppercase;"
        "padding-top: 2px;"
        "}"
        "QLabel#detailsValueLabel {"
        "color: #0f172a;"
        "font-size: 13px;"
        "font-weight: 500;"
        "padding-bottom: 10px;"
        "}"
        "QLabel#detailsStatValue {"
        "color: #0f172a;"
        "font-size: 18px;"
        "font-weight: 700;"
        "}"
        "QLabel#detailsStatLabel {"
        "color: #64748b;"
        "font-size: 12px;"
        "font-weight: 500;"
        "}"
        "QPushButton {"
        "min-width: 96px;"
        "padding: 8px 18px;"
        "border-radius: 10px;"
        "border: 1px solid rgba(37, 99, 235, 0.18);"
        "background-color: #e0ecff;"
        "color: #1d4ed8;"
        "font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "background-color: #d4e4ff;"
        "}"));

    auto* rootLayout = new QVBoxLayout(&dialog);
    rootLayout->setContentsMargins(24, 22, 24, 20);
    rootLayout->setSpacing(16);

    auto* headerCard = new QFrame(&dialog);
    headerCard->setObjectName(QStringLiteral("detailsHeaderCard"));
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(22, 20, 22, 20);
    headerLayout->setSpacing(8);

    auto* titleLabel = new QLabel(title, headerCard);
    titleLabel->setObjectName(QStringLiteral("detailsTitleLabel"));
    titleLabel->setWordWrap(true);
    headerLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(subtitle, headerCard);
    subtitleLabel->setObjectName(QStringLiteral("detailsSubtitleLabel"));
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    headerLayout->addWidget(subtitleLabel);

    if (!statRows.isEmpty()) {
        auto* statsGrid = new QGridLayout();
        statsGrid->setContentsMargins(0, 8, 0, 0);
        statsGrid->setHorizontalSpacing(12);
        statsGrid->setVerticalSpacing(12);

        for (int statIndex = 0; statIndex < statRows.size(); ++statIndex) {
            const auto& stat = statRows.at(statIndex);
            statsGrid->addWidget(
                createDetailsStatCard(stat.first, stat.second, headerCard),
                statIndex / 2,
                statIndex % 2);
        }
        headerLayout->addLayout(statsGrid);
    }

    rootLayout->addWidget(headerCard);

    auto* bodyCard = new QFrame(&dialog);
    bodyCard->setObjectName(QStringLiteral("detailsBodyCard"));
    auto* bodyLayout = new QVBoxLayout(bodyCard);
    bodyLayout->setContentsMargins(22, 18, 22, 16);
    bodyLayout->setSpacing(12);

    auto* sectionLabel = new QLabel(QCoreApplication::translate("MainWindow", "Detailed Information"), bodyCard);
    sectionLabel->setObjectName(QStringLiteral("detailsSectionLabel"));
    bodyLayout->addWidget(sectionLabel);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(1, 1);

    for (int rowIndex = 0; rowIndex < detailRows.size(); ++rowIndex) {
        const auto& row = detailRows.at(rowIndex);
        auto* keyLabel = new QLabel(row.first, bodyCard);
        keyLabel->setObjectName(QStringLiteral("detailsKeyLabel"));
        keyLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        grid->addWidget(keyLabel, rowIndex, 0);
        grid->addWidget(createDetailsValueLabel(row.second, bodyCard), rowIndex, 1);
    }

    bodyLayout->addLayout(grid);
    rootLayout->addWidget(bodyCard, 1);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    rootLayout->addWidget(buttonBox);

    enforceLightDialogButtonStyles(&dialog);
    dialog.exec();
}

QColor showStyledColorDialog(
    QWidget* parent,
    const QColor& initialColor,
    const QString& title)
{
    QColorDialog dialog(initialColor, parent);
    dialog.setWindowTitle(title);
    dialog.setCurrentColor(initialColor);
    dialog.setOption(QColorDialog::DontUseNativeDialog, true);
    dialog.setStyleSheet(QStringLiteral(
        "QColorDialog, QColorDialog QWidget {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "}"
        "QColorDialog QGroupBox {"
        "background-color: #ffffff;"
        "border: 1px solid #d6dde8;"
        "border-radius: 8px;"
        "margin-top: 8px;"
        "padding-top: 10px;"
        "font-weight: 600;"
        "}"
        "QColorDialog QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 8px;"
        "padding: 0 4px;"
        "color: #334155;"
        "background-color: #f8fafc;"
        "}"
        "QColorDialog QLineEdit,"
        "QColorDialog QSpinBox,"
        "QColorDialog QDoubleSpinBox,"
        "QColorDialog QComboBox,"
        "QColorDialog QPushButton,"
        "QColorDialog QToolButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "min-height: 24px;"
        "padding: 3px 8px;"
        "}"
        "QColorDialog QPushButton:hover,"
        "QColorDialog QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QColorDialog QPushButton:pressed,"
        "QColorDialog QToolButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QColorDialog QToolButton:disabled,"
        "QColorDialog QPushButton:disabled {"
        "background-color: #f1f5f9;"
        "color: #94a3b8;"
        "border-color: #dbe3ee;"
        "}"
        "QColorDialog QDialogButtonBox QPushButton {"
        "min-width: 80px;"
        "}"
        "QColorDialog QDialogButtonBox QPushButton:default {"
        "background-color: #e0ecff;"
        "color: #1d4ed8;"
        "border-color: #93c5fd;"
        "}"
        "QColorDialog QDialogButtonBox QPushButton:default:hover {"
        "background-color: #d4e4ff;"
        "}"
        "QColorDialog QAbstractItemView {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "}"
        "QColorDialog QLabel,"
        "QColorDialog QCheckBox,"
        "QColorDialog QRadioButton {"
        "color: #0f172a;"
        "}"
    ));

    QPalette palette = dialog.palette();
    palette.setColor(QPalette::Window, QColor(248, 250, 252));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(241, 245, 249));
    palette.setColor(QPalette::WindowText, QColor(15, 23, 42));
    palette.setColor(QPalette::Text, QColor(15, 23, 42));
    palette.setColor(QPalette::Button, QColor(255, 255, 255));
    palette.setColor(QPalette::ButtonText, QColor(15, 23, 42));
    palette.setColor(QPalette::Highlight, QColor(219, 234, 254));
    palette.setColor(QPalette::HighlightedText, QColor(15, 23, 42));
    dialog.setPalette(palette);
    enforceLightDialogButtonStyles(&dialog);

    if (dialog.exec() == QDialog::Accepted) {
        return dialog.currentColor();
    }
    return QColor();
}

QString backstagePageStyleSheet()
{
    return QStringLiteral(
        "QWidget {"
        "background-color: #f8fbff;"
        "color: #0f172a;"
        "}"
        "QFrame#backstageCard {"
        "background-color: #ffffff;"
        "border: 1px solid #d8e3f0;"
        "border-radius: 14px;"
        "}"
        "QLabel#backstageTitleLabel {"
        "font-size: 28px;"
        "font-weight: 700;"
        "color: #0f172a;"
        "}"
        "QLabel#backstageSubtitleLabel {"
        "font-size: 13px;"
        "color: #475569;"
        "}"
        "QLabel#backstageSectionLabel {"
        "font-size: 15px;"
        "font-weight: 700;"
        "color: #0f172a;"
        "}"
        "QLabel#backstageBodyLabel {"
        "font-size: 13px;"
        "line-height: 1.4;"
        "color: #334155;"
        "}"
        "QLineEdit, QListWidget {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 10px;"
        "padding: 6px 10px;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "}"
        "QListWidget::item {"
        "padding: 8px 10px;"
        "border-radius: 8px;"
        "}"
        "QListWidget::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QPushButton, QToolButton {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 10px;"
        "padding: 8px 14px;"
        "font-weight: 600;"
        "}"
        "QPushButton:hover, QToolButton:hover {"
        "background-color: #eef4ff;"
        "border-color: #93c5fd;"
        "}"
        "QPushButton:pressed, QToolButton:pressed {"
        "background-color: #dbeafe;"
        "}"
        "QPushButton:disabled, QToolButton:disabled {"
        "background-color: #f1f5f9;"
        "color: #94a3b8;"
        "border-color: #e2e8f0;"
        "}"
        "QToolButton:checked {"
        "background-color: #dbeafe;"
        "border-color: #60a5fa;"
        "}"
        "QGroupBox {"
        "font-weight: 700;"
        "color: #0f172a;"
        "border: 1px solid #d8e3f0;"
        "border-radius: 12px;"
        "margin-top: 16px;"
        "padding: 16px 14px 14px 14px;"
        "background-color: #ffffff;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: 12px;"
        "padding: 0 6px;"
        "}"
        "QCheckBox {"
        "color: #0f172a;"
        "spacing: 8px;"
        "}"
        "QCheckBox::indicator {"
        "width: 16px;"
        "height: 16px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "background-color: #ffffff;"
        "border: 1px solid #94a3b8;"
        "border-radius: 4px;"
        "}"
        "QCheckBox::indicator:checked {"
        "background-color: #2563eb;"
        "border: 1px solid #2563eb;"
        "border-radius: 4px;"
        "}");
}

QFrame* createBackstageCard(QWidget* parent = nullptr)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("backstageCard"));
    return card;
}

QToolButton* createBackstageActionButton(QAction* action, QWidget* parent = nullptr)
{
    auto* button = new QToolButton(parent);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(false);
    button->setDefaultAction(action);
    button->setIconSize(QSize(20, 20));
    return button;
}

}


QWidget* MainWindow::createSliderControl(QSlider*& slider, QLabel*& valueLabel, int minimum, int maximum, int step)
{
    auto* container = new QWidget(renderingGroupBox_);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    slider = new QSlider(Qt::Horizontal, container);
    slider->setRange(minimum, maximum);
    slider->setSingleStep(step);
    slider->setPageStep(std::max(step, (maximum - minimum) / 10));
    slider->setTracking(true);
    slider->setTickInterval(step);

    valueLabel = new QLabel(container);
    valueLabel->setMinimumWidth(56);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    valueLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "color: #475569;"
        "font-weight: 600;"
        "}"));

    layout->addWidget(slider, 1);
    layout->addWidget(valueLabel, 0);
    return container;
}

void MainWindow::updateSliderValueLabel(QSlider* slider, QLabel* valueLabel, const QString& formatText) const
{
    if (slider == nullptr || valueLabel == nullptr) {
        return;
    }

    valueLabel->setText(formatText.arg(QLocale().toString(slider->value())));
}

void MainWindow::updateVisualizationTooltips()
{
    const auto applyTooltip = [](QWidget* primary, QWidget* secondary, QWidget* tertiary, const QString& tooltip) {
        if (primary != nullptr) {
            primary->setToolTip(tooltip);
        }
        if (secondary != nullptr) {
            secondary->setToolTip(tooltip);
        }
        if (tertiary != nullptr) {
            tertiary->setToolTip(tooltip);
        }
    };

    if (pointSizeSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Point Size</b><br/>"
            "Controls the screen size of each rendered point.");
        applyTooltip(pointSizeSlider_, pointSizeValueLabel_, pointSizeControl_, tooltip);
    }
    if (pointOpacitySlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Point Opacity</b><br/>"
            "Controls how solid each point appears.<br/>"
            "Lower values reveal deeper layers; higher values make the cloud denser and stronger.");
        applyTooltip(pointOpacitySlider_, pointOpacityValueLabel_, pointOpacityControl_, tooltip);
    }
    if (depthCueSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>Depth Cue</b><br/>"
            "Adds distance-based fading for better front/back separation.<br/>"
            "Higher values make distant points fade more strongly.");
        applyTooltip(depthCueSlider_, depthCueValueLabel_, depthCueControl_, tooltip);
    }
    if (edlStrengthSlider_ != nullptr) {
        const QString tooltip = tr(
            "<b>EDL-style Shading</b><br/>"
            "Enhances point edges and local depth contrast, similar to survey software display enhancement.<br/>"
            "Higher values produce stronger contour darkening and a more layered look.");
        applyTooltip(edlStrengthSlider_, edlStrengthValueLabel_, edlStrengthControl_, tooltip);
    }
    if (roundSplatsCheckBox_ != nullptr) {
        roundSplatsCheckBox_->setToolTip(tr(
            "<b>Round splats</b><br/>"
            "Draw points as circular splats instead of square pixels for a more natural survey-style point cloud look."));
    }
}

void MainWindow::retranslateUi()
{
    retranslateActionsAndBackstage();
    retranslatePanelsAndRuntimeState();
}

void MainWindow::retranslateActionsAndBackstage()
{
    setWindowTitle(tr("LAS Point Cloud Viewer"));

    openAction_->setText(tr("Open"));
    openAction_->setToolTip(tr("Open a point cloud, route file, or project"));
    addPointCloudAction_->setText(tr("Add Data Files"));
    addPointCloudAction_->setToolTip(tr("Add LAS/LAZ point clouds or one Gaussian PLY model"));
    removeDatasetAction_->setText(tr("Remove Selected Dataset"));
    removeDatasetAction_->setToolTip(tr("Remove the selected LAS or LAZ dataset from the project"));
    locateDatasetAction_->setText(tr("Open Folder"));
    locateDatasetAction_->setToolTip(tr("Open the folder that contains the selected dataset"));
    copyDatasetPathAction_->setText(tr("Copy Path"));
    copyDatasetPathAction_->setToolTip(tr("Copy the full path of the selected dataset"));
    expandProjectTreeAction_->setText(tr("Expand All"));
    expandProjectTreeAction_->setToolTip(tr("Expand the project explorer tree"));
    collapseProjectTreeAction_->setText(tr("Collapse All"));
    collapseProjectTreeAction_->setToolTip(tr("Collapse the project explorer tree"));
    openProjectAction_->setText(tr("Open Project"));
    saveProjectAction_->setText(tr("Save Project"));
    saveProjectAsAction_->setText(tr("Save Project As"));
    projectCoordinateSystemsAction_->setText(tr("Project Management"));
    projectCoordinateSystemsAction_->setToolTip(tr("Open project management in the backstage view"));
    clearAction_->setText(tr("Clear"));
    clearAction_->setToolTip(tr("Clear the current scene"));
    exitAction_->setText(tr("Exit"));
    fitSceneAction_->setText(tr("Fit Scene"));
    fitSceneAction_->setToolTip(tr("Reset to a fitted isometric view"));
    topViewAction_->setText(tr("Top"));
    topViewAction_->setToolTip(tr("Switch to top view"));
    frontViewAction_->setText(tr("Front"));
    frontViewAction_->setToolTip(tr("Switch to front view"));
    rightViewAction_->setText(tr("Right"));
    rightViewAction_->setToolTip(tr("Switch to right view"));
    showAxesAction_->setText(tr("Axes"));
    showAxesAction_->setToolTip(tr("Show or hide XYZ axes"));
    showBoundingBoxAction_->setText(tr("Bounds"));
    showBoundingBoxAction_->setToolTip(tr("Show or hide point cloud bounds"));
    captureScreenshotAction_->setText(tr("Screenshot"));
    captureScreenshotAction_->setToolTip(tr("Capture the current application window as a PNG image"));
    const bool recordingActive =
        (recordingProcess_ != nullptr && recordingProcess_->state() != QProcess::NotRunning)
        || (screenRecorder_ != nullptr && screenRecorder_->isRecording());
    if (recordingStatusBadgeLabel_ != nullptr) {
        recordingStatusBadgeLabel_->setVisible(recordingActive);
    }
    toggleScreenRecordingAction_->setText(recordingActive ? tr("Stop Recording") : tr("Start Recording"));
    toggleScreenRecordingAction_->setToolTip(
        recordingActive
            ? tr("Stop the active MP4 screen recording")
            : tr("Start MP4 screen recording for the current application window"));
    darkBackgroundAction_->setText(tr("Dark"));
    darkBackgroundAction_->setToolTip(tr("Switch to dark background"));
    lightBackgroundAction_->setText(tr("Light"));
    lightBackgroundAction_->setToolTip(tr("Switch to light background"));
    rgbColorAction_->setText(tr("RGB"));
    elevationColorAction_->setText(tr("Elevation"));
    singleColorAction_->setText(tr("Single"));
    classificationColorAction_->setText(tr("Classification"));
    themeColorfulAction_->setText(tr("Colorful"));
    themeWhiteAction_->setText(tr("White"));
    themeDarkGrayAction_->setText(tr("Dark Gray"));
    measureAction_->setText(tr("Measure"));
    profileClassificationAction_->setText(tr("Profile Classify"));
    profileClassificationAction_->setToolTip(tr("Enable profile classification and choose rectangle or polygon selection in the panel"));
    showProfileClassificationDockAction_->setText(tr("Classify Panel"));
    showProfileClassificationDockAction_->setToolTip(tr("Show or hide the profile classification dock"));
    saveProfileClassificationEditsAction_->setText(tr("Save Classify Result"));
    saveProfileClassificationEditsAction_->setToolTip(tr("Write current profile classification edits back to LAS files"));
    clearMeasurementAction_->setText(tr("Clear Measure"));
    undoProfileClassificationAction_->setText(tr("Undo Classify"));
    redoProfileClassificationAction_->setText(tr("Redo Classify"));
    clearProfileClassificationEditsAction_->setText(tr("Clear Classify Edits"));
    exportClearanceCsvAction_->setText(tr("Export Clearance CSV"));
    showProfileDockAction_->setText(tr("Profile View"));
    showProfileDockAction_->setToolTip(tr("Show or hide the span profile dock"));
    showWebPanelAction_->setText(tr("Web Panel"));
    showWebPanelAction_->setToolTip(tr("Show or hide the embedded web panel"));
    analyzeVegetationRisksAction_->setText(tr("Analyze Risks"));
    analyzeVegetationRisksAction_->setToolTip(tr("Analyze vegetation risks around the current measured corridor"));
    focusVegetationRiskAction_->setText(tr("Focus Current Risk"));
    createIssueFromRiskAction_->setText(tr("Create Issue"));
    createIssuesFromRisksAction_->setText(tr("Create All Issues"));
    clearVegetationRisksAction_->setText(tr("Clear Risks"));
    generateInspectionRouteAction_->setText(tr("Generate Route"));
    regenerateInspectionRouteAction_->setText(tr("Regenerate Route"));
    clearInspectionRouteAction_->setText(tr("Clear Route"));
    toggleRouteEditingAction_->setText(tr("Edit Route"));
    toggleRouteEditingAction_->setToolTip(tr("Enable waypoint edit, delete, and drag operations for the current route"));
    startInspectionRouteRoamAction_->setText(tr("Start Roam"));
    pauseInspectionRouteRoamAction_->setText(tr("Pause Roam"));
    stopInspectionRouteRoamAction_->setText(tr("Stop Roam"));
    focusRouteWaypointAction_->setText(tr("Focus Route Point"));
    importRouteFileAction_->setText(tr("Import Route File"));
    saveRouteFileAction_->setText(tr("Save Route File"));
    saveRouteFileAsAction_->setText(tr("Save Route File As"));
    reloadRouteFileAction_->setText(tr("Reload Route File"));
    importRouteKmlAction_->setText(tr("Import Route KML"));
    exportRouteKmlAction_->setText(tr("Export Route KML"));
    exportRouteDjiKmzAction_->setText(tr("Export DJI KMZ"));
    startTowerEditAction_->setText(tr("Start Editing"));
    finishTowerEditAction_->setText(tr("Finish Editing"));
    addTowerAction_->setText(tr("Click To Add Tower"));
    insertTowerAction_->setText(tr("Insert Before Current"));
    moveTowerAction_->setText(tr("Move Current Tower"));
    editCurrentTowerAction_->setText(tr("Edit Current Tower"));
    focusTowerAction_->setText(tr("Focus Current Tower"));
    removeTowerAction_->setText(tr("Remove Current Tower"));
    clearTowersAction_->setText(tr("Clear Tower Markers"));
    cancelTowerToolAction_->setText(tr("Cancel Tower Tool"));
    importTowerFileAction_->setText(tr("Import Tower File"));
    saveTowerFileAction_->setText(tr("Save Tower File"));
    saveTowerFileAsAction_->setText(tr("Save Tower File As"));
    reloadTowerFileAction_->setText(tr("Reload Tower File"));
    importTowerFileAction_->setToolTip(tr("Import towers from a LiTower file"));
    saveTowerFileAction_->setToolTip(tr("Save towers to the linked LiTower file"));
    saveTowerFileAsAction_->setToolTip(tr("Save towers to a new LiTower file"));
    reloadTowerFileAction_->setToolTip(tr("Reload towers from the linked LiTower file"));
    showTowerXAction_->setText(tr("Show X"));
    showTowerYAction_->setText(tr("Show Y"));
    showTowerZAction_->setText(tr("Show Z"));
    startIssueMarkAction_->setText(tr("Mark Issue"));
    startIssueMarkAction_->setToolTip(tr("Click a point in the view to add an inspection issue"));
    cancelIssueToolAction_->setText(tr("Cancel Issue Tool"));
    focusIssueAction_->setText(tr("Focus Current Issue"));
    removeIssueAction_->setText(tr("Remove Current Issue"));
    clearIssuesAction_->setText(tr("Clear Issues"));
    exportIssuesCsvAction_->setText(tr("Export Issues CSV"));
    exportInspectionReportAction_->setText(tr("Export Inspection Report"));
    showLogAction_->setText(tr("Log"));
    showLogAction_->setToolTip(tr("Show or hide the log panel"));
    languageEnglishAction_->setText(tr("English"));
    languageChineseAction_->setText(tr("Chinese"));

    clipModeNoneAction_->setText(tr("No Clip"));
    clipModeNoneAction_->setToolTip(tr("Disable clipping and show all points"));
    clipModeBoxAction_->setText(tr("Box Clip"));
    clipModeBoxAction_->setToolTip(tr("Pick two point-cloud points to build a clipping box. Preview is shown before the second click."));
    clipModePolygonAction_->setText(tr("Polygon Clip"));
    clipModePolygonAction_->setToolTip(tr("Draw a screen-space polygon in the current view. The finished polygon is frozen into a 3D clip volume."));
    clipBoxWorldAlignedAction_->setText(tr("World Aligned"));
    clipBoxWorldAlignedAction_->setToolTip(tr("Build the clip box aligned to the world XYZ axes."));
    clipBoxViewAlignedAction_->setText(tr("View Aligned"));
    clipBoxViewAlignedAction_->setToolTip(tr("Build the clip box aligned to the camera axes captured at the first click."));
    clipScopeActiveDatasetAction_->setText(tr("Active Dataset"));
    clipScopeActiveDatasetAction_->setToolTip(tr("Apply clipping only to the currently selected point-cloud dataset."));
    clipScopeVisibleDatasetsAction_->setText(tr("Visible Datasets"));
    clipScopeVisibleDatasetsAction_->setToolTip(tr("Apply clipping to all currently visible point-cloud datasets."));
    clipToggleInsideAction_->setText(tr("Keep Inside"));
    clipToggleInsideAction_->setToolTip(tr("Switch between Keep Inside and Keep Outside for the current clip region."));
    clipApplyExportAction_->setText(tr("Apply & Export"));
    clipApplyExportAction_->setToolTip(tr("Export the currently clipped result as a new LAS file. The exported file is added to the project tree."));

    if (backstageSystemButton_ != nullptr) {
        backstageSystemButton_->setText(tr("File"));
        backstageSystemButton_->setToolTip(tr("Open the backstage view"));
    }
    if (backstageOpenPage_ != nullptr) {
        backstageOpenPage_->setWindowTitle(tr("Open"));
    }
    if (backstageOpenProjectPage_ != nullptr) {
        backstageOpenProjectPage_->setWindowTitle(tr("Open Project"));
    }
    if (backstageProjectPropertiesPage_ != nullptr) {
        backstageProjectPropertiesPage_->setWindowTitle(tr("Project Management"));
    }
    if (backstageApplicationSettingsPage_ != nullptr) {
        backstageApplicationSettingsPage_->setWindowTitle(tr("Application Settings"));
    }
    if (backstageAboutPage_ != nullptr) {
        backstageAboutPage_->setWindowTitle(tr("About"));
    }
    if (backstageOpenPageAction_ != nullptr) {
        backstageOpenPageAction_->setText(tr("Open"));
        backstageOpenPageAction_->setIcon(createRibbonIcon(RibbonGlyph::Open));
    }
    if (backstageOpenProjectPageAction_ != nullptr) {
        backstageOpenProjectPageAction_->setText(tr("Open Project"));
        backstageOpenProjectPageAction_->setIcon(createRibbonIcon(RibbonGlyph::Open));
    }
    if (backstageSaveAction_ != nullptr) {
        backstageSaveAction_->setText(saveProjectAction_->text());
        backstageSaveAction_->setIcon(createRibbonIcon(RibbonGlyph::Save));
    }
    if (backstageSaveAsAction_ != nullptr) {
        backstageSaveAsAction_->setText(saveProjectAsAction_->text());
        backstageSaveAsAction_->setIcon(createRibbonIcon(RibbonGlyph::Save));
    }
    if (backstageProjectPropertiesPageAction_ != nullptr) {
        backstageProjectPropertiesPageAction_->setText(tr("Project Management"));
        backstageProjectPropertiesPageAction_->setIcon(projectCoordinateSystemsAction_->icon());
    }
    if (backstageApplicationSettingsPageAction_ != nullptr) {
        backstageApplicationSettingsPageAction_->setText(tr("Application Settings"));
        backstageApplicationSettingsPageAction_->setIcon(createRibbonIcon(RibbonGlyph::Settings));
    }
    if (backstageAboutPageAction_ != nullptr) {
        backstageAboutPageAction_->setText(tr("About"));
        backstageAboutPageAction_->setIcon(createRibbonIcon(RibbonGlyph::About));
    }
    if (backstageExitAction_ != nullptr) {
        backstageExitAction_->setText(exitAction_->text());
        backstageExitAction_->setIcon(createRibbonIcon(RibbonGlyph::Exit));
    }
    if (backstageOpenTitleLabel_ != nullptr) {
        backstageOpenTitleLabel_->setText(tr("Open"));
    }
    if (backstageOpenSubtitleLabel_ != nullptr) {
        backstageOpenSubtitleLabel_->setText(tr("Open point clouds and projects, or continue from a recent engineering file."));
    }
    if (backstageOpenProjectTitleLabel_ != nullptr) {
        backstageOpenProjectTitleLabel_->setText(tr("Open Project"));
    }
    if (backstageOpenProjectSubtitleLabel_ != nullptr) {
        backstageOpenProjectSubtitleLabel_->setText(tr("Select a recent project or browse to a project file."));
    }
    if (backstageOpenProjectWidget_ != nullptr) {
        backstageOpenProjectWidget_->retranslateUi();
    }
    if (backstageProjectPropertiesTitleLabel_ != nullptr) {
        backstageProjectPropertiesTitleLabel_->setText(tr("Project Management"));
    }
    if (backstageProjectPropertiesSubtitleLabel_ != nullptr) {
        backstageProjectPropertiesSubtitleLabel_->setText(tr("Review the active project file, datasets, and coordinate system configuration."));
    }
    if (backstageProjectPropertiesWidget_ != nullptr) {
        backstageProjectPropertiesWidget_->retranslateUi();
    }
    if (backstageApplicationSettingsTitleLabel_ != nullptr) {
        backstageApplicationSettingsTitleLabel_->setText(tr("Application Settings"));
    }
    if (backstageApplicationSettingsSubtitleLabel_ != nullptr) {
        backstageApplicationSettingsSubtitleLabel_->setText(tr("Adjust the office theme, interface language, workspace panels, and capture behavior."));
    }
    if (backstageApplicationSettingsWidget_ != nullptr) {
        backstageApplicationSettingsWidget_->retranslateUi();
    }
    if (backstageAboutTitleLabel_ != nullptr) {
        backstageAboutTitleLabel_->setText(tr("About"));
    }
    if (backstageAboutSubtitleLabel_ != nullptr) {
        backstageAboutSubtitleLabel_->setText(tr("Build information and the key runtime components used by this application."));
    }
    if (backstageAboutWidget_ != nullptr) {
        backstageAboutWidget_->retranslateUi();
    }
    if (backstageProjectPathLineEdit_ != nullptr) {
        backstageProjectPathLineEdit_->setPlaceholderText(tr("Project file path"));
    }
    if (backstageProjectBrowseButton_ != nullptr) {
        backstageProjectBrowseButton_->setText(tr("Browse..."));
    }
    if (backstageProjectOpenButton_ != nullptr) {
        backstageProjectOpenButton_->setText(tr("Open"));
    }
    if (backstageShowLogCheckBox_ != nullptr) {
        backstageShowLogCheckBox_->setText(tr("Show log panel"));
    }
    if (backstageWebPanelUrlLineEdit_ != nullptr) {
        backstageWebPanelUrlLineEdit_->setPlaceholderText(defaultWebPanelUrl());
    }
    if (backstageCaptureBrowseButton_ != nullptr) {
        backstageCaptureBrowseButton_->setText(tr("Browse..."));
    }
    if (backstageCaptureSaveDirectoryLineEdit_ != nullptr) {
        backstageCaptureSaveDirectoryLineEdit_->setPlaceholderText(tr("Choose a folder for screenshots and recordings"));
    }
    if (backstageCaptureAutoSaveCheckBox_ != nullptr) {
        backstageCaptureAutoSaveCheckBox_->setText(tr("Use default path and save automatically (no save dialog)"));
    }
    if (backstageOpenProjectWidget_ != nullptr) {
        if (QGroupBox* recentProjectsGroup = backstageOpenProjectWidget_->recentProjectsGroup()) {
            recentProjectsGroup->setTitle(tr("Recent Projects"));
        }
        if (QGroupBox* projectFileGroup = backstageOpenProjectWidget_->projectFileGroup()) {
            projectFileGroup->setTitle(tr("Project File"));
        }
    }
    if (backstageApplicationSettingsWidget_ != nullptr) {
        if (QGroupBox* captureGroup = backstageApplicationSettingsWidget_->captureGroup()) {
            captureGroup->setTitle(tr("Capture"));
        }
    }
}

void MainWindow::retranslatePanelsAndRuntimeState()
{
    if (homePage_ != nullptr) {
        homePage_->setTitle(tr("Home"));
    }
    if (routePage_ != nullptr) {
        routePage_->setTitle(tr("Route"));
    }
    if (datasetRibbonGroup_ != nullptr) {
        datasetRibbonGroup_->setTitle(tr("Dataset"));
    }
    if (cameraRibbonGroup_ != nullptr) {
        cameraRibbonGroup_->setTitle(tr("Camera"));
    }
    if (sceneRibbonGroup_ != nullptr) {
        sceneRibbonGroup_->setTitle(tr("Scene"));
    }
    if (captureRibbonGroup_ != nullptr) {
        captureRibbonGroup_->setTitle(tr("Capture"));
    }
    if (measureRibbonGroup_ != nullptr) {
        measureRibbonGroup_->setTitle(tr("Measure"));
    }
    if (classificationRibbonGroup_ != nullptr) {
        classificationRibbonGroup_->setTitle(tr("Classification"));
    }
    if (clipRibbonGroup_ != nullptr) {
        clipRibbonGroup_->setTitle(tr("Clip"));
    }
    if (routePlanningRibbonGroup_ != nullptr) {
        routePlanningRibbonGroup_->setTitle(tr("Route Planning"));
    }
    if (routeFileRibbonGroup_ != nullptr) {
        routeFileRibbonGroup_->setTitle(tr("Route Files"));
    }
    if (routeExchangeRibbonGroup_ != nullptr) {
        routeExchangeRibbonGroup_->setTitle(tr("Route Exchange"));
    }
    if (workspaceRibbonGroup_ != nullptr) {
        workspaceRibbonGroup_->setTitle(tr("Workspace"));
    }
    if (towerRibbonGroup_ != nullptr) {
        towerRibbonGroup_->setTitle(tr("Tower Editing"));
    }
    if (themeRibbonGroup_ != nullptr) {
        themeRibbonGroup_->setTitle(tr("Office Theme"));
    }
    if (languageRibbonGroup_ != nullptr) {
        languageRibbonGroup_->setTitle(tr("Language"));
    }

    refreshBackstageRecentProjects();
    refreshBackstageProjectPropertiesPage();
    refreshBackstageApplicationSettingsPage();
    refreshBackstageAboutPage();

    if (projectDock_ != nullptr) {
        if (projectExplorerController_ != nullptr) {
            projectExplorerController_->retranslateUi();
        } else {
            projectDock_->retranslateUi();
        }
    }
    if (inspectorDock_ != nullptr) {
        inspectorDock_->retranslateUi();
    }
    if (routeDetailsDock_ != nullptr) {
        routeDetailsDock_->retranslateUi();
    }
    if (profileDock_ != nullptr) {
        profileDock_->retranslateUi();
    }
    if (profileClassificationDock_ != nullptr) {
        profileClassificationDock_->retranslateUi();
    }
    if (viewQuickToolBar_ != nullptr) {
        viewQuickToolBar_->setWindowTitle(tr("View Toolbar"));
    }
    if (logDock_ != nullptr) {
        logDock_->retranslateUi();
    }
    if (webPageDock_ != nullptr) {
        webPageDock_->retranslateUi();
    }
    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setTabText(0, tr("Overview"));
        inspectorTabWidget_->setTabText(1, tr("Tower"));
        inspectorTabWidget_->setTabText(2, tr("Issues"));
        inspectorTabWidget_->setTabText(3, tr("Rendering"));
        inspectorTabWidget_->setTabText(4, tr("Measurement"));
        inspectorTabWidget_->setTabText(5, tr("Analysis"));
        inspectorTabWidget_->setTabText(6, tr("Navigation"));
    }
    if (routeWaypointLabelModeComboBox_ != nullptr) {
        const int selectedMode = routeWaypointLabelModeComboBox_->currentData().toInt();
        const QSignalBlocker blocker(routeWaypointLabelModeComboBox_);
        routeWaypointLabelModeComboBox_->clear();
        routeWaypointLabelModeComboBox_->addItem(tr("Name"), static_cast<int>(RouteLabelDisplayMode::Name));
        routeWaypointLabelModeComboBox_->addItem(tr("Index"), static_cast<int>(RouteLabelDisplayMode::Sequence));
        routeWaypointLabelModeComboBox_->addItem(tr("Compact Name"), static_cast<int>(RouteLabelDisplayMode::CompactName));
        routeWaypointLabelModeComboBox_->addItem(tr("Compact Index"), static_cast<int>(RouteLabelDisplayMode::CompactSequence));
        routeWaypointLabelModeComboBox_->addItem(tr("Hidden"), static_cast<int>(RouteLabelDisplayMode::Hidden));
        const int selectedModeIndex = routeWaypointLabelModeComboBox_->findData(selectedMode);
        routeWaypointLabelModeComboBox_->setCurrentIndex(selectedModeIndex >= 0 ? selectedModeIndex : 0);
    }
    if (routePartLabelModeComboBox_ != nullptr) {
        const int selectedMode = routePartLabelModeComboBox_->currentData().toInt();
        const QSignalBlocker blocker(routePartLabelModeComboBox_);
        routePartLabelModeComboBox_->clear();
        routePartLabelModeComboBox_->addItem(tr("Name"), static_cast<int>(RouteLabelDisplayMode::Name));
        routePartLabelModeComboBox_->addItem(tr("Index"), static_cast<int>(RouteLabelDisplayMode::Sequence));
        routePartLabelModeComboBox_->addItem(tr("Compact Name"), static_cast<int>(RouteLabelDisplayMode::CompactName));
        routePartLabelModeComboBox_->addItem(tr("Compact Index"), static_cast<int>(RouteLabelDisplayMode::CompactSequence));
        routePartLabelModeComboBox_->addItem(tr("Hidden"), static_cast<int>(RouteLabelDisplayMode::Hidden));
        const int selectedModeIndex = routePartLabelModeComboBox_->findData(selectedMode);
        routePartLabelModeComboBox_->setCurrentIndex(selectedModeIndex >= 0 ? selectedModeIndex : 0);
    }
    if (datasetGroupBox_ != nullptr) {
        datasetGroupBox_->setTitle(tr("Dataset Summary"));
    }
    if (datasetSummaryWidget_ != nullptr) {
        datasetSummaryWidget_->retranslateUi();
    }
    if (towerTableWidget_ != nullptr) {
        towerTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Name"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") });
    }
    if (issueTableWidget_ != nullptr) {
        issueTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Title"), tr("Severity"), tr("Status"), tr("Tower"), tr("Category") });
    }
    if (issueMenuButton_ != nullptr) {
        issueMenuButton_->setText(tr("Menu"));
    }
    if (renderingGroupBox_ != nullptr) {
        renderingGroupBox_->setTitle(tr("Rendering Controls"));
    }
    if (classificationColorsGroupBox_ != nullptr) {
        classificationColorsGroupBox_->setTitle(tr("Classification Mapping"));
    }
    if (classificationColorsTableWidget_ != nullptr) {
        classificationColorsTableWidget_->setHorizontalHeaderLabels({ tr("Show"), tr("Class ID"), tr("Class Name"), tr("Color") });
    }
    if (resetClassificationColorsButton_ != nullptr) {
        resetClassificationColorsButton_->setText(tr("Reset Defaults"));
    }
    if (profileClassificationController_ != nullptr) {
        profileClassificationController_->retranslateUi();
    } else if (profileClassificationGroupBox_ != nullptr) {
        profileClassificationGroupBox_->retranslateUi();
    }
    if (navigationSettingsWidget_ != nullptr) {
        navigationSettingsWidget_->retranslateUi();
    }
    refreshLogPanel();
    updateProfileClassificationPanel();
    if (measurementGroupBox_ != nullptr) {
        measurementGroupBox_->setTitle(tr("Measurement"));
    }
    if (clearanceGroupBox_ != nullptr) {
        clearanceGroupBox_->setTitle(tr("Clearance Analysis"));
    }
    if (clearanceSegmentsGroupBox_ != nullptr) {
        clearanceSegmentsGroupBox_->setTitle(tr("Path Segment Details"));
    }
    if (analysisParametersGroupBox_ != nullptr) {
        analysisParametersGroupBox_->setTitle(tr("Vegetation Risk Analysis"));
    }
    if (vegetationRisksGroupBox_ != nullptr) {
        vegetationRisksGroupBox_->setTitle(tr("Detected Risk Clusters"));
    }
    if (routePlanningGroupBox_ != nullptr) {
        routePlanningGroupBox_->setTitle(tr("Inspection Route Planning"));
    }
    if (routePartsGroupBox_ != nullptr) {
        routePartsGroupBox_->setTitle(tr("Route Part Points"));
    }
    if (routeWaypointsGroupBox_ != nullptr) {
        routeWaypointsGroupBox_->setTitle(tr("Route Waypoints"));
    }
    if (routeWaypointTargetsGroupBox_ != nullptr) {
        routeWaypointTargetsGroupBox_->setTitle(tr("Waypoint Targets"));
    }
    if (routeQaGroupBox_ != nullptr) {
        routeQaGroupBox_->setTitle(tr("Route QA"));
    }
    if (navigationGroupBox_ != nullptr) {
        navigationGroupBox_->setTitle(tr("Navigation"));
    }
    if (towerDetailsGroupBox_ != nullptr) {
        towerDetailsGroupBox_->setTitle(tr("Selected Tower Details"));
    }
    if (issueDetailsGroupBox_ != nullptr) {
        issueDetailsGroupBox_->setTitle(tr("Selected Issue Details"));
    }
    setFormFieldLabel(datasetLayout_, datasetNameValueLabel_, tr("Name"));
    setFormFieldLabel(datasetLayout_, datasetPathValueLabel_, tr("Path"));
    setFormFieldLabel(datasetLayout_, datasetPointsValueLabel_, tr("Points"));
    setFormFieldLabel(datasetLayout_, datasetBoundsValueLabel_, tr("Bounds"));
    setFormFieldLabel(datasetLayout_, datasetExtentValueLabel_, tr("Extent"));
    setFormFieldLabel(datasetLayout_, datasetColorValueLabel_, tr("Color Source"));
    setFormFieldLabel(towerDetailsLayout_, towerCodeEdit_, tr("Code"));
    setFormFieldLabel(towerDetailsLayout_, towerLineNameEdit_, tr("Line"));
    setFormFieldLabel(towerDetailsLayout_, towerVoltageLevelEdit_, tr("Voltage"));
    setFormFieldLabel(towerDetailsLayout_, towerTypeComboBox_, tr("Tower Category"));
    setFormFieldLabel(towerDetailsLayout_, towerStructureTypeEdit_, tr("Structure Type"));
    setFormFieldLabel(towerDetailsLayout_, towerInspectionDateEdit_, tr("Inspection Date"));
    setFormFieldLabel(towerDetailsLayout_, towerStatusEdit_, tr("Tower Status"));
    setFormFieldLabel(towerDetailsLayout_, towerNotesEdit_, tr("Notes"));
    if (towerTypeComboBox_ != nullptr && towerTypeComboBox_->count() >= 3) {
        towerTypeComboBox_->setItemText(0, towerTypeDisplayName(TowerType::Unknown));
        towerTypeComboBox_->setItemText(1, towerTypeDisplayName(TowerType::Tangent));
        towerTypeComboBox_->setItemText(2, towerTypeDisplayName(TowerType::Strain));
    }
    setFormFieldLabel(issueDetailsLayout_, issueTitleEdit_, tr("Title"));
    setFormFieldLabel(issueDetailsLayout_, issueCategoryComboBox_, tr("Category"));
    setFormFieldLabel(issueDetailsLayout_, issueSeverityComboBox_, tr("Severity"));
    setFormFieldLabel(issueDetailsLayout_, issueStatusComboBox_, tr("Issue Status"));
    setFormFieldLabel(issueDetailsLayout_, issueRelatedTowerComboBox_, tr("Related Tower"));
    setFormFieldLabel(issueDetailsLayout_, issueImagePathEdit_, tr("Image Path"));
    setFormFieldLabel(issueDetailsLayout_, issueLocationValueLabel_, tr("Location"));
    setFormFieldLabel(issueDetailsLayout_, issueCreatedAtValueLabel_, tr("Created At"));
    setFormFieldLabel(issueDetailsLayout_, issueDescriptionEdit_, tr("Description"));
    if (issueCategoryComboBox_ != nullptr && issueCategoryComboBox_->count() >= 5) {
        issueCategoryComboBox_->setItemText(0, tr("Vegetation"));
        issueCategoryComboBox_->setItemText(1, tr("Insulator"));
        issueCategoryComboBox_->setItemText(2, tr("Tower Body"));
        issueCategoryComboBox_->setItemText(3, tr("Channel Risk"));
        issueCategoryComboBox_->setItemText(4, tr("Other"));
    }
    if (issueSeverityComboBox_ != nullptr && issueSeverityComboBox_->count() >= 4) {
        issueSeverityComboBox_->setItemText(0, issueSeverityDisplayName(IssueSeverity::Info));
        issueSeverityComboBox_->setItemText(1, issueSeverityDisplayName(IssueSeverity::Minor));
        issueSeverityComboBox_->setItemText(2, issueSeverityDisplayName(IssueSeverity::Major));
        issueSeverityComboBox_->setItemText(3, issueSeverityDisplayName(IssueSeverity::Critical));
    }
    if (issueStatusComboBox_ != nullptr && issueStatusComboBox_->count() >= 3) {
        issueStatusComboBox_->setItemText(0, issueStatusDisplayName(IssueStatus::Open));
        issueStatusComboBox_->setItemText(1, issueStatusDisplayName(IssueStatus::Monitoring));
        issueStatusComboBox_->setItemText(2, issueStatusDisplayName(IssueStatus::Resolved));
    }

    setFormFieldLabel(renderingLayout_, pointSizeControl_, tr("Point Size"));
    setFormFieldLabel(renderingLayout_, pointOpacityControl_, tr("Point Opacity"));
    setFormFieldLabel(renderingLayout_, depthCueControl_, tr("Depth Cue"));
    setFormFieldLabel(renderingLayout_, edlStrengthControl_, tr("EDL-style Shading"));
    setFormFieldLabel(renderingLayout_, colorModeComboBox_, tr("Color Mode"));
    setFormFieldLabel(renderingLayout_, pointColorButton_, tr("Single Color"));
    setFormFieldLabel(renderingLayout_, backgroundColorButton_, tr("Background"));
    colorModeComboBox_->setItemText(0, tr("RGB"));
    colorModeComboBox_->setItemText(1, tr("Elevation Ramp"));
    colorModeComboBox_->setItemText(2, tr("Single Color"));
    if (colorModeComboBox_->count() >= 4) {
        colorModeComboBox_->setItemText(3, tr("Classification"));
    }
    roundSplatsCheckBox_->setText(tr("Round splats (survey style)"));
    axesCheckBox_->setText(tr("Show XYZ axes"));
    boundingBoxCheckBox_->setText(tr("Show bounding box"));
    updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));
    updateSliderValueLabel(wheelZoomSensitivitySlider_, wheelZoomSensitivityValueLabel_, tr("%1%"));
    updateVisualizationTooltips();

    setFormFieldLabel(measurementLayout_, measurementStartValueLabel_, tr("Start Point"));
    setFormFieldLabel(measurementLayout_, measurementEndValueLabel_, tr("End Point"));
    setFormFieldLabel(measurementLayout_, measurementDistanceValueLabel_, tr("3D Distance"));
    setFormFieldLabel(measurementLayout_, measurementHorizontalDistanceValueLabel_, tr("Horizontal Distance"));
    setFormFieldLabel(measurementLayout_, measurementDeltaZValueLabel_, tr("Height Delta"));
    setFormFieldLabel(measurementLayout_, measurementSegmentsValueLabel_, tr("Path Segments"));
    setFormFieldLabel(clearanceLayout_, clearanceRulePresetComboBox_, tr("Rule Preset"));
    setFormFieldLabel(clearanceLayout_, clearanceThresholdSpinBox_, tr("Critical Threshold"));
    setFormFieldLabel(clearanceLayout_, clearanceRuleBandsValueLabel_, tr("Risk Bands"));
    setFormFieldLabel(clearanceLayout_, clearanceShortestValueLabel_, tr("Shortest Segment"));
    setFormFieldLabel(clearanceLayout_, clearanceWarningCountValueLabel_, tr("Risk Segments"));
    setFormFieldLabel(clearanceLayout_, clearanceStatusValueLabel_, tr("Status"));
    if (clearanceSegmentsSummaryLabel_ != nullptr) {
        clearanceSegmentsSummaryLabel_->setText(tr("Add at least two measured points to list corridor segments and export clearance details."));
    }
    if (clearanceSegmentsTableWidget_ != nullptr) {
        clearanceSegmentsTableWidget_->setHorizontalHeaderLabels({
            tr("Segment"),
            tr("From"),
            tr("To"),
            tr("Chainage"),
            tr("Horizontal"),
            tr("3D"),
            tr("dZ"),
            tr("Status")
        });
    }
    if (clearanceRulePresetComboBox_ != nullptr && clearanceRulePresetComboBox_->count() >= 4) {
        clearanceRulePresetComboBox_->setItemText(0, clearanceRulePresetDisplayName(ClearanceRulePreset::TransmissionCorridor));
        clearanceRulePresetComboBox_->setItemText(1, clearanceRulePresetDisplayName(ClearanceRulePreset::DistributionCorridor));
        clearanceRulePresetComboBox_->setItemText(2, clearanceRulePresetDisplayName(ClearanceRulePreset::StructureApproach));
        clearanceRulePresetComboBox_->setItemText(3, clearanceRulePresetDisplayName(ClearanceRulePreset::Custom));
    }
    measurementToggleButton_->setText(
        viewer_ != nullptr && viewer_->measurementEnabled() ? tr("Stop Measurement") : tr("Start Measurement"));
    measurementClearButton_->setText(tr("Clear Measurement"));
    if (clearanceThresholdSpinBox_ != nullptr) {
        clearanceThresholdSpinBox_->setSuffix(tr(" m"));
        clearanceThresholdSpinBox_->setSpecialValueText(tr("Disabled"));
    }
    if (vegetationSearchRadiusSpinBox_ != nullptr) {
        vegetationSearchRadiusSpinBox_->setSuffix(tr(" m"));
    }
    if (vegetationClusterGapSpinBox_ != nullptr) {
        vegetationClusterGapSpinBox_->setSuffix(tr(" m"));
    }
    if (analysisParametersLayout_ != nullptr) {
        setFormFieldLabel(analysisParametersLayout_, vegetationSearchRadiusSpinBox_, tr("Search Radius"));
        setFormFieldLabel(analysisParametersLayout_, vegetationClusterGapSpinBox_, tr("Cluster Gap"));
        setFormFieldLabel(analysisParametersLayout_, vegetationClusterPointCountSpinBox_, tr("Min Cluster Points"));
        setFormFieldLabel(analysisParametersLayout_, vegetationRiskCountValueLabel_, tr("Risk Count"));
        setFormFieldLabel(analysisParametersLayout_, vegetationRiskStatusValueLabel_, tr("Status"));
        setFormFieldLabel(analysisParametersLayout_, vegetationRiskSummaryLabel_, tr("Summary"));
    }
    if (routePlanningGroupBox_ != nullptr) {
        if (auto* routePlanningLayout = qobject_cast<QFormLayout*>(routePlanningGroupBox_->layout())) {
            setFormFieldLabel(routePlanningLayout, aircraftProfileComboBox_, tr("DJI Profile"));
            setFormFieldLabel(routePlanningLayout, routeSafetyHeightSpinBox_, tr("Safety Height"));
            setFormFieldLabel(routePlanningLayout, routeWaypointSpeedSpinBox_, tr("Waypoint Speed"));
            setFormFieldLabel(routePlanningLayout, routeWaypointSpacingSpinBox_, tr("Waypoint Spacing"));
            setFormFieldLabel(routePlanningLayout, routeSmoothingStrengthSpinBox_, tr("Smoothing"));
            setFormFieldLabel(routePlanningLayout, routeHeightOffsetSpinBox_, tr("Height Offset"));
            setFormFieldLabel(routePlanningLayout, routeRoamSpeedSpinBox_, tr("Roam Speed"));
            setFormFieldLabel(routePlanningLayout, routeRoamViewModeComboBox_, tr("Roam View Mode"));
            setFormFieldLabel(routePlanningLayout, routeRoamControlsRow_, tr("Roam Controls"));
            setFormFieldLabel(routePlanningLayout, routeStatusValueLabel_, tr("Status"));
            setFormFieldLabel(routePlanningLayout, routeSummaryValueLabel_, tr("Summary"));
        }
    }
    if (routeRoamViewModeComboBox_ != nullptr) {
        const int selectedMode = routeRoamViewModeComboBox_->currentData().toInt();
        const QSignalBlocker blocker(routeRoamViewModeComboBox_);
        routeRoamViewModeComboBox_->clear();
        routeRoamViewModeComboBox_->addItem(tr("Third Person"), static_cast<int>(RouteRoamViewMode::ThirdPerson));
        routeRoamViewModeComboBox_->addItem(tr("First Person"), static_cast<int>(RouteRoamViewMode::FirstPerson));
        const int selectedIndex = routeRoamViewModeComboBox_->findData(selectedMode);
        routeRoamViewModeComboBox_->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    }
    if (routeRoamStartButton_ != nullptr) {
        routeRoamStartButton_->setText(tr("Start Roam"));
    }
    if (routeRoamPauseResumeButton_ != nullptr) {
        routeRoamPauseResumeButton_->setText(
            viewer_ != nullptr && viewer_->inspectionRouteRoamPaused() ? tr("Resume Roam") : tr("Pause Roam"));
    }
    if (routeRoamStopButton_ != nullptr) {
        routeRoamStopButton_->setText(tr("Stop Roam"));
    }
    if (routeRoamFloatingDialog_ != nullptr) {
        routeRoamFloatingDialog_->setWindowTitle(tr("Route Roam Controls"));
    }
    if (routeRoamFloatingViewModeComboBox_ != nullptr) {
        const int selectedMode = routeRoamFloatingViewModeComboBox_->currentData().toInt();
        const QSignalBlocker blocker(routeRoamFloatingViewModeComboBox_);
        routeRoamFloatingViewModeComboBox_->clear();
        routeRoamFloatingViewModeComboBox_->addItem(tr("Third Person"), static_cast<int>(RouteRoamViewMode::ThirdPerson));
        routeRoamFloatingViewModeComboBox_->addItem(tr("First Person"), static_cast<int>(RouteRoamViewMode::FirstPerson));
        const int selectedIndex = routeRoamFloatingViewModeComboBox_->findData(selectedMode);
        routeRoamFloatingViewModeComboBox_->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    }
    if (routeRoamFloatingCaptureLabel_ != nullptr && routeRoamLastCaptureSummary_.trimmed().isEmpty()) {
        routeRoamFloatingCaptureLabel_->setText(tr("Awaiting photo capture."));
    }
    if (routeRoamFloatingDialog_ != nullptr) {
        if (auto* floatingLayout = qobject_cast<QFormLayout*>(routeRoamFloatingDialog_->layout())) {
            setFormFieldLabel(floatingLayout, routeRoamFloatingSpeedSpinBox_, tr("Roam Speed"));
            setFormFieldLabel(floatingLayout, routeRoamFloatingViewModeComboBox_, tr("Roam View Mode"));
            setFormFieldLabel(floatingLayout, routeRoamFloatingCaptureLabel_, tr("Capture"));
        }
    }
    if (aircraftProfileComboBox_ != nullptr) {
        const QList<DjiAircraftProfile> profiles = supportedDjiAircraftProfiles();
        for (int index = 0; index < profiles.size() && index < aircraftProfileComboBox_->count(); ++index) {
            aircraftProfileComboBox_->setItemText(index, djiAircraftProfileDisplayName(profiles.at(index)));
        }
    }
    if (preferVegetationClassificationCheckBox_ != nullptr) {
        preferVegetationClassificationCheckBox_->setText(tr("Prefer LAS vegetation classifications when available"));
    }
    if (vegetationRisksTableWidget_ != nullptr) {
        vegetationRisksTableWidget_->setHorizontalHeaderLabels({
            tr("Index"),
            tr("Title"),
            tr("Severity"),
            tr("Min Distance"),
            tr("Chainage"),
            tr("Tower"),
            tr("Points")
        });
    }
    if (routeWaypointsTableWidget_ != nullptr) {
        routeWaypointsTableWidget_->setHorizontalHeaderLabels({
            tr("Index"),
            tr("Part"),
            tr("X"),
            tr("Y"),
            tr("Z"),
            tr("Aircraft Yaw"),
            tr("Gimbal Pitch"),
            tr("Camera Yaw"),
            tr("Camera Pitch")
        });
    }
    if (routeWaypointTargetsTableWidget_ != nullptr) {
        routeWaypointTargetsTableWidget_->setHorizontalHeaderLabels({
            tr("Index"),
            tr("Part"),
            tr("Focal Ratio"),
            tr("Camera Yaw"),
            tr("Camera Pitch"),
            tr("Target Point")
        });
    }
    if (routePartPointsTableWidget_ != nullptr) {
        routePartPointsTableWidget_->setHorizontalHeaderLabels({
            tr("Index"),
            tr("Part Name"),
            tr("Hardware"),
            tr("Phase"),
            tr("Camera Angle"),
            tr("X"),
            tr("Y"),
            tr("Z")
        });
    }
    if (routeQaIssuesTableWidget_ != nullptr) {
        routeQaIssuesTableWidget_->setHorizontalHeaderLabels({
            tr("Severity"),
            tr("Issue"),
            tr("Location"),
            tr("Part"),
            tr("Description")
        });
    }

    if (routeWaypointShowCoordinatesCheckBox_ != nullptr) {
        routeWaypointShowCoordinatesCheckBox_->setText(tr("Show Coordinates"));
    }
    if (routeWaypointShowCaptureAnglesCheckBox_ != nullptr) {
        routeWaypointShowCaptureAnglesCheckBox_->setText(tr("Show Capture Angles"));
    }
    if (routePartShowCoordinatesCheckBox_ != nullptr) {
        routePartShowCoordinatesCheckBox_->setText(tr("Show Coordinates"));
    }
    if (routePartShowCaptureAnglesCheckBox_ != nullptr) {
        routePartShowCaptureAnglesCheckBox_->setText(tr("Show Capture Angles"));
    }

    invertOrbitCheckBox_->setText(tr("Invert orbit drag"));
    invertPanCheckBox_->setText(tr("Invert pan drag"));
    invertWheelCheckBox_->setText(tr("Invert wheel zoom"));
    setFormFieldLabel(navigationToggleLayout_, wheelZoomSensitivityControl_, tr("Zoom Sensitivity"));
    updateSliderValueLabel(wheelZoomSensitivitySlider_, wheelZoomSensitivityValueLabel_, tr("%1%"));
    if (wheelZoomSensitivityControl_ != nullptr) {
        wheelZoomSensitivityControl_->setToolTip(tr("Lower values zoom more gently. Higher values zoom faster."));
    }
    if (wheelZoomSensitivitySlider_ != nullptr) {
        wheelZoomSensitivitySlider_->setToolTip(tr("Lower values zoom more gently. Higher values zoom faster."));
    }
    if (wheelZoomSensitivityValueLabel_ != nullptr) {
        wheelZoomSensitivityValueLabel_->setToolTip(tr("Lower values zoom more gently. Higher values zoom faster."));
    }

    if (viewer_ != nullptr) {
        setColorButtonAppearance(pointColorButton_, viewer_->visualizationOptions().singleColor, tr("Pick Color"));
        setColorButtonAppearance(backgroundColorButton_, viewer_->visualizationOptions().backgroundColor, tr("Pick Background"));
        setColorButtonAppearance(routeWaypointColorButton_, viewer_->inspectionRouteWaypointColor(), tr("Waypoint Color"));
        setColorButtonAppearance(routePartPointColorButton_, viewer_->inspectionRoutePartPointColor(), tr("Part Point Color"));
        setColorButtonAppearance(routeTrajectoryColorButton_, viewer_->inspectionRouteTrajectoryColor(), tr("Trajectory Color"));
    }
    updateClassificationColorTable();
    updateProfileClassificationPanel();
    updateWindowControlButtons();
    rebuildProjectTree();
    updateDatasetPanel();
    updateNavigationHelpText();
    updateMeasurementPanel();
    updateRoutePlanningPanel();
    updateTowerPanel();
    updateActionState();
    if (viewer_ != nullptr) {
        viewer_->update();
    }
}

void MainWindow::openProjectExplorerFile()
{
    hideBackstageView();
    const QString filePath = showStyledOpenFileNameDialog(
        this,
        tr("Open"),
        QString(),
        tr("Supported Files (*.las *.laz *.ply *.lpproj *.json);;Point Cloud Files (*.las *.laz *.ply);;Gaussian PLY (*.ply);;Route JSON Files (*.json);;LiDAR Power Projects (*.lpproj *.json);;All Files (*.*)"));

    if (filePath.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open cancelled."), 2000);
        return;
    }

    const OpenFileKind openFileKind = detectOpenFileKind(filePath);
    switch (openFileKind) {
    case OpenFileKind::PointCloud:
        loadPointCloudFiles(QStringList { filePath });
        return;
    case OpenFileKind::Project:
        loadProjectFile(filePath);
        return;
    case OpenFileKind::Route:
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before importing route files."), 3000);
            return;
        }
        importRouteFile(filePath, true, true);
        return;
    case OpenFileKind::Unknown:
    default:
        break;
    }

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QStringLiteral("json")) {
        bool routeImported = false;
        if (viewer_ != nullptr && viewer_->hasPointCloud()) {
            routeImported = importRouteFile(filePath, true, false);
            if (routeImported) {
                showUserMessage(
                    LogLevel::Info,
                    tr("Imported route file: %1").arg(QFileInfo(filePath).fileName()),
                    3500);
                return;
            }
        }

        if (loadProjectFile(filePath)) {
            return;
        }
    }

    showUserMessage(
        LogLevel::Warning,
        tr("Unsupported file type. Choose LAS/LAZ point cloud, route JSON, or project file."),
        4500);
}

void MainWindow::openProject()
{
    openBackstagePage(backstageOpenProjectPage_);
}

void MainWindow::saveProject()
{
    hideBackstageView();
    if (currentProjectFilePath_.isEmpty()) {
        saveProjectAs();
        return;
    }

    saveProjectFile(currentProjectFilePath_);
}

void MainWindow::saveProjectAs()
{
    hideBackstageView();
    const QString initialPath = currentProjectFilePath_.isEmpty()
        ? QStringLiteral("project.lpproj")
        : currentProjectFilePath_;
    const QString filePath = showStyledSaveFileNameDialog(
        this,
        tr("Save Project"),
        initialPath,
        tr("LiDAR Power Projects (*.lpproj);;JSON Files (*.json)"));

    if (filePath.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Save project cancelled."), 2000);
        return;
    }

    QString normalizedPath = filePath;
    if (QFileInfo(normalizedPath).suffix().isEmpty()) {
        normalizedPath += QStringLiteral(".lpproj");
    }

    saveProjectFile(normalizedPath);
}

void MainWindow::openProjectCoordinateSystems()
{
    hideBackstageView();
    ProjectCoordinateSystemsDialog dialog(this);
    dialog.setCoordinateSystems(projectCoordinateSystems_);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    projectCoordinateSystems_ = dialog.coordinateSystems();
    syncRoutePlanningFromProjectCoordinateSystems();
    updateRoutePlanningPanel();
    rebuildProjectTree();
    updateActionState();
    showUserMessage(LogLevel::Info, tr("Project coordinate systems updated."), 3000);
}

void MainWindow::applyOfficeTheme(Qtitan::RibbonStyle::Theme theme)
{
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        ribbonStyle->setTheme(theme);
        ribbonStyle->setActiveTabAccented(false);
        updateWindowChromePalette(theme);
        persistThemeSettings();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Theme updated."), 2500);
    }
}

void MainWindow::updateWindowChromePalette(Qtitan::RibbonStyle::Theme theme)
{
    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QColor frameColor = useDarkChrome ? kWindowChromeDark : kWindowChromeLight;
    const QColor textColor = useDarkChrome ? QColor(241, 245, 249) : QColor(31, 41, 55);
    const QString dockTitleBackground = useDarkChrome ? QStringLiteral("#1f2937") : QStringLiteral("#e2e8f0");
    const QString dockTitleText = useDarkChrome ? QStringLiteral("#f1f5f9") : QStringLiteral("#0f172a");
    const QString dockBorder = useDarkChrome ? QStringLiteral("#475569") : QStringLiteral("#cbd5e1");
    const QString dockTabBackground = QStringLiteral("#e2e8f0");
    const QString dockTabText = QStringLiteral("#334155");
    const QString dockTabSelectedBackground = QStringLiteral("#ffffff");
    const QString dockTabSelectedText = QStringLiteral("#0f172a");
    const QString dockTabHoverBackground = QStringLiteral("#dbeafe");

    QPalette windowPalette = palette();
    windowPalette.setColor(QPalette::Window, frameColor);
    windowPalette.setColor(QPalette::WindowText, textColor);
    setPalette(windowPalette);
    setBackgroundRole(QPalette::Window);

    if (ribbonBar_ != nullptr) {
        QPalette ribbonPalette = ribbonBar_->palette();
        ribbonPalette.setColor(QPalette::Window, frameColor);
        ribbonPalette.setColor(QPalette::WindowText, textColor);
        ribbonBar_->setPalette(ribbonPalette);
        ribbonBar_->setBackgroundRole(QPalette::Window);
        ribbonBar_->setAutoFillBackground(true);

        const QList<Qtitan::RibbonPage*> pages = ribbonBar_->pages();
        for (Qtitan::RibbonPage* page : pages) {
            if (page == nullptr) {
                continue;
            }

            QPalette pagePalette = page->palette();
            pagePalette.setColor(QPalette::Window, frameColor);
            pagePalette.setColor(QPalette::WindowText, textColor);
            page->setPalette(pagePalette);
            page->setBackgroundRole(QPalette::Window);
            page->setAutoFillBackground(true);
            page->update();
        }

        const QString normalText = useDarkChrome ? QStringLiteral("#e2e8f0") : QStringLiteral("#0f172a");
        const QString hoverBg = useDarkChrome ? QStringLiteral("rgba(148, 163, 184, 0.22)") : QStringLiteral("rgba(37, 99, 235, 0.16)");
        const QString hoverText = useDarkChrome ? QStringLiteral("#f8fafc") : QStringLiteral("#0b1220");
        const QString checkedBg = useDarkChrome ? QStringLiteral("#2563eb") : QStringLiteral("#1d4ed8");
        const QString disabledText = useDarkChrome ? QStringLiteral("#94a3b8") : QStringLiteral("#475569");
        const QString ribbonTabBackground = useDarkChrome ? QStringLiteral("#334155") : QStringLiteral("#e2e8f0");
        const QString ribbonTabText = useDarkChrome ? QStringLiteral("#f1f5f9") : QStringLiteral("#1e293b");
        const QString ribbonTabBorder = useDarkChrome ? QStringLiteral("#64748b") : QStringLiteral("#cbd5e1");
        const QString ribbonTabSelectedBackground = QStringLiteral("#ffffff");
        const QString ribbonTabSelectedText = QStringLiteral("#0f172a");
        ribbonBar_->setStyleSheet(QStringLiteral(
            "QAbstractButton {"
            "background-color: transparent;"
            "border: none;"
            "color: %1;"
            "}"
            "QAbstractButton:hover {"
            "background-color: %2;"
            "color: %3;"
            "}"
            "QAbstractButton:checked, QAbstractButton:pressed {"
            "background-color: %4;"
            "color: #ffffff;"
            "}"
            "QAbstractButton:disabled {"
            "background-color: transparent;"
            "color: %5;"
            "}"
            "QTabBar::tab {"
            "background-color: %6;"
            "color: %7;"
            "border: 1px solid %8;"
            "border-bottom: none;"
            "border-top-left-radius: 6px;"
            "border-top-right-radius: 6px;"
            "padding: 6px 12px;"
            "margin-right: 2px;"
            "font-weight: 600;"
            "}"
            "QTabBar::tab:selected {"
            "background-color: %9;"
            "color: %10;"
            "}"
            "QTabBar::tab:hover:!selected {"
            "background-color: %2;"
            "color: %3;"
            "}")
                .arg(normalText)
                .arg(hoverBg)
                .arg(hoverText)
                .arg(checkedBg)
                .arg(disabledText)
                .arg(ribbonTabBackground)
                .arg(ribbonTabText)
                .arg(ribbonTabBorder)
                .arg(ribbonTabSelectedBackground)
                .arg(ribbonTabSelectedText));

        ribbonBar_->update();
    }

    setStyleSheet(QStringLiteral(
        "QDockWidget {"
        "border: 1px solid %1;"
        "}"
        "QMainWindow::tab-bar {"
        "alignment: left;"
        "}"
        "QMainWindow QTabBar::tab {"
        "background-color: %4;"
        "color: %5;"
        "border: 1px solid %1;"
        "border-bottom: none;"
        "border-top-left-radius: 6px;"
        "border-top-right-radius: 6px;"
        "padding: 6px 12px;"
        "margin-right: 2px;"
        "font-weight: 600;"
        "}"
        "QMainWindow QTabBar::tab:selected {"
        "background-color: %6;"
        "color: %7;"
        "}"
        "QMainWindow QTabBar::tab:hover:!selected {"
        "background-color: %8;"
        "color: %7;"
        "}"
        "QDockWidget::title {"
        "background-color: %2;"
        "color: %3;"
        "text-align: left;"
        "padding: 6px 10px;"
        "border-bottom: 1px solid %1;"
        "font-weight: 600;"
        "}"
        "QToolTip {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "border: 1px solid #94a3b8;"
        "padding: 4px 8px;"
        "border-radius: 4px;"
        "}")
            .arg(dockBorder, dockTitleBackground, dockTitleText,
                dockTabBackground, dockTabText, dockTabSelectedBackground,
                dockTabSelectedText, dockTabHoverBackground));

    applyLightToolTipStyle();

    updateWindowControlAppearance(theme);
    updateWindowControlButtons();
    update();
}

void MainWindow::applyLightToolTipStyle()
{
    const QColor toolTipBackground(248, 250, 252);
    const QColor toolTipText(15, 23, 42);

    QPalette toolTipPalette = QToolTip::palette();
    toolTipPalette.setColor(QPalette::ToolTipBase, toolTipBackground);
    toolTipPalette.setColor(QPalette::ToolTipText, toolTipText);
    QToolTip::setPalette(toolTipPalette);

    Qtitan::RibbonToolTip* ribbonToolTip = Qtitan::RibbonToolTip::instance();
    if (ribbonToolTip == nullptr) {
        return;
    }

    QPalette ribbonToolTipPalette = ribbonToolTip->palette();
    ribbonToolTipPalette.setColor(QPalette::Window, toolTipBackground);
    ribbonToolTipPalette.setColor(QPalette::WindowText, toolTipText);
    ribbonToolTipPalette.setColor(QPalette::Base, toolTipBackground);
    ribbonToolTipPalette.setColor(QPalette::Text, toolTipText);
    ribbonToolTipPalette.setColor(QPalette::ToolTipBase, toolTipBackground);
    ribbonToolTipPalette.setColor(QPalette::ToolTipText, toolTipText);
    ribbonToolTip->setPalette(ribbonToolTipPalette);
    ribbonToolTip->setAutoFillBackground(true);
    ribbonToolTip->setStyleSheet(QStringLiteral(
        "Qtitan--RibbonToolTip, RibbonToolTip, QFrame {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "border: 1px solid #94a3b8;"
        "}"
        "QLabel {"
        "color: #0f172a;"
        "background: transparent;"
        "}"
        "QToolButton {"
        "color: #0f172a;"
        "background: transparent;"
        "}"));
}

void MainWindow::syncUiFromViewer()
{
    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    const InteractionOptions& interactionOptions = viewer_->interactionOptions();

    {
        const QSignalBlocker pointSizeBlocker(pointSizeSlider_);
        const QSignalBlocker pointOpacityBlocker(pointOpacitySlider_);
        const QSignalBlocker depthCueBlocker(depthCueSlider_);
        const QSignalBlocker edlStrengthBlocker(edlStrengthSlider_);
        const QSignalBlocker colorModeBlocker(colorModeComboBox_);
        const QSignalBlocker roundSplatsBlocker(roundSplatsCheckBox_);
        const QSignalBlocker axesBlocker(axesCheckBox_);
        const QSignalBlocker boundsBlocker(boundingBoxCheckBox_);
        const QSignalBlocker orbitBlocker(invertOrbitCheckBox_);
        const QSignalBlocker panBlocker(invertPanCheckBox_);
        const QSignalBlocker wheelBlocker(invertWheelCheckBox_);
        const QSignalBlocker wheelSensitivityBlocker(wheelZoomSensitivitySlider_);
        const QSignalBlocker axesActionBlocker(showAxesAction_);
        const QSignalBlocker boundsActionBlocker(showBoundingBoxAction_);
        const QSignalBlocker rgbActionBlocker(rgbColorAction_);
        const QSignalBlocker elevationActionBlocker(elevationColorAction_);
        const QSignalBlocker singleActionBlocker(singleColorAction_);
        const QSignalBlocker classificationActionBlocker(classificationColorAction_);
        const QSignalBlocker colorfulThemeBlocker(themeColorfulAction_);
        const QSignalBlocker whiteThemeBlocker(themeWhiteAction_);
        const QSignalBlocker darkThemeBlocker(themeDarkGrayAction_);
        const QSignalBlocker measurementActionBlocker(measureAction_);
        const QSignalBlocker englishLanguageBlocker(languageEnglishAction_);
        const QSignalBlocker chineseLanguageBlocker(languageChineseAction_);
        const QSignalBlocker clipModeNoneBlocker(clipModeNoneAction_);
        const QSignalBlocker clipModeBoxBlocker(clipModeBoxAction_);
        const QSignalBlocker clipModePolygonBlocker(clipModePolygonAction_);
        const QSignalBlocker clipBoxWorldBlocker(clipBoxWorldAlignedAction_);
        const QSignalBlocker clipBoxViewBlocker(clipBoxViewAlignedAction_);
        const QSignalBlocker clipScopeActiveBlocker(clipScopeActiveDatasetAction_);
        const QSignalBlocker clipScopeVisibleBlocker(clipScopeVisibleDatasetsAction_);
        const QSignalBlocker clipKeepInsideBlocker(clipToggleInsideAction_);
        const QSignalBlocker clearanceRulePresetBlocker(clearanceRulePresetComboBox_);
        const QSignalBlocker vegetationSearchRadiusBlocker(vegetationSearchRadiusSpinBox_);
        const QSignalBlocker vegetationClusterGapBlocker(vegetationClusterGapSpinBox_);
        const QSignalBlocker vegetationClusterPointCountBlocker(vegetationClusterPointCountSpinBox_);
        const QSignalBlocker preferVegetationClassificationBlocker(preferVegetationClassificationCheckBox_);
        const QSignalBlocker aircraftProfileBlocker(aircraftProfileComboBox_);
        const QSignalBlocker routeSafetyHeightBlocker(routeSafetyHeightSpinBox_);
        const QSignalBlocker routeWaypointSpeedBlocker(routeWaypointSpeedSpinBox_);
        const QSignalBlocker routeWaypointSpacingBlocker(routeWaypointSpacingSpinBox_);
        const QSignalBlocker routeSmoothingStrengthBlocker(routeSmoothingStrengthSpinBox_);
        const QSignalBlocker routeHeightOffsetBlocker(routeHeightOffsetSpinBox_);

        pointSizeSlider_->setValue(static_cast<int>(options.pointSize));
        pointOpacitySlider_->setValue(static_cast<int>(std::lround(options.pointOpacity * 100.0f)));
        depthCueSlider_->setValue(static_cast<int>(std::lround(options.depthCueStrength * 100.0f)));
        edlStrengthSlider_->setValue(static_cast<int>(std::lround(options.edlStrength * 100.0f)));
        colorModeComboBox_->setCurrentIndex(static_cast<int>(options.colorMode));
        roundSplatsCheckBox_->setChecked(options.useRoundSplats);
        axesCheckBox_->setChecked(options.showAxes);
        boundingBoxCheckBox_->setChecked(options.showBoundingBox);
        invertOrbitCheckBox_->setChecked(interactionOptions.invertOrbitDrag);
        invertPanCheckBox_->setChecked(interactionOptions.invertPanDrag);
        invertWheelCheckBox_->setChecked(interactionOptions.invertWheelZoom);
        wheelZoomSensitivitySlider_->setValue(interactionOptions.wheelZoomSensitivityPercent);

        showAxesAction_->setChecked(options.showAxes);
        showBoundingBoxAction_->setChecked(options.showBoundingBox);
        rgbColorAction_->setChecked(options.colorMode == PointCloudColorMode::Rgb);
        elevationColorAction_->setChecked(options.colorMode == PointCloudColorMode::Elevation);
        singleColorAction_->setChecked(options.colorMode == PointCloudColorMode::SingleColor);
        classificationColorAction_->setChecked(options.colorMode == PointCloudColorMode::Classification);
        measureAction_->setChecked(viewer_->measurementEnabled());
        languageEnglishAction_->setChecked(currentLanguage_ == UiLanguage::English);
        languageChineseAction_->setChecked(currentLanguage_ == UiLanguage::Chinese);
        clipModeNoneAction_->setChecked(viewer_->clipMode() == ClipRegion::None);
        clipModeBoxAction_->setChecked(viewer_->clipMode() == ClipRegion::Box);
        clipModePolygonAction_->setChecked(viewer_->clipMode() == ClipRegion::ScreenPolygonPrism);
        clipBoxWorldAlignedAction_->setChecked(viewer_->clipBoxAlignment() == ClipRegion::WorldAligned);
        clipBoxViewAlignedAction_->setChecked(viewer_->clipBoxAlignment() == ClipRegion::ViewAligned);
        clipScopeActiveDatasetAction_->setChecked(viewer_->clipScope() == ClipRegion::ActiveDataset);
        clipScopeVisibleDatasetsAction_->setChecked(viewer_->clipScope() == ClipRegion::VisibleDatasets);
        clipToggleInsideAction_->setChecked(viewer_->clipKeepInside());
        if (clearanceRulePresetComboBox_ != nullptr) {
            const int presetIndex = clearanceRulePresetComboBox_->findData(static_cast<int>(clearanceRulePreset_));
            clearanceRulePresetComboBox_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 0);
        }
        if (vegetationSearchRadiusSpinBox_ != nullptr) {
            vegetationSearchRadiusSpinBox_->setValue(vegetationSearchRadiusMeters_);
        }
        if (vegetationClusterGapSpinBox_ != nullptr) {
            vegetationClusterGapSpinBox_->setValue(vegetationClusterGapMeters_);
        }
        if (vegetationClusterPointCountSpinBox_ != nullptr) {
            vegetationClusterPointCountSpinBox_->setValue(vegetationClusterPointCount_);
        }
        if (preferVegetationClassificationCheckBox_ != nullptr) {
            preferVegetationClassificationCheckBox_->setChecked(preferVegetationClassification_);
        }
        if (aircraftProfileComboBox_ != nullptr) {
            const int profileIndex = aircraftProfileComboBox_->findData(static_cast<int>(routePlanningOptions_.aircraftProfile));
            aircraftProfileComboBox_->setCurrentIndex(profileIndex >= 0 ? profileIndex : 0);
        }
        if (routeSafetyHeightSpinBox_ != nullptr) {
            routeSafetyHeightSpinBox_->setValue(routePlanningOptions_.safety.safetyHeightMeters);
        }
        if (routeWaypointSpeedSpinBox_ != nullptr) {
            routeWaypointSpeedSpinBox_->setValue(routePlanningOptions_.safety.defaultWaypointSpeedMps);
        }
        if (routeWaypointSpacingSpinBox_ != nullptr) {
            routeWaypointSpacingSpinBox_->setValue(routePlanningOptions_.generation.waypointSpacingMeters);
        }
        if (routeSmoothingStrengthSpinBox_ != nullptr) {
            routeSmoothingStrengthSpinBox_->setValue(routePlanningOptions_.generation.smoothingStrengthPercent);
        }
        if (routeHeightOffsetSpinBox_ != nullptr) {
            routeHeightOffsetSpinBox_->setValue(routePlanningOptions_.safety.heightOffsetMeters);
        }

        if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
            themeColorfulAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016Colorful);
            themeWhiteAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016White);
            themeDarkGrayAction_->setChecked(ribbonStyle->getTheme() == Qtitan::RibbonStyle::Office2016DarkGray);
        }
    }

    updateSliderValueLabel(pointSizeSlider_, pointSizeValueLabel_, tr("%1 px"));
    updateSliderValueLabel(pointOpacitySlider_, pointOpacityValueLabel_, tr("%1%"));
    updateSliderValueLabel(depthCueSlider_, depthCueValueLabel_, tr("%1%"));
    updateSliderValueLabel(edlStrengthSlider_, edlStrengthValueLabel_, tr("%1%"));
    updateSliderValueLabel(wheelZoomSensitivitySlider_, wheelZoomSensitivityValueLabel_, tr("%1%"));

    setColorButtonAppearance(pointColorButton_, options.singleColor, tr("Pick Color"));
    setColorButtonAppearance(backgroundColorButton_, options.backgroundColor, tr("Pick Background"));
    setColorButtonAppearance(routeWaypointColorButton_, viewer_->inspectionRouteWaypointColor(), tr("Waypoint Color"));
    setColorButtonAppearance(routePartPointColorButton_, viewer_->inspectionRoutePartPointColor(), tr("Part Point Color"));
    setColorButtonAppearance(routeTrajectoryColorButton_, viewer_->inspectionRouteTrajectoryColor(), tr("Trajectory Color"));
    clipToggleInsideAction_->setText(viewer_->clipKeepInside() ? tr("Keep Inside") : tr("Keep Outside"));
    clipToggleInsideAction_->setToolTip(
        viewer_->clipKeepInside()
            ? tr("Keep points inside the current clip region.")
            : tr("Keep points outside the current clip region."));
    updateClassificationColorTable();
    updateNavigationHelpText();
    updateDatasetPanel();
    updateMeasurementPanel();
    updateTowerPanel();
    updateIssuePanel();
    updateVegetationRiskPanel();
    updateRoutePlanningPanel();
    if (routeRoamSpeedSpinBox_ != nullptr) {
        const QSignalBlocker blocker(routeRoamSpeedSpinBox_);
        routeRoamSpeedSpinBox_->setValue(viewer_->inspectionRouteRoamSpeedMetersPerSecond());
    }
    if (routeRoamViewModeComboBox_ != nullptr) {
        const QSignalBlocker blocker(routeRoamViewModeComboBox_);
        const int roamModeIndex = routeRoamViewModeComboBox_->findData(static_cast<int>(viewer_->inspectionRouteRoamViewMode()));
        routeRoamViewModeComboBox_->setCurrentIndex(roamModeIndex >= 0 ? roamModeIndex : 0);
    }
    updateActionState();
}

void MainWindow::updateActionState()
{
    const bool hasPointCloud = viewer_->hasPointCloud();
    const bool hasRenderableScene = viewer_ != nullptr && viewer_->hasRenderableScene();
    const bool profileClassificationReady = hasPointCloud;
    const bool hasTowerMarkers = viewer_ != nullptr && !viewer_->towerMarkers().isEmpty();
    const bool hasTowerSelection = viewer_ != nullptr && viewer_->selectedTowerIndex() >= 0;
    const bool towerToolActive = viewer_ != nullptr && viewer_->towerEditMode() != TowerEditMode::None;
    const bool hasLinkedTowerFile = !linkedTowerFilePath_.trimmed().isEmpty();
    const bool hasLinkedRouteFile = !linkedRouteFilePath_.trimmed().isEmpty();
    const bool hasIssues = viewer_ != nullptr && !viewer_->inspectionIssues().isEmpty();
    const bool hasIssueSelection = viewer_ != nullptr && viewer_->selectedIssueIndex() >= 0;
    const bool issueToolActive = viewer_ != nullptr && viewer_->issueEditMode() != IssueEditMode::None;
    const bool hasVegetationRisks = !vegetationRiskResults_.isEmpty();
    const bool hasVegetationRiskSelection = selectedVegetationRiskIndex_ >= 0 && selectedVegetationRiskIndex_ < vegetationRiskResults_.size();
    const bool hasInspectionRoute = !currentPowerlineRoute_.waypoints.isEmpty();
    const bool hasRouteWaypointSelection = selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size();
    const bool routeRoamActive = viewer_ != nullptr && viewer_->inspectionRouteRoamActive();
    const bool routeRoamPaused = viewer_ != nullptr && viewer_->inspectionRouteRoamPaused();
    const bool routeRoamReady = hasInspectionRoute
        && hasPointCloud
        && viewer_ != nullptr
        && viewer_->inspectionRouteVisible();
    const bool hasMeasuredCorridor = viewer_ != nullptr && viewer_->measurementResult().isComplete();
    const QTreeWidgetItem* currentProjectItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
    const bool hasDatasetSelection = !selectedDatasetPath().isEmpty();
    const bool hasPathSelection = !projectTreeItemFilePath(currentProjectItem).isEmpty();
    openProjectAction_->setEnabled(true);
    saveProjectAction_->setEnabled(viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty());
    saveProjectAsAction_->setEnabled(viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty());
    projectCoordinateSystemsAction_->setEnabled(viewer_ != nullptr);
    addPointCloudAction_->setEnabled(true);
    removeDatasetAction_->setEnabled(hasDatasetSelection);
    locateDatasetAction_->setEnabled(hasPathSelection);
    copyDatasetPathAction_->setEnabled(hasPathSelection);
    expandProjectTreeAction_->setEnabled(projectTreeWidget_ != nullptr && projectTreeWidget_->topLevelItemCount() > 0);
    collapseProjectTreeAction_->setEnabled(projectTreeWidget_ != nullptr && projectTreeWidget_->topLevelItemCount() > 0);
    clearAction_->setEnabled(
        viewer_ != nullptr
        && !viewer_->isPointCloudLoadingInProgress()
        && !viewer_->currentFilePaths().isEmpty());
    fitSceneAction_->setEnabled(hasRenderableScene);
    topViewAction_->setEnabled(hasRenderableScene);
    frontViewAction_->setEnabled(hasRenderableScene);
    rightViewAction_->setEnabled(hasRenderableScene);
    captureScreenshotAction_->setEnabled(true);
    toggleScreenRecordingAction_->setEnabled(true);
    const bool recordingActive =
        (recordingProcess_ != nullptr && recordingProcess_->state() != QProcess::NotRunning)
        || (screenRecorder_ != nullptr && screenRecorder_->isRecording());
    toggleScreenRecordingAction_->setText(recordingActive ? tr("Stop Recording") : tr("Start Recording"));
    toggleScreenRecordingAction_->setToolTip(
        recordingActive
            ? tr("Stop the active MP4 screen recording")
            : tr("Start MP4 screen recording for the current application window"));
    measureAction_->setEnabled(hasPointCloud);
    profileClassificationAction_->setEnabled(profileClassificationReady);
    showProfileClassificationDockAction_->setEnabled(true);
    {
        const QSignalBlocker blocker(showProfileClassificationDockAction_);
        showProfileClassificationDockAction_->setChecked(
            profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible());
    }
    saveProfileClassificationEditsAction_->setEnabled(hasPointCloud && viewer_->classificationEditedPointCount() > 0);
    clearMeasurementAction_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    analyzeVegetationRisksAction_->setEnabled(hasPointCloud && hasMeasuredCorridor);
    exportClearanceCsvAction_->setEnabled(hasPointCloud && viewer_->measurementResult().isComplete());
    showProfileDockAction_->setEnabled(true);
    {
        const QSignalBlocker blocker(showProfileDockAction_);
        showProfileDockAction_->setChecked(profileDock_ != nullptr && profileDock_->isVisible());
    }
    measurementToggleButton_->setEnabled(hasPointCloud);
    measurementClearButton_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    clearanceThresholdSpinBox_->setEnabled(hasPointCloud);
    if (classificationColorsGroupBox_ != nullptr) {
        classificationColorsGroupBox_->setEnabled(hasPointCloud);
    }
    if (resetClassificationColorsButton_ != nullptr) {
        resetClassificationColorsButton_->setEnabled(hasPointCloud);
    }
    if (clearanceSegmentsTableWidget_ != nullptr) {
        clearanceSegmentsTableWidget_->setEnabled(hasPointCloud && viewer_->measurementResult().isComplete());
    }
    startTowerEditAction_->setEnabled(hasPointCloud && !towerEditingEnabled_);
    finishTowerEditAction_->setEnabled(towerEditingEnabled_);
    addTowerAction_->setEnabled(hasPointCloud);
    insertTowerAction_->setEnabled(hasPointCloud && hasTowerSelection);
    moveTowerAction_->setEnabled(hasPointCloud && hasTowerSelection);
    editCurrentTowerAction_->setEnabled(hasPointCloud && hasTowerSelection);
    focusTowerAction_->setEnabled(hasTowerSelection);
    removeTowerAction_->setEnabled(hasTowerSelection);
    clearTowersAction_->setEnabled(hasTowerMarkers && !towerToolActive);
    cancelTowerToolAction_->setEnabled(towerToolActive);
    importTowerFileAction_->setEnabled(hasPointCloud);
    saveTowerFileAction_->setEnabled(hasTowerMarkers);
    saveTowerFileAsAction_->setEnabled(hasTowerMarkers);
    reloadTowerFileAction_->setEnabled(hasLinkedTowerFile && hasPointCloud);
    startIssueMarkAction_->setEnabled(hasPointCloud && !issueToolActive);
    cancelIssueToolAction_->setEnabled(issueToolActive);
    focusIssueAction_->setEnabled(hasIssueSelection);
    removeIssueAction_->setEnabled(hasIssueSelection);
    clearIssuesAction_->setEnabled(hasIssues);
    clipModeNoneAction_->setEnabled(hasPointCloud);
    clipModeBoxAction_->setEnabled(hasPointCloud);
    clipModePolygonAction_->setEnabled(hasPointCloud);
    clipBoxWorldAlignedAction_->setEnabled(hasPointCloud);
    clipBoxViewAlignedAction_->setEnabled(hasPointCloud);
    clipScopeActiveDatasetAction_->setEnabled(hasPointCloud);
    clipScopeVisibleDatasetsAction_->setEnabled(hasPointCloud);
    clipToggleInsideAction_->setEnabled(hasPointCloud);
    clipApplyExportAction_->setEnabled(hasPointCloud && viewer_ != nullptr && viewer_->hasActiveClipRegion());
    exportIssuesCsvAction_->setEnabled(hasIssues);
    exportInspectionReportAction_->setEnabled(hasPointCloud && (hasTowerMarkers || hasIssues));
    focusVegetationRiskAction_->setEnabled(hasVegetationRiskSelection);
    createIssueFromRiskAction_->setEnabled(hasVegetationRiskSelection);
    createIssuesFromRisksAction_->setEnabled(hasVegetationRisks);
    clearVegetationRisksAction_->setEnabled(hasVegetationRisks);
    generateInspectionRouteAction_->setEnabled(hasVegetationRisks && hasPointCloud);
    regenerateInspectionRouteAction_->setEnabled(hasVegetationRisks && hasPointCloud);
    clearInspectionRouteAction_->setEnabled(hasInspectionRoute);
    toggleRouteEditingAction_->setEnabled(hasInspectionRoute);
    startInspectionRouteRoamAction_->setEnabled(routeRoamReady && !routeRoamActive);
    pauseInspectionRouteRoamAction_->setEnabled(routeRoamReady && routeRoamActive);
    pauseInspectionRouteRoamAction_->setText(routeRoamPaused ? tr("Resume Roam") : tr("Pause Roam"));
    stopInspectionRouteRoamAction_->setEnabled(routeRoamReady && routeRoamActive);
    focusRouteWaypointAction_->setEnabled(hasRouteWaypointSelection);
    importRouteFileAction_->setEnabled(hasPointCloud);
    saveRouteFileAction_->setEnabled(hasInspectionRoute);
    saveRouteFileAsAction_->setEnabled(hasInspectionRoute);
    reloadRouteFileAction_->setEnabled(hasLinkedRouteFile && hasPointCloud);
    importRouteKmlAction_->setEnabled(hasPointCloud);
    exportRouteKmlAction_->setEnabled(hasInspectionRoute);
    exportRouteDjiKmzAction_->setEnabled(hasInspectionRoute && currentPowerlineRoute_.waypoints.size() >= 2);

    if (routeRoamStartButton_ != nullptr) {
        routeRoamStartButton_->setEnabled(routeRoamReady && !routeRoamActive);
    }
    if (routeRoamPauseResumeButton_ != nullptr) {
        routeRoamPauseResumeButton_->setEnabled(routeRoamReady && routeRoamActive);
        routeRoamPauseResumeButton_->setText(routeRoamPaused ? tr("Resume Roam") : tr("Pause Roam"));
    }
    if (routeRoamStopButton_ != nullptr) {
        routeRoamStopButton_->setEnabled(routeRoamReady && routeRoamActive);
    }
    syncRouteRoamFloatingDialog();
    refreshBackstageProjectPropertiesPage();
    refreshBackstageApplicationSettingsPage();
}

void MainWindow::setColorButtonAppearance(QPushButton* button, const QColor& color, const QString& fallbackText) const
{
    if (button == nullptr) {
        return;
    }

    const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    const QString foreground = luminance >= 150 ? QStringLiteral("#111827") : QStringLiteral("#f8fafc");

    button->setText(fallbackText);
    button->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "background-color: %1;"
        "color: %2;"
        "border: 1px solid rgba(15, 23, 42, 0.14);"
        "border-radius: 4px;"
        "padding: 6px 10px;"
        "}").arg(color.name(), foreground));
}

void MainWindow::appendLog(LogLevel level, const QString& message)
{
    const QString normalizedMessage = message.trimmed();
    if (normalizedMessage.isEmpty()) {
        return;
    }

    lasviewer::logging::LogLevel loggerLevel = lasviewer::logging::LogLevel::Info;
    switch (level) {
    case LogLevel::Warning:
        loggerLevel = lasviewer::logging::LogLevel::Warning;
        break;
    case LogLevel::Error:
        loggerLevel = lasviewer::logging::LogLevel::Error;
        break;
    case LogLevel::Info:
    default:
        loggerLevel = lasviewer::logging::LogLevel::Info;
        break;
    }

    lasviewer::logging::ApplicationLogger::instance().log(
        loggerLevel,
        QStringLiteral("UI"),
        normalizedMessage);
}

void MainWindow::refreshLogPanel()
{
    if (logDock_ != nullptr) {
        logDock_->refreshEntries();
    }
}

void MainWindow::exportLogEntries()
{
    const QString defaultPath = QDir::home().filePath(
        QStringLiteral("lasviewer-log-%1.csv")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));

    const QString filePath = showStyledSaveFileNameDialog(
        this,
        tr("Export Application Log"),
        defaultPath,
        tr("CSV Files (*.csv);;Text Files (*.txt)"));
    if (filePath.isEmpty()) {
        return;
    }

    const bool asCsv = QFileInfo(filePath).suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0;
    QString errorMessage;
    if (!lasviewer::logging::ApplicationLogger::instance().exportEntries(filePath, asCsv, &errorMessage)) {
        showUserMessage(LogLevel::Error, tr("Failed to export log: %1").arg(errorMessage), 4500);
        return;
    }

    showUserMessage(
        LogLevel::Info,
        tr("Log exported to %1").arg(QDir::toNativeSeparators(filePath)),
        3500);
}

void MainWindow::showUserMessage(LogLevel level, const QString& message, int timeoutMs)
{
    if (statusBar() != nullptr) {
        statusBar()->showMessage(message, timeoutMs);
    }
    appendLog(level, message);
}



void MainWindow::beginOperationProgress(const QString& message)
{
    if (globalProgressBar_ != nullptr) {
        globalProgressBar_->setVisible(true);
        globalProgressBar_->setRange(0, 0);
    }
    if (statusBar() != nullptr && !message.trimmed().isEmpty()) {
        statusBar()->showMessage(message);
    }
}

void MainWindow::updateOperationProgress(const QString& message, int value, int maximum)
{
    if (globalProgressBar_ != nullptr) {
        if (!globalProgressBar_->isVisible()) {
            globalProgressBar_->setVisible(true);
        }
        if (maximum <= 0) {
            globalProgressBar_->setRange(0, 0);
        } else {
            globalProgressBar_->setRange(0, maximum);
            globalProgressBar_->setValue(std::clamp(value, 0, maximum));
        }
    }

    if (statusBar() != nullptr && !message.trimmed().isEmpty()) {
        statusBar()->showMessage(message);
    }
}

void MainWindow::endOperationProgress()
{
    if (globalProgressBar_ != nullptr) {
        globalProgressBar_->hide();
        globalProgressBar_->setRange(0, 1000);
        globalProgressBar_->setValue(0);
    }
}

void MainWindow::updateNavigationHelpText()
{
    if (navigationTipsLabel_ == nullptr || viewer_ == nullptr) {
        return;
    }

    const InteractionOptions& options = viewer_->interactionOptions();
    navigationTipsLabel_->setText(tr(
        "Left drag orbits (%1), middle or right drag pans (%2), and the mouse wheel zooms (%3). "
        "Current zoom sensitivity is %4. Use the controls below to match your preferred interaction feel.")
        .arg(options.invertOrbitDrag ? tr("inverted") : tr("normal"))
        .arg(options.invertPanDrag ? tr("inverted") : tr("normal"))
        .arg(options.invertWheelZoom ? tr("inverted") : tr("normal"))
        .arg(tr("%1%").arg(QLocale().toString(options.wheelZoomSensitivityPercent))));
}

void MainWindow::showPointCloudDatasetDetails(const QString& filePath)
{
    if (viewer_ == nullptr || filePath.trimmed().isEmpty()) {
        return;
    }

    for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
        if (datasetInfo.filePath.compare(filePath, Qt::CaseInsensitive) != 0) {
            continue;
        }

        const QFileInfo fileInfo(datasetInfo.filePath);
        const QString extentText = formatTriplet(
            datasetInfo.maxBounds.x - datasetInfo.minBounds.x,
            datasetInfo.maxBounds.y - datasetInfo.minBounds.y,
            datasetInfo.maxBounds.z - datasetInfo.minBounds.z);
        const QString subtitle = tr("Review the active dataset metadata, spatial bounds, and attribute availability before further analysis.");
        const QList<QPair<QString, QString>> statRows = {
            qMakePair(tr("Point Count"), QLocale().toString(static_cast<qlonglong>(datasetInfo.pointCount))),
            qMakePair(tr("Extent"), extentText),
            qMakePair(tr("Projection"), datasetInfo.projectionText.trimmed().isEmpty() ? tr("Unknown") : datasetInfo.projectionText),
            qMakePair(tr("Native RGB"), datasetInfo.hasColor ? tr("Available") : tr("Unavailable"))
        };
        const QList<QPair<QString, QString>> detailRows = {
            qMakePair(tr("Dataset Name"), fileInfo.fileName().isEmpty() ? datasetInfo.filePath : fileInfo.fileName()),
            qMakePair(tr("Full Path"), QDir::toNativeSeparators(datasetInfo.filePath)),
            qMakePair(tr("File Size"), fileInfo.exists() ? QLocale().formattedDataSize(fileInfo.size()) : tr("N/A")),
            qMakePair(tr("Visibility"), datasetInfo.visible ? tr("Visible") : tr("Hidden")),
            qMakePair(tr("Min Bounds"), formatTriplet(datasetInfo.minBounds.x, datasetInfo.minBounds.y, datasetInfo.minBounds.z)),
            qMakePair(tr("Max Bounds"), formatTriplet(datasetInfo.maxBounds.x, datasetInfo.maxBounds.y, datasetInfo.maxBounds.z)),
            qMakePair(tr("Extent"), extentText),
            qMakePair(tr("Projection Text"), datasetInfo.projectionText.trimmed().isEmpty() ? tr("Unknown") : datasetInfo.projectionText),
            qMakePair(tr("RGB Attribute"), datasetInfo.hasColor ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("Intensity"), datasetInfo.hasIntensity ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("Classification"), datasetInfo.hasClassification ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("Return Info"), datasetInfo.hasReturnInfo ? tr("Available") : tr("Unavailable")),
            qMakePair(tr("GPS Time"), datasetInfo.hasGpsTime ? tr("Available") : tr("Unavailable"))
        };
        showStyledDetailsDialog(this, tr("Point Cloud Details"), subtitle, detailRows, statRows);
        return;
    }
}

void MainWindow::showInspectionRouteDetails() const
{
    const DataTrajectoryItem& trajectoryItem = DataManager::instance().trajectoryItem();
    const QList<PointRecord>& routePoints = trajectoryItem.points;

    PointRecord minBounds;
    PointRecord maxBounds;
    const bool hasBounds = boundsFromPoints(routePoints, &minBounds, &maxBounds);
    const QString routeName = trajectoryItem.name.trimmed().isEmpty()
        ? tr("Inspection Route")
        : trajectoryItem.name.trimmed();
    const QString extentText = hasBounds
        ? formatTriplet(maxBounds.x - minBounds.x, maxBounds.y - minBounds.y, maxBounds.z - minBounds.z)
        : tr("N/A");
    const QList<QPair<QString, QString>> statRows = {
        qMakePair(tr("Waypoints"), QLocale().toString(routePoints.size())),
        qMakePair(tr("Visibility"), trajectoryItem.visible ? tr("Visible") : tr("Hidden")),
        qMakePair(tr("Extent"), extentText)
    };
    const QList<QPair<QString, QString>> detailRows = {
        qMakePair(tr("Route Name"), routeName),
        qMakePair(tr("Waypoints"), QLocale().toString(routePoints.size())),
        qMakePair(tr("Min Bounds"), hasBounds ? formatTriplet(minBounds.x, minBounds.y, minBounds.z) : tr("N/A")),
        qMakePair(tr("Max Bounds"), hasBounds ? formatTriplet(maxBounds.x, maxBounds.y, maxBounds.z) : tr("N/A")),
        qMakePair(tr("Extent"), extentText)
    };
    showStyledDetailsDialog(
        const_cast<MainWindow*>(this),
        tr("Trajectory Details"),
        tr("Inspect route bounds, waypoint count, and visibility before exporting or editing."),
        detailRows,
        statRows);
}

void MainWindow::applyLanguage(UiLanguage language)
{
    currentLanguage_ = language;

    if (appTranslator_ != nullptr) {
        qApp->removeTranslator(appTranslator_);
        if (language == UiLanguage::Chinese) {
            const QString translationDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
            appTranslator_->load(QStringLiteral("lasviewer_zh_CN"), translationDir);
            qApp->installTranslator(appTranslator_);
        }
    }

    if (qtTranslator_ != nullptr) {
        qApp->removeTranslator(qtTranslator_);
        if (language == UiLanguage::Chinese) {
            const QString deployedTranslationDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
            const QString qtTranslationName = QStringLiteral("qt_zh_CN");
            if (qtTranslator_->load(qtTranslationName, deployedTranslationDir)
                || qtTranslator_->load(qtTranslationName, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
                qApp->installTranslator(qtTranslator_);
            }
        }
    }

    persistLanguageSettings();
    retranslateUi();
}



