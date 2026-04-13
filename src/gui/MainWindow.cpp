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

namespace
{
const QColor kWindowChromeLight(243, 246, 251);
const QColor kWindowChromeDark(51, 65, 85);
constexpr int kMainWindowStateVersion = 1;
constexpr int kWindowResizeBorder = 8;
constexpr int kProjectTreeItemTypeRole = Qt::UserRole;
constexpr int kProjectTreeFilePathRole = Qt::UserRole + 1;
constexpr int kProjectTreeIssueIndexRole = Qt::UserRole + 2;
constexpr int kProjectTreeRouteIndexRole = Qt::UserRole + 3;
constexpr int kRouteWaypointColumnPart = 1;
constexpr int kRouteWaypointColumnX = 2;
constexpr int kRouteWaypointColumnY = 3;
constexpr int kRouteWaypointColumnZ = 4;
constexpr int kRouteWaypointColumnAircraftYaw = 5;
constexpr int kRouteWaypointColumnGimbalPitch = 6;
constexpr int kRouteWaypointColumnCameraYaw = 7;
constexpr int kRouteWaypointColumnCameraPitch = 8;
constexpr int kRouteWaypointTargetColumnPart = 1;
constexpr int kRouteWaypointTargetColumnFocalRatio = 2;
constexpr int kRouteWaypointTargetColumnCameraYaw = 3;
constexpr int kRouteWaypointTargetColumnCameraPitch = 4;
constexpr int kRouteWaypointTargetColumnTargetPoint = 5;
constexpr int kRouteQaColumnSeverity = 0;
constexpr int kRouteQaColumnType = 1;
constexpr int kRouteQaColumnLocation = 2;
constexpr int kRouteQaColumnPart = 3;
constexpr int kRouteQaColumnDescription = 4;
constexpr int kRoutePartColumnPartName = 1;
constexpr int kRoutePartColumnHardware = 2;
constexpr int kRoutePartColumnPhase = 3;
constexpr int kRoutePartColumnCameraAngle = 4;
constexpr int kRoutePartColumnX = 5;
constexpr int kRoutePartColumnY = 6;
constexpr int kRoutePartColumnZ = 7;

struct ClassificationDisplayItem
{
    int code;
    const char* sourceText;
};

const std::array<ClassificationDisplayItem, 33> kClassificationDisplayItems = {
    ClassificationDisplayItem { 0, QT_TRANSLATE_NOOP("MainWindow", "Created / Unclassified") },
    ClassificationDisplayItem { 1, QT_TRANSLATE_NOOP("MainWindow", "Unclassified Point") },
    ClassificationDisplayItem { 2, QT_TRANSLATE_NOOP("MainWindow", "Ground Point") },
    ClassificationDisplayItem { 3, QT_TRANSLATE_NOOP("MainWindow", "Low Vegetation Point") },
    ClassificationDisplayItem { 4, QT_TRANSLATE_NOOP("MainWindow", "Medium Vegetation Point") },
    ClassificationDisplayItem { 5, QT_TRANSLATE_NOOP("MainWindow", "High Vegetation Point") },
    ClassificationDisplayItem { 6, QT_TRANSLATE_NOOP("MainWindow", "Building Point") },
    ClassificationDisplayItem { 7, QT_TRANSLATE_NOOP("MainWindow", "Low Point") },
    ClassificationDisplayItem { 8, QT_TRANSLATE_NOOP("MainWindow", "Model Key Point") },
    ClassificationDisplayItem { 9, QT_TRANSLATE_NOOP("MainWindow", "Temporary Structure") },
    ClassificationDisplayItem { 10, QT_TRANSLATE_NOOP("MainWindow", "Bridge") },
    ClassificationDisplayItem { 11, QT_TRANSLATE_NOOP("MainWindow", "Railway") },
    ClassificationDisplayItem { 12, QT_TRANSLATE_NOOP("MainWindow", "Highway") },
    ClassificationDisplayItem { 13, QT_TRANSLATE_NOOP("MainWindow", "Non-navigable River") },
    ClassificationDisplayItem { 14, QT_TRANSLATE_NOOP("MainWindow", "Lake") },
    ClassificationDisplayItem { 15, QT_TRANSLATE_NOOP("MainWindow", "Substation") },
    ClassificationDisplayItem { 16, QT_TRANSLATE_NOOP("MainWindow", "Conductor") },
    ClassificationDisplayItem { 17, QT_TRANSLATE_NOOP("MainWindow", "Tower") },
    ClassificationDisplayItem { 18, QT_TRANSLATE_NOOP("MainWindow", "Crossing Above") },
    ClassificationDisplayItem { 19, QT_TRANSLATE_NOOP("MainWindow", "Crossing Below") },
    ClassificationDisplayItem { 20, QT_TRANSLATE_NOOP("MainWindow", "Ground Wire") },
    ClassificationDisplayItem { 21, QT_TRANSLATE_NOOP("MainWindow", "Other") },
    ClassificationDisplayItem { 22, QT_TRANSLATE_NOOP("MainWindow", "Boat / Vehicle") },
    ClassificationDisplayItem { 23, QT_TRANSLATE_NOOP("MainWindow", "Other Line") },
    ClassificationDisplayItem { 24, QT_TRANSLATE_NOOP("MainWindow", "Under-Line Structure") },
    ClassificationDisplayItem { 25, QT_TRANSLATE_NOOP("MainWindow", "Navigable River") },
    ClassificationDisplayItem { 26, QT_TRANSLATE_NOOP("MainWindow", "Railway Catenary / Contact Wire") },
    ClassificationDisplayItem { 27, QT_TRANSLATE_NOOP("MainWindow", "Insulator") },
    ClassificationDisplayItem { 28, QT_TRANSLATE_NOOP("MainWindow", "Jumper Wire") },
    ClassificationDisplayItem { 29, QT_TRANSLATE_NOOP("MainWindow", "Tower Body") },
    ClassificationDisplayItem { 30, QT_TRANSLATE_NOOP("MainWindow", "Reserved30") },
    ClassificationDisplayItem { 31, QT_TRANSLATE_NOOP("MainWindow", "Sag Zone") },
    ClassificationDisplayItem { -1, QT_TRANSLATE_NOOP("MainWindow", "Other / Unknown") }
};

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

QString formatCoordinate(float value)
{
    return QLocale().toString(static_cast<double>(value), 'f', 2);
}

QString formatTriplet(float x, float y, float z)
{
    return QStringLiteral("%1, %2, %3")
        .arg(formatCoordinate(x))
        .arg(formatCoordinate(y))
        .arg(formatCoordinate(z));
}

QString formatCoordinateSystemCode(const CoordinateSystemRef& crs, const QString& unsetText)
{
    if (crs.code <= 0) {
        return unsetText;
    }
    return QStringLiteral("%1:%2")
        .arg(crs.authName.trimmed().isEmpty() ? QStringLiteral("EPSG") : crs.authName.trimmed())
        .arg(QLocale().toString(crs.code));
}

QString formatProjectCoordinateSystemsSummary(const lasviewer::crs::ProjectCoordinateSystems& coordinateSystems)
{
    return QCoreApplication::translate("MainWindow", "%1 -> %2")
        .arg(formatCoordinateSystemCode(coordinateSystems.pointCloudCrs, QCoreApplication::translate("MainWindow", "Unset")))
        .arg(formatCoordinateSystemCode(coordinateSystems.geographicCrs, QStringLiteral("EPSG:4326")));
}

QString projectTreeItemType(const QTreeWidgetItem* item)
{
    return item != nullptr ? item->data(0, kProjectTreeItemTypeRole).toString() : QString();
}

QString projectTreeItemFilePath(const QTreeWidgetItem* item)
{
    const QString itemType = projectTreeItemType(item);
    if (itemType == QStringLiteral("pointCloudItem") || itemType == QStringLiteral("imageItem")) {
        return item->data(0, kProjectTreeFilePathRole).toString();
    }
    return QString();
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

QString defaultClassificationDisplayName(int classificationCode)
{
    for (const ClassificationDisplayItem& item : kClassificationDisplayItems) {
        if (item.code == classificationCode) {
            return QCoreApplication::translate("MainWindow", item.sourceText);
        }
    }

    return QCoreApplication::translate("MainWindow", "Custom Class %1")
        .arg(QLocale().toString(classificationCode));
}

QString classificationDisplayName(
    int classificationCode,
    const QMap<int, QString>& customNames)
{
    const QString customName = customNames.value(classificationCode).trimmed();
    return customName.isEmpty()
        ? defaultClassificationDisplayName(classificationCode)
        : customName;
}

QJsonObject colorToJson(const QColor& color);
QColor colorFromJson(const QJsonObject& object, const QColor& fallback);

QString measurementPointText(const MeasurementResult& measurementResult, bool useStartPoint)
{
    const bool hasPoint = useStartPoint ? measurementResult.hasStartPoint : measurementResult.hasEndPoint;
    if (!hasPoint) {
        return QCoreApplication::translate("MainWindow", "Not set");
    }

    const PointRecord& point = useStartPoint ? measurementResult.startPoint : measurementResult.endPoint;
    return formatTriplet(point.x, point.y, point.z);
}

QJsonObject classificationColorMapToJson(const QMap<int, QColor>& colorMap)
{
    QJsonObject object;
    for (auto it = colorMap.constBegin(); it != colorMap.constEnd(); ++it) {
        object.insert(QString::number(it.key()), colorToJson(it.value()));
    }
    return object;
}

QMap<int, QColor> classificationColorMapFromJson(
    const QJsonObject& object,
    const QMap<int, QColor>& fallback)
{
    QMap<int, QColor> map = fallback;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const int classificationCode = it.key().toInt(&ok);
        if (!ok || classificationCode < 0 || classificationCode > 255 || !it->isObject()) {
            continue;
        }

        map.insert(classificationCode, colorFromJson(it->toObject(), map.value(classificationCode)));
    }

    return map;
}

QJsonObject classificationVisibilityMapToJson(const QMap<int, bool>& visibilityMap)
{
    QJsonObject object;
    for (auto it = visibilityMap.constBegin(); it != visibilityMap.constEnd(); ++it) {
        object.insert(QString::number(it.key()), it.value());
    }
    return object;
}

QMap<int, bool> classificationVisibilityMapFromJson(
    const QJsonObject& object,
    const QMap<int, bool>& fallback)
{
    QMap<int, bool> map = fallback;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const int classificationCode = it.key().toInt(&ok);
        if (!ok || classificationCode < -1 || classificationCode > 255 || !it->isBool()) {
            continue;
        }

        map.insert(classificationCode, it->toBool());
    }

    if (!map.contains(-1)) {
        map.insert(-1, true);
    }
    return map;
}

QJsonObject classificationNameMapToJson(const QMap<int, QString>& nameMap)
{
    QJsonObject object;
    for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it) {
        const QString trimmedName = it.value().trimmed();
        if (trimmedName.isEmpty()) {
            continue;
        }
        object.insert(QString::number(it.key()), trimmedName);
    }
    return object;
}

QMap<int, QString> classificationNameMapFromJson(const QJsonObject& object)
{
    QMap<int, QString> map;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        bool ok = false;
        const int classificationCode = it.key().toInt(&ok);
        if (!ok || classificationCode < -1 || classificationCode > 255 || !it->isString()) {
            continue;
        }

        const QString trimmedName = it->toString().trimmed();
        if (!trimmedName.isEmpty()) {
            map.insert(classificationCode, trimmedName);
        }
    }
    return map;
}
QString languageCodeFor(MainWindow::UiLanguage language)
{
    switch (language) {
    case MainWindow::UiLanguage::Chinese:
        return QStringLiteral("zh_CN");
    case MainWindow::UiLanguage::English:
    default:
        return QStringLiteral("en");
    }
}

MainWindow::UiLanguage defaultLanguageFromLocale()
{
    const QString localeName = QLocale::system().name().toLower();
    return localeName.startsWith(QStringLiteral("zh"))
        ? MainWindow::UiLanguage::Chinese
        : MainWindow::UiLanguage::English;
}

bool isSupportedPointCloudFile(const QString& filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    return suffix == QStringLiteral("las") || suffix == QStringLiteral("laz");
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
    if (suffix == QStringLiteral("las") || suffix == QStringLiteral("laz")) {
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

QString routeTableStyleSheet()
{
    return QStringLiteral(
        "QTableWidget {"
        "background-color: #ffffff;"
        "alternate-background-color: #f8fafc;"
        "gridline-color: #e2e8f0;"
        "color: #0f172a;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "}"
        "QHeaderView::section {"
        "background-color: #e2e8f0;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "padding: 4px 8px;"
        "font-weight: 600;"
        "}"
        "QHeaderView::section:hover {"
        "background-color: #dbeafe;"
        "}"
        "QHeaderView::section:pressed {"
        "background-color: #bfdbfe;"
        "}"
        "QTableCornerButton::section {"
        "background-color: #e2e8f0;"
        "border: 1px solid #cbd5e1;"
        "}");
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

constexpr int kRecentProjectHistoryLimit = 10;
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

QString normalizedProjectFilePath(const QString& filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return QString();
    }

    return QDir::fromNativeSeparators(QFileInfo(trimmedPath).absoluteFilePath());
}

bool isSupportedProjectFilePath(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("json") || suffix == QStringLiteral("lpproj");
}

int indexOfProjectFilePath(const QStringList& filePaths, const QString& targetPath)
{
    for (int index = 0; index < filePaths.size(); ++index) {
        if (QString::compare(filePaths.at(index), targetPath, Qt::CaseInsensitive) == 0) {
            return index;
        }
    }

    return -1;
}

QStringList normalizedRecentProjectFiles(const QStringList& recentPaths, const QString& preferredPath = QString())
{
    QStringList normalizedPaths;
    QSet<QString> dedupKeys;
    const auto appendPath = [&](const QString& path, bool allowMissing) {
        const QString normalizedPath = normalizedProjectFilePath(path);
        if (normalizedPath.isEmpty() || !isSupportedProjectFilePath(normalizedPath)) {
            return;
        }

        const QFileInfo fileInfo(normalizedPath);
        if (!allowMissing && !fileInfo.exists()) {
            return;
        }

        const QString dedupKey = normalizedPath.toLower();
        if (dedupKeys.contains(dedupKey)) {
            return;
        }
        dedupKeys.insert(dedupKey);
        normalizedPaths.append(normalizedPath);
    };

    if (!preferredPath.isEmpty()) {
        appendPath(preferredPath, false);
    }
    for (const QString& recentPath : recentPaths) {
        appendPath(recentPath, false);
    }

    if (normalizedPaths.size() > kRecentProjectHistoryLimit) {
        normalizedPaths = normalizedPaths.mid(0, kRecentProjectHistoryLimit);
    }
    return normalizedPaths;
}

void recordRecentProjectFilePath(const QString& filePath)
{
    const QString normalizedPath = normalizedProjectFilePath(filePath);
    if (normalizedPath.isEmpty() || !QFileInfo::exists(normalizedPath)) {
        return;
    }

    QSettings settings;
    const QStringList recentProjects = normalizedRecentProjectFiles(
        settings.value(settingskeys::kProjectRecentProjects).toStringList(),
        normalizedPath);
    settings.setValue(settingskeys::kProjectRecentProjects, recentProjects);
    settings.setValue(settingskeys::kProjectLastOpenedProject, normalizedPath);
}

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

QString projectRelativePathFor(const QString& projectFilePath, const QString& targetFilePath)
{
    const QFileInfo targetInfo(targetFilePath);
    if (!targetInfo.exists()) {
        return targetFilePath;
    }

    const QDir projectDir = QFileInfo(projectFilePath).absoluteDir();
    return projectDir.relativeFilePath(targetInfo.absoluteFilePath());
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

void MainWindow::createActions()
{
    openAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Open"), this);
    openAction_->setShortcut(QKeySequence::Open);
    openAction_->setToolTip(tr("Open a point cloud, route file, or project"));
    addPointCloudAction_ = new QAction(createRibbonIcon(RibbonGlyph::Open), tr("Add LAS Files"), this);
    addPointCloudAction_->setToolTip(tr("Add one or more LAS or LAZ datasets to the current project"));
    removeDatasetAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Selected Dataset"), this);
    removeDatasetAction_->setToolTip(tr("Remove the selected LAS or LAZ dataset from the project"));
    locateDatasetAction_ = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon), tr("Open Folder"), this);
    locateDatasetAction_->setToolTip(tr("Open the folder that contains the selected dataset"));
    copyDatasetPathAction_ = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), tr("Copy Path"), this);
    copyDatasetPathAction_->setToolTip(tr("Copy the full path of the selected dataset"));
    expandProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowDown), tr("Expand All"), this);
    expandProjectTreeAction_->setToolTip(tr("Expand the project explorer tree"));
    collapseProjectTreeAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowUp), tr("Collapse All"), this);
    collapseProjectTreeAction_->setToolTip(tr("Collapse the project explorer tree"));

    openProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Open Project"), this);
    saveProjectAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project"), this);
    saveProjectAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Project As"), this);
    projectCoordinateSystemsAction_ = new QAction(
        style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        tr("Project Properties"),
        this);
    projectCoordinateSystemsAction_->setToolTip(tr("Open project coordinate system settings"));

    clearAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear"), this);
    clearAction_->setToolTip(tr("Clear the current scene"));

    exitAction_ = new QAction(createRibbonIcon(RibbonGlyph::Exit), tr("Exit"), this);
    exitAction_->setShortcut(QKeySequence::Quit);

    fitSceneAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Fit Scene"), this);
    fitSceneAction_->setToolTip(tr("Reset to a fitted isometric view"));

    topViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Top), tr("Top"), this);
    topViewAction_->setToolTip(tr("Switch to top view"));
    frontViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Front), tr("Front"), this);
    frontViewAction_->setToolTip(tr("Switch to front view"));
    rightViewAction_ = new QAction(createRibbonIcon(RibbonGlyph::Right), tr("Right"), this);
    rightViewAction_->setToolTip(tr("Switch to right view"));

    showAxesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Axes), tr("Axes"), this);
    showAxesAction_->setCheckable(true);
    showAxesAction_->setToolTip(tr("Show or hide XYZ axes"));

    showBoundingBoxAction_ = new QAction(createRibbonIcon(RibbonGlyph::Bounds), tr("Bounds"), this);
    showBoundingBoxAction_->setCheckable(true);
    showBoundingBoxAction_->setToolTip(tr("Show or hide point cloud bounds"));

    darkBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::DarkBackground), tr("Dark"), this);
    darkBackgroundAction_->setToolTip(tr("Switch to dark background"));
    lightBackgroundAction_ = new QAction(createRibbonIcon(RibbonGlyph::LightBackground), tr("Light"), this);
    lightBackgroundAction_->setToolTip(tr("Switch to light background"));

    colorModeActionGroup_ = new QActionGroup(this);
    colorModeActionGroup_->setExclusive(true);

    rgbColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Rgb), tr("RGB"), this);
    rgbColorAction_->setCheckable(true);
    elevationColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Elevation), tr("Elevation"), this);
    elevationColorAction_->setCheckable(true);
    singleColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::SingleColor), tr("Single"), this);
    singleColorAction_->setCheckable(true);
    classificationColorAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Classification"), this);
    classificationColorAction_->setCheckable(true);

    colorModeActionGroup_->addAction(rgbColorAction_);
    colorModeActionGroup_->addAction(elevationColorAction_);
    colorModeActionGroup_->addAction(singleColorAction_);
    colorModeActionGroup_->addAction(classificationColorAction_);

    themeActionGroup_ = new QActionGroup(this);
    themeActionGroup_->setExclusive(true);

    themeColorfulAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeColorful), tr("Colorful"), this);
    themeColorfulAction_->setCheckable(true);
    themeWhiteAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeWhite), tr("White"), this);
    themeWhiteAction_->setCheckable(true);
    themeDarkGrayAction_ = new QAction(createRibbonIcon(RibbonGlyph::ThemeDarkGray), tr("Dark Gray"), this);
    themeDarkGrayAction_->setCheckable(true);

    themeActionGroup_->addAction(themeColorfulAction_);
    themeActionGroup_->addAction(themeWhiteAction_);
    themeActionGroup_->addAction(themeDarkGrayAction_);

    measureAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Measure"), this);
    measureAction_->setCheckable(true);
    profileClassificationAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Profile Classify"), this);
    profileClassificationAction_->setCheckable(true);
    profileClassificationAction_->setToolTip(tr("Enable profile classification and choose rectangle or polygon selection in the panel"));
    showProfileClassificationDockAction_ = new QAction(createRibbonIcon(RibbonGlyph::Classification), tr("Classify Panel"), this);
    showProfileClassificationDockAction_->setCheckable(true);
    showProfileClassificationDockAction_->setChecked(false);
    saveProfileClassificationEditsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Classify Result"), this);
    undoProfileClassificationAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowBack), tr("Undo Classify"), this);
    redoProfileClassificationAction_ = new QAction(style()->standardIcon(QStyle::SP_ArrowForward), tr("Redo Classify"), this);
    clearProfileClassificationEditsAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Classify Edits"), this);

    clearMeasurementAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Measure"), this);
    exportClearanceCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Clearance CSV"), this);
    showProfileDockAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Profile View"), this);
    showProfileDockAction_->setCheckable(true);
    showProfileDockAction_->setChecked(false);
    analyzeVegetationRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Analyze Risks"), this);
    analyzeVegetationRisksAction_->setToolTip(tr("Analyze vegetation risks around the current measured corridor"));
    focusVegetationRiskAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Risk"), this);
    createIssueFromRiskAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Create Issue"), this);
    createIssuesFromRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Create All Issues"), this);
    clearVegetationRisksAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Risks"), this);
    generateInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Generate Route"), this);
    regenerateInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Regenerate Route"), this);
    clearInspectionRouteAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Route"), this);
    toggleRouteEditingAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerAdjust), tr("Edit Route"), this);
    toggleRouteEditingAction_->setCheckable(true);
    toggleRouteEditingAction_->setChecked(routeEditingEnabled_);
    toggleRouteEditingAction_->setToolTip(tr("Enable waypoint edit, delete, and drag operations for the current route"));
    startInspectionRouteRoamAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Start Roam"), this);
    pauseInspectionRouteRoamAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Pause Roam"), this);
    stopInspectionRouteRoamAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Stop Roam"), this);
    focusRouteWaypointAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Route Point"), this);
    importRouteFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import Route File"), this);
    saveRouteFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Route File"), this);
    saveRouteFileAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Route File As"), this);
    reloadRouteFileAction_ = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Reload Route File"), this);
    importRouteKmlAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import Route KML"), this);
    exportRouteKmlAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Route KML"), this);
    exportRouteDjiKmzAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export DJI KMZ"), this);

    startTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Tower), tr("Start Editing"), this);
    finishTowerEditAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Finish Editing"), this);
    addTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/addTower.png"), RibbonGlyph::TowerAdd),
        tr("Click To Add Tower"),
        this);
    insertTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/addTowerPrevious.png"), RibbonGlyph::TowerInsert),
        tr("Insert Before Current"),
        this);
    moveTowerAction_ = new QAction(createRibbonIcon(RibbonGlyph::TowerMove), tr("Move Current Tower"), this);
    editCurrentTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/modifyTower.png"), RibbonGlyph::TowerAdjust),
        tr("Edit Current Tower"),
        this);
    focusTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/focusTower.png"), RibbonGlyph::TowerFocus),
        tr("Focus Current Tower"),
        this);
    removeTowerAction_ = new QAction(
        createResourceIconOrFallback(QStringLiteral(":/assets/icon/deleteTower.png"), RibbonGlyph::TowerRemove),
        tr("Remove Current Tower"),
        this);
    clearTowersAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Tower Markers"), this);
    cancelTowerToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Tower Tool"), this);
    importTowerFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import Tower File"), this);
    saveTowerFileAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Tower File"), this);
    saveTowerFileAsAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Save Tower File As"), this);
    reloadTowerFileAction_ = new QAction(style()->standardIcon(QStyle::SP_BrowserReload), tr("Reload Tower File"), this);
    showTowerXAction_ = new QAction(tr("Show X"), this);
    showTowerYAction_ = new QAction(tr("Show Y"), this);
    showTowerZAction_ = new QAction(tr("Show Z"), this);
    showTowerXAction_->setCheckable(true);
    showTowerYAction_->setCheckable(true);
    showTowerZAction_->setCheckable(true);
    startIssueMarkAction_ = new QAction(createRibbonIcon(RibbonGlyph::Measure), tr("Mark Issue"), this);
    startIssueMarkAction_->setToolTip(tr("Click a point in the view to add an inspection issue"));
    cancelIssueToolAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Cancel Issue Tool"), this);
    focusIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Fit), tr("Focus Current Issue"), this);
    removeIssueAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Remove Current Issue"), this);
    clearIssuesAction_ = new QAction(createRibbonIcon(RibbonGlyph::Clear), tr("Clear Issues"), this);
    exportIssuesCsvAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Issues CSV"), this);
    exportInspectionReportAction_ = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("Export Inspection Report"), this);

    showLogAction_ = new QAction(createRibbonIcon(RibbonGlyph::Log), tr("Log"), this);
    showLogAction_->setCheckable(true);
    showLogAction_->setChecked(false);
    showLogAction_->setToolTip(tr("Show or hide the log panel"));

    languageActionGroup_ = new QActionGroup(this);
    languageActionGroup_->setExclusive(true);

    languageEnglishAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), tr("English"), this);
    languageEnglishAction_->setCheckable(true);
    languageChineseAction_ = new QAction(createRibbonIcon(RibbonGlyph::Language), tr("Chinese"), this);
    languageChineseAction_->setCheckable(true);

    languageActionGroup_->addAction(languageEnglishAction_);
    languageActionGroup_->addAction(languageChineseAction_);
}

void MainWindow::createRibbon()
{
    ribbonBar_ = new Qtitan::RibbonBar(this);
    // Avoid Qtitan's DWM-integrated frame path, which paints a black caption area on this setup.
    ribbonBar_->setFrameThemeEnabled(false);
    ribbonBar_->setTitleBarVisible(false);
    ribbonBar_->showQuickAccess(true);
    ribbonBar_->setStyleSheet(QStringLiteral(
        "QAbstractButton {"
        "background-color: transparent;"
        "border: none;"
        "color: #0f172a;"
        "}"
        "QAbstractButton:hover {"
        "background-color: rgba(37, 99, 235, 0.16);"
        "color: #0b1220;"
        "}"
        "QAbstractButton:checked, QAbstractButton:pressed {"
        "background-color: #1d4ed8;"
        "color: #ffffff;"
        "}"
        "QAbstractButton:disabled {"
        "background-color: transparent;"
        "color: #64748b;"
        "}"));
    ribbonBar_->installEventFilter(this);
    setRibbonBar(ribbonBar_);
    createWindowControls();
    ribbonBar_->addSystemButton(createRibbonIcon(RibbonGlyph::Open), tr("File"));
    backstageSystemButton_ = ribbonBar_->getSystemButton();
    createBackstageView();

    ribbonBar_->quickAccessBar()->addAction(openAction_);
    ribbonBar_->quickAccessBar()->addAction(saveProjectAction_);
    ribbonBar_->quickAccessBar()->addAction(fitSceneAction_);
    ribbonBar_->quickAccessBar()->addAction(showAxesAction_);
    ribbonBar_->quickAccessBar()->addAction(measureAction_);

    homePage_ = ribbonBar_->addPage(tr("Home"));
    datasetRibbonGroup_ = homePage_->addGroup(tr("Dataset"));
    datasetRibbonGroup_->addAction(openAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(addPointCloudAction_, Qt::ToolButtonTextUnderIcon);
    datasetRibbonGroup_->addAction(clearAction_, Qt::ToolButtonTextUnderIcon);

    cameraRibbonGroup_ = homePage_->addGroup(tr("Camera"));
    cameraRibbonGroup_->addAction(fitSceneAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(topViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(frontViewAction_, Qt::ToolButtonTextUnderIcon);
    cameraRibbonGroup_->addAction(rightViewAction_, Qt::ToolButtonTextUnderIcon);

    sceneRibbonGroup_ = homePage_->addGroup(tr("Scene"));
    sceneRibbonGroup_->addAction(showAxesAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(showBoundingBoxAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(darkBackgroundAction_, Qt::ToolButtonTextUnderIcon);
    sceneRibbonGroup_->addAction(lightBackgroundAction_, Qt::ToolButtonTextUnderIcon);

    measureRibbonGroup_ = homePage_->addGroup(tr("Measure"));
    measureRibbonGroup_->addAction(measureAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(clearMeasurementAction_, Qt::ToolButtonTextUnderIcon);
    measureRibbonGroup_->addAction(showProfileDockAction_, Qt::ToolButtonTextUnderIcon);

    classificationRibbonGroup_ = homePage_->addGroup(tr("Classification"));
    classificationRibbonGroup_->addAction(profileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(showProfileClassificationDockAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(saveProfileClassificationEditsAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(undoProfileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(redoProfileClassificationAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(clearProfileClassificationEditsAction_, Qt::ToolButtonTextUnderIcon);
    classificationRibbonGroup_->addAction(exportClearanceCsvAction_, Qt::ToolButtonTextUnderIcon);

    routePage_ = ribbonBar_->addPage(tr("Route"));
    routePlanningRibbonGroup_ = routePage_->addGroup(tr("Route Planning"));
    routePlanningRibbonGroup_->addAction(generateInspectionRouteAction_, Qt::ToolButtonTextUnderIcon);
    routePlanningRibbonGroup_->addAction(regenerateInspectionRouteAction_, Qt::ToolButtonTextUnderIcon);
    routePlanningRibbonGroup_->addAction(clearInspectionRouteAction_, Qt::ToolButtonTextUnderIcon);
    routePlanningRibbonGroup_->addAction(toggleRouteEditingAction_, Qt::ToolButtonTextUnderIcon);

    routeRoamRibbonGroup_ = routePage_->addGroup(tr("Route Roam"));
    routeRoamRibbonGroup_->addAction(startInspectionRouteRoamAction_, Qt::ToolButtonTextUnderIcon);
    routeRoamRibbonGroup_->addAction(pauseInspectionRouteRoamAction_, Qt::ToolButtonTextUnderIcon);
    routeRoamRibbonGroup_->addAction(stopInspectionRouteRoamAction_, Qt::ToolButtonTextUnderIcon);
    routeRoamRibbonGroup_->addAction(focusRouteWaypointAction_, Qt::ToolButtonTextUnderIcon);

    routeFileRibbonGroup_ = routePage_->addGroup(tr("Route Files"));
    routeFileRibbonGroup_->addAction(importRouteFileAction_, Qt::ToolButtonTextUnderIcon);
    routeFileRibbonGroup_->addAction(saveRouteFileAction_, Qt::ToolButtonTextUnderIcon);
    routeFileRibbonGroup_->addAction(saveRouteFileAsAction_, Qt::ToolButtonTextUnderIcon);
    routeFileRibbonGroup_->addAction(reloadRouteFileAction_, Qt::ToolButtonTextUnderIcon);

    routeExchangeRibbonGroup_ = routePage_->addGroup(tr("Route Exchange"));
    routeExchangeRibbonGroup_->addAction(importRouteKmlAction_, Qt::ToolButtonTextUnderIcon);
    routeExchangeRibbonGroup_->addAction(exportRouteKmlAction_, Qt::ToolButtonTextUnderIcon);
    routeExchangeRibbonGroup_->addAction(exportRouteDjiKmzAction_, Qt::ToolButtonTextUnderIcon);

    analysisPage_ = ribbonBar_->addPage(tr("Analysis"));
    vegetationRiskRibbonGroup_ = analysisPage_->addGroup(tr("Vegetation Risks"));
    vegetationRiskRibbonGroup_->addAction(analyzeVegetationRisksAction_, Qt::ToolButtonTextUnderIcon);
    vegetationRiskRibbonGroup_->addAction(focusVegetationRiskAction_, Qt::ToolButtonTextUnderIcon);
    vegetationRiskRibbonGroup_->addAction(createIssueFromRiskAction_, Qt::ToolButtonTextUnderIcon);
    vegetationRiskRibbonGroup_->addAction(createIssuesFromRisksAction_, Qt::ToolButtonTextUnderIcon);
    vegetationRiskRibbonGroup_->addAction(clearVegetationRisksAction_, Qt::ToolButtonTextUnderIcon);

    issuePage_ = ribbonBar_->addPage(tr("Issue"));
    issueRibbonGroup_ = issuePage_->addGroup(tr("Inspection Issues"));
    issueRibbonGroup_->addAction(startIssueMarkAction_, Qt::ToolButtonTextUnderIcon);
    issueRibbonGroup_->addAction(cancelIssueToolAction_, Qt::ToolButtonTextUnderIcon);
    issueRibbonGroup_->addAction(focusIssueAction_, Qt::ToolButtonTextUnderIcon);
    issueRibbonGroup_->addAction(removeIssueAction_, Qt::ToolButtonTextUnderIcon);
    issueRibbonGroup_->addAction(clearIssuesAction_, Qt::ToolButtonTextUnderIcon);
    issueRibbonGroup_->addAction(exportIssuesCsvAction_, Qt::ToolButtonTextUnderIcon);
    issueRibbonGroup_->addAction(exportInspectionReportAction_, Qt::ToolButtonTextUnderIcon);

    towerPage_ = ribbonBar_->addPage(tr("Tower"));
    towerRibbonGroup_ = towerPage_->addGroup(tr("Tower Editing"));
    towerRibbonGroup_->addAction(startTowerEditAction_, Qt::ToolButtonTextUnderIcon);
    towerRibbonGroup_->addAction(finishTowerEditAction_, Qt::ToolButtonTextUnderIcon);

    appearancePage_ = ribbonBar_->addPage(tr("Appearance"));
    colorRibbonGroup_ = appearancePage_->addGroup(tr("Point Colors"));
    colorRibbonGroup_->addAction(rgbColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(elevationColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(singleColorAction_, Qt::ToolButtonTextUnderIcon);
    colorRibbonGroup_->addAction(classificationColorAction_, Qt::ToolButtonTextUnderIcon);
}

void MainWindow::createBackstageView()
{
    if (ribbonBar_ == nullptr || backstageSystemButton_ == nullptr) {
        return;
    }

    backstageView_ = new Qtitan::RibbonBackstageView(ribbonBar_);
    backstageView_->setObjectName(QStringLiteral("mainBackstageView"));

    const auto initializeBackstagePage = [](QWidget* page) {
        if (page == nullptr) {
            return;
        }
        page->setStyleSheet(backstagePageStyleSheet());
    };

    backstageOpenPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageOpenPage_->setObjectName(QStringLiteral("backstageOpenPage"));
    backstageOpenPage_->setWindowTitle(tr("Open"));
    initializeBackstagePage(backstageOpenPage_);
    {
        auto* layout = new QVBoxLayout(backstageOpenPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* openHeaderWidget = new BackstagePageHeaderWidget(backstageOpenPage_);
        openHeaderWidget->setTitleText(tr("Open"));
        openHeaderWidget->setSubtitleText(tr("Open point clouds and projects, or continue from a recent engineering file."));
        backstageOpenTitleLabel_ = openHeaderWidget->titleLabel();
        backstageOpenSubtitleLabel_ = openHeaderWidget->subtitleLabel();
        layout->addWidget(openHeaderWidget);

        auto* actionsCard = createBackstageCard(backstageOpenPage_);
        auto* actionsCardLayout = new QVBoxLayout(actionsCard);
        actionsCardLayout->setContentsMargins(0, 0, 0, 0);

        auto* openActionsWidget = new BackstageOpenActionsWidget(actionsCard);
        if (QVBoxLayout* actionsLayout = openActionsWidget->actionsLayout()) {
            actionsLayout->addWidget(createBackstageActionButton(openAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(addPointCloudAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(openProjectAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(saveProjectAction_, openActionsWidget));
            actionsLayout->addWidget(createBackstageActionButton(saveProjectAsAction_, openActionsWidget));
            actionsLayout->addStretch(1);
        }
        actionsCardLayout->addWidget(openActionsWidget);
        layout->addWidget(actionsCard, 1);
    }
    backstageOpenPageAction_ = backstageView_->addPage(backstageOpenPage_);

    backstageOpenProjectPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageOpenProjectPage_->setObjectName(QStringLiteral("backstageOpenProjectPage"));
    backstageOpenProjectPage_->setWindowTitle(tr("Open Project"));
    initializeBackstagePage(backstageOpenProjectPage_);
    {
        auto* layout = new QVBoxLayout(backstageOpenProjectPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* openProjectHeaderWidget = new BackstagePageHeaderWidget(backstageOpenProjectPage_);
        openProjectHeaderWidget->setTitleText(tr("Open Project"));
        openProjectHeaderWidget->setSubtitleText(tr("Select a recent project or browse to a project file."));
        backstageOpenProjectTitleLabel_ = openProjectHeaderWidget->titleLabel();
        backstageOpenProjectSubtitleLabel_ = openProjectHeaderWidget->subtitleLabel();
        layout->addWidget(openProjectHeaderWidget);

        auto* contentCard = createBackstageCard(backstageOpenProjectPage_);
        auto* contentLayout = new QVBoxLayout(contentCard);
        contentLayout->setContentsMargins(20, 20, 20, 20);
        contentLayout->setSpacing(14);

        backstageOpenProjectWidget_ = new BackstageOpenProjectWidget(contentCard);
        backstageRecentProjectsListWidget_ = backstageOpenProjectWidget_->recentProjectsListWidget();
        backstageProjectPathLineEdit_ = backstageOpenProjectWidget_->projectPathLineEdit();
        backstageProjectBrowseButton_ = backstageOpenProjectWidget_->browseButton();
        backstageProjectOpenButton_ = backstageOpenProjectWidget_->openButton();
        contentLayout->addWidget(backstageOpenProjectWidget_);

        layout->addWidget(contentCard, 1);
    }
    backstageOpenProjectPageAction_ = backstageView_->addPage(backstageOpenProjectPage_);

    backstageSaveAction_ = backstageView_->addAction(saveProjectAction_->icon(), saveProjectAction_->text());
    backstageSaveAsAction_ = backstageView_->addAction(saveProjectAsAction_->icon(), saveProjectAsAction_->text());

    backstageProjectPropertiesPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageProjectPropertiesPage_->setObjectName(QStringLiteral("backstageProjectPropertiesPage"));
    backstageProjectPropertiesPage_->setWindowTitle(tr("Project Properties"));
    initializeBackstagePage(backstageProjectPropertiesPage_);
    {
        auto* layout = new QVBoxLayout(backstageProjectPropertiesPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* projectPropertiesHeaderWidget = new BackstagePageHeaderWidget(backstageProjectPropertiesPage_);
        projectPropertiesHeaderWidget->setTitleText(tr("Project Properties"));
        projectPropertiesHeaderWidget->setSubtitleText(tr("Review the active project file and coordinate system configuration."));
        backstageProjectPropertiesTitleLabel_ = projectPropertiesHeaderWidget->titleLabel();
        backstageProjectPropertiesSubtitleLabel_ = projectPropertiesHeaderWidget->subtitleLabel();
        layout->addWidget(projectPropertiesHeaderWidget);

        auto* summaryCard = createBackstageCard(backstageProjectPropertiesPage_);
        auto* summaryLayout = new QVBoxLayout(summaryCard);
        summaryLayout->setContentsMargins(20, 20, 20, 20);
        summaryLayout->setSpacing(0);

        backstageProjectPropertiesWidget_ = new BackstageProjectPropertiesWidget(summaryCard);
        backstageProjectFileValueLabel_ = backstageProjectPropertiesWidget_->projectFileValueLabel();
        backstageProjectDatasetCountValueLabel_ = backstageProjectPropertiesWidget_->datasetCountValueLabel();
        backstageProjectCoordinateSystemsValueLabel_ = backstageProjectPropertiesWidget_->coordinateSystemsValueLabel();
        backstageEditProjectPropertiesButton_ = backstageProjectPropertiesWidget_->editCoordinateSystemsButton();
        summaryLayout->addWidget(backstageProjectPropertiesWidget_);
        layout->addWidget(summaryCard);
        layout->addStretch(1);
    }
    backstageProjectPropertiesPageAction_ = backstageView_->addPage(backstageProjectPropertiesPage_);

    backstageApplicationSettingsPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageApplicationSettingsPage_->setObjectName(QStringLiteral("backstageApplicationSettingsPage"));
    backstageApplicationSettingsPage_->setWindowTitle(tr("Application Settings"));
    initializeBackstagePage(backstageApplicationSettingsPage_);
    {
        auto* layout = new QVBoxLayout(backstageApplicationSettingsPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* applicationSettingsHeaderWidget = new BackstagePageHeaderWidget(backstageApplicationSettingsPage_);
        applicationSettingsHeaderWidget->setTitleText(tr("Application Settings"));
        applicationSettingsHeaderWidget->setSubtitleText(tr("Adjust the office theme, interface language, and workspace panels."));
        backstageApplicationSettingsTitleLabel_ = applicationSettingsHeaderWidget->titleLabel();
        backstageApplicationSettingsSubtitleLabel_ = applicationSettingsHeaderWidget->subtitleLabel();
        layout->addWidget(applicationSettingsHeaderWidget);

        backstageApplicationSettingsWidget_ = new BackstageApplicationSettingsWidget(backstageApplicationSettingsPage_);
        if (QHBoxLayout* themeLayout = backstageApplicationSettingsWidget_->themeButtonLayout()) {
            themeLayout->addWidget(createBackstageActionButton(themeColorfulAction_, backstageApplicationSettingsWidget_));
            themeLayout->addWidget(createBackstageActionButton(themeWhiteAction_, backstageApplicationSettingsWidget_));
            themeLayout->addWidget(createBackstageActionButton(themeDarkGrayAction_, backstageApplicationSettingsWidget_));
            themeLayout->addStretch(1);
        }
        if (QHBoxLayout* languageLayout = backstageApplicationSettingsWidget_->languageButtonLayout()) {
            languageLayout->addWidget(createBackstageActionButton(languageEnglishAction_, backstageApplicationSettingsWidget_));
            languageLayout->addWidget(createBackstageActionButton(languageChineseAction_, backstageApplicationSettingsWidget_));
            languageLayout->addStretch(1);
        }
        backstageShowLogCheckBox_ = backstageApplicationSettingsWidget_->showLogCheckBox();
        layout->addWidget(backstageApplicationSettingsWidget_);
        layout->addStretch(1);
    }
    backstageApplicationSettingsPageAction_ = backstageView_->addPage(backstageApplicationSettingsPage_);

    backstageAboutPage_ = new Qtitan::RibbonBackstagePage(backstageView_);
    backstageAboutPage_->setObjectName(QStringLiteral("backstageAboutPage"));
    backstageAboutPage_->setWindowTitle(tr("About"));
    initializeBackstagePage(backstageAboutPage_);
    {
        auto* layout = new QVBoxLayout(backstageAboutPage_);
        layout->setContentsMargins(34, 28, 34, 28);
        layout->setSpacing(18);

        auto* aboutHeaderWidget = new BackstagePageHeaderWidget(backstageAboutPage_);
        aboutHeaderWidget->setTitleText(tr("About"));
        aboutHeaderWidget->setSubtitleText(tr("Build information and the key runtime components used by this application."));
        backstageAboutTitleLabel_ = aboutHeaderWidget->titleLabel();
        backstageAboutSubtitleLabel_ = aboutHeaderWidget->subtitleLabel();
        layout->addWidget(aboutHeaderWidget);

        auto* aboutCard = createBackstageCard(backstageAboutPage_);
        auto* aboutLayout = new QVBoxLayout(aboutCard);
        aboutLayout->setContentsMargins(20, 20, 20, 20);
        backstageAboutWidget_ = new BackstageAboutWidget(aboutCard);
        backstageAboutBodyLabel_ = backstageAboutWidget_->bodyLabel();
        aboutLayout->addWidget(backstageAboutWidget_);
        layout->addWidget(aboutCard);
        layout->addStretch(1);
    }
    backstageAboutPageAction_ = backstageView_->addPage(backstageAboutPage_);

    backstageView_->addSeparator();
    backstageExitAction_ = backstageView_->addAction(exitAction_->icon(), exitAction_->text());

    backstageSystemButton_->setBackstage(backstageView_);

    connect(backstageSaveAction_, &QAction::triggered, this, [this]() { saveProject(); });
    connect(backstageSaveAsAction_, &QAction::triggered, this, [this]() { saveProjectAs(); });
    connect(backstageExitAction_, &QAction::triggered, this, [this]() {
        hideBackstageView();
        close();
    });
    connect(backstageProjectBrowseButton_, &QPushButton::clicked, this, [this]() {
        const QString selectedPath = showStyledOpenFileNameDialog(
            this,
            tr("Open Project"),
            backstageProjectPathLineEdit_ != nullptr ? backstageProjectPathLineEdit_->text().trimmed() : QString(),
            tr("LiDAR Power Projects (*.json *.lpproj);;JSON Files (*.json);;All Files (*.*)"));
        if (!selectedPath.isEmpty() && backstageProjectPathLineEdit_ != nullptr) {
            backstageProjectPathLineEdit_->setText(normalizedProjectFilePath(selectedPath));
        }
    });
    connect(backstageProjectOpenButton_, &QPushButton::clicked, this, [this]() { openProjectFromBackstage(); });
    connect(backstageProjectPathLineEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (backstageProjectOpenButton_ != nullptr) {
            backstageProjectOpenButton_->setEnabled(!normalizedProjectFilePath(text).isEmpty());
        }
    });
    connect(backstageRecentProjectsListWidget_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem*) {
        if (current == nullptr || backstageProjectPathLineEdit_ == nullptr) {
            return;
        }
        const QString selectedPath = current->data(Qt::UserRole).toString();
        if (!selectedPath.isEmpty()) {
            backstageProjectPathLineEdit_->setText(selectedPath);
        }
    });
    connect(backstageRecentProjectsListWidget_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (item == nullptr || item->data(Qt::UserRole).toString().isEmpty()) {
            return;
        }
        openProjectFromBackstage();
    });
    connect(backstageEditProjectPropertiesButton_, &QPushButton::clicked, this, [this]() { openProjectCoordinateSystems(); });
    connect(backstageShowLogCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        if (showLogAction_ != nullptr && showLogAction_->isChecked() != checked) {
            showLogAction_->setChecked(checked);
        }
    });
    connect(showLogAction_, &QAction::toggled, this, [this](bool checked) {
        if (backstageShowLogCheckBox_ != nullptr && backstageShowLogCheckBox_->isChecked() != checked) {
            const QSignalBlocker blocker(backstageShowLogCheckBox_);
            backstageShowLogCheckBox_->setChecked(checked);
        }
    });
    connect(backstageView_, &Qtitan::RibbonBackstageView::aboutToShow, this, [this]() {
        refreshBackstageRecentProjects();
        refreshBackstageProjectPropertiesPage();
        refreshBackstageApplicationSettingsPage();
        refreshBackstageAboutPage();
    });
}

void MainWindow::createViewQuickToolBar()
{
    const double quickToolUiScale = std::clamp(static_cast<double>(logicalDpiX()) / 96.0, 1.0, 2.0);
    const int quickToolIconSize = static_cast<int>(std::lround(18.0 * quickToolUiScale));
    const int quickToolSpacing = static_cast<int>(std::lround(4.0 * quickToolUiScale));
    const int quickToolPaddingY = static_cast<int>(std::lround(6.0 * quickToolUiScale));
    const int quickToolPaddingX = static_cast<int>(std::lround(3.0 * quickToolUiScale));
    const int quickToolButtonPadding = static_cast<int>(std::lround(6.0 * quickToolUiScale));
    const int quickToolButtonRadius = static_cast<int>(std::lround(8.0 * quickToolUiScale));
    const int quickToolButtonSize = quickToolIconSize + quickToolButtonPadding * 2;
    const int quickToolSeparatorMarginY = static_cast<int>(std::lround(6.0 * quickToolUiScale));
    const int quickToolSeparatorMarginX = static_cast<int>(std::lround(8.0 * quickToolUiScale));
    const int quickToolTipPaddingY = static_cast<int>(std::lround(4.0 * quickToolUiScale));
    const int quickToolTipPaddingX = static_cast<int>(std::lround(8.0 * quickToolUiScale));

    viewQuickToolBar_ = new QToolBar(tr("View Toolbar"), this);
    viewQuickToolBar_->setObjectName(QStringLiteral("viewQuickToolBar"));
    viewQuickToolBar_->setOrientation(Qt::Vertical);
    viewQuickToolBar_->setMovable(false);
    viewQuickToolBar_->setFloatable(false);
    viewQuickToolBar_->setIconSize(QSize(quickToolIconSize, quickToolIconSize));
    viewQuickToolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    viewQuickToolBar_->setContextMenuPolicy(Qt::PreventContextMenu);
    viewQuickToolBar_->setStyleSheet(QStringLiteral(
        "QToolBar#viewQuickToolBar {"
        "background-color: #f8fbff;"
        "border-right: 1px solid #d5e2f0;"
        "spacing: %1px;"
        "padding: %2px %3px;"
        "}"
        "QToolButton {"
        "background: transparent;"
        "border: 1px solid transparent;"
        "border-radius: %4px;"
        "padding: %5px;"
        "min-width: %6px;"
        "min-height: %6px;"
        "color: #0f172a;"
        "}"
        "QToolButton:hover {"
        "background-color: #e0ebfb;"
        "border-color: #c4d7f2;"
        "}"
        "QToolButton:checked {"
        "background-color: #d0e2ff;"
        "border-color: #97b8ea;"
        "}"
        "QToolButton:pressed {"
        "background-color: #bfdbfe;"
        "border-color: #7ea6de;"
        "}"
        "QToolButton:disabled {"
        "color: #94a3b8;"
        "}"
        "QToolTip {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "border: 1px solid #94a3b8;"
        "padding: %7px %8px;"
        "border-radius: 4px;"
        "}"
        "QToolBar::separator {"
        "background: #d8e3f2;"
        "width: 1px;"
        "height: 1px;"
        "margin: %9px %10px;"
        "}")
        .arg(quickToolSpacing)
        .arg(quickToolPaddingY)
        .arg(quickToolPaddingX)
        .arg(quickToolButtonRadius)
        .arg(quickToolButtonPadding)
        .arg(quickToolButtonSize)
        .arg(quickToolTipPaddingY)
        .arg(quickToolTipPaddingX)
        .arg(quickToolSeparatorMarginY)
        .arg(quickToolSeparatorMarginX));

    viewQuickToolBar_->addAction(fitSceneAction_);
    viewQuickToolBar_->addAction(topViewAction_);
    viewQuickToolBar_->addAction(frontViewAction_);
    viewQuickToolBar_->addAction(rightViewAction_);
    viewQuickToolBar_->addSeparator();
    viewQuickToolBar_->addAction(showAxesAction_);
    viewQuickToolBar_->addAction(showBoundingBoxAction_);
    viewQuickToolBar_->addSeparator();
    viewQuickToolBar_->addAction(darkBackgroundAction_);
    viewQuickToolBar_->addAction(lightBackgroundAction_);

    addToolBar(Qt::LeftToolBarArea, viewQuickToolBar_);
}

void MainWindow::createWindowControls()
{
    if (ribbonBar_ == nullptr) {
        return;
    }

    windowControlsWidget_ = new QWidget(ribbonBar_);
    auto* controlsLayout = new QHBoxLayout(windowControlsWidget_);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);

    minimizeButton_ = new QToolButton(windowControlsWidget_);
    maximizeButton_ = new QToolButton(windowControlsWidget_);
    closeButton_ = new QToolButton(windowControlsWidget_);

    const QList<QToolButton*> buttons = { minimizeButton_, maximizeButton_, closeButton_ };
    for (QToolButton* button : buttons) {
        button->setAutoRaise(true);
        button->setCursor(Qt::ArrowCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(QSize(12, 12));
        button->setFixedSize(34, 26);
        controlsLayout->addWidget(button);
    }

    minimizeButton_->setObjectName(QStringLiteral("windowMinimizeButton"));
    maximizeButton_->setObjectName(QStringLiteral("windowMaximizeButton"));
    closeButton_->setObjectName(QStringLiteral("windowCloseButton"));

    connect(minimizeButton_, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(maximizeButton_, &QToolButton::clicked, this, [this]() { toggleMaximizedWindow(); });
    connect(closeButton_, &QToolButton::clicked, this, &QWidget::close);

    ribbonBar_->setCornerWidget(windowControlsWidget_, Qt::TopRightCorner);
    updateWindowControlButtons();
}

void MainWindow::createProjectDock()
{
    projectDock_ = new ProjectExplorerDock(this);
    projectExplorerController_ = new ProjectExplorerController(
        projectDock_,
        openAction_,
        addPointCloudAction_,
        removeDatasetAction_,
        locateDatasetAction_,
        copyDatasetPathAction_,
        expandProjectTreeAction_,
        collapseProjectTreeAction_,
        this);
    projectSearchEdit_ = projectExplorerController_->searchEdit();
    projectToolBar_ = projectExplorerController_->toolBar();
    projectTreeWidget_ = projectExplorerController_->treeWidget();
    addDockWidget(Qt::LeftDockWidgetArea, projectDock_);
}

void MainWindow::createInspectorPanel()
{
    inspectorDock_ = new SceneInspectorDock(this);
    inspectorTabWidget_ = inspectorDock_->tabWidget();

    const auto overviewTab = qMakePair(inspectorDock_->overviewScrollArea(), inspectorDock_->overviewLayout());
    const auto towerTab = qMakePair(inspectorDock_->towerScrollArea(), inspectorDock_->towerLayout());
    const auto issueTab = qMakePair(inspectorDock_->issueScrollArea(), inspectorDock_->issueLayout());
    const auto renderingTab = qMakePair(inspectorDock_->renderingScrollArea(), inspectorDock_->renderingLayout());
    const auto measurementTab = qMakePair(inspectorDock_->measurementScrollArea(), inspectorDock_->measurementLayout());
    const auto analysisTab = qMakePair(inspectorDock_->analysisScrollArea(), inspectorDock_->analysisLayout());
    const auto navigationTab = qMakePair(inspectorDock_->navigationScrollArea(), inspectorDock_->navigationLayout());

    datasetSummaryWidget_ = new DatasetSummaryWidget(overviewTab.first);
    datasetGroupBox_ = datasetSummaryWidget_;
    datasetLayout_ = datasetSummaryWidget_->datasetLayout();
    datasetNameValueLabel_ = datasetSummaryWidget_->datasetNameValueLabel();
    datasetPathValueLabel_ = datasetSummaryWidget_->datasetPathValueLabel();
    datasetPointsValueLabel_ = datasetSummaryWidget_->datasetPointsValueLabel();
    datasetBoundsValueLabel_ = datasetSummaryWidget_->datasetBoundsValueLabel();
    datasetExtentValueLabel_ = datasetSummaryWidget_->datasetExtentValueLabel();
    datasetColorValueLabel_ = datasetSummaryWidget_->datasetColorValueLabel();

    towerEditorWidget_ = new TowerEditorWidget(towerTab.first);
    towerToolBar_ = towerEditorWidget_->toolBar();
    towerCountValueLabel_ = towerEditorWidget_->towerCountLabel();
    towerToolStatusLabel_ = towerEditorWidget_->towerToolStatusLabel();
    towerTableWidget_ = towerEditorWidget_->towerTable();
    towerDetailsGroupBox_ = towerEditorWidget_->towerDetailsGroupBox();
    towerDetailsLayout_ = towerEditorWidget_->towerDetailsLayout();
    towerCodeEdit_ = towerEditorWidget_->towerCodeEdit();
    towerLineNameEdit_ = towerEditorWidget_->towerLineNameEdit();
    towerVoltageLevelEdit_ = towerEditorWidget_->towerVoltageLevelEdit();
    towerTypeComboBox_ = towerEditorWidget_->towerTypeComboBox();
    towerStructureTypeEdit_ = towerEditorWidget_->towerStructureTypeEdit();
    towerInspectionDateEdit_ = towerEditorWidget_->towerInspectionDateEdit();
    towerStatusEdit_ = towerEditorWidget_->towerStatusEdit();
    towerNotesEdit_ = towerEditorWidget_->towerNotesEdit();

    if (towerToolBar_ != nullptr) {
        towerToolBar_->addAction(addTowerAction_);
        towerToolBar_->addAction(insertTowerAction_);
        towerToolBar_->addAction(moveTowerAction_);
        towerToolBar_->addAction(editCurrentTowerAction_);
        towerToolBar_->addAction(focusTowerAction_);
        towerToolBar_->addAction(removeTowerAction_);
        towerToolBar_->addSeparator();
        towerToolBar_->addAction(importTowerFileAction_);
        towerToolBar_->addAction(saveTowerFileAction_);
        towerToolBar_->addAction(saveTowerFileAsAction_);
        towerToolBar_->addAction(reloadTowerFileAction_);
    }

    if (towerTableWidget_ != nullptr) {
        towerTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Name"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") });
    }

    if (towerTypeComboBox_ != nullptr) {
        towerTypeComboBox_->addItem(QString(), static_cast<int>(TowerType::Unknown));
        towerTypeComboBox_->addItem(QString(), static_cast<int>(TowerType::Tangent));
        towerTypeComboBox_->addItem(QString(), static_cast<int>(TowerType::Strain));
    }

    issueEditorWidget_ = new IssueEditorWidget(issueTab.first);
    issueToolBar_ = issueEditorWidget_->toolBar();
    issueMenuButton_ = issueEditorWidget_->menuButton();
    issueCountValueLabel_ = issueEditorWidget_->issueCountLabel();
    issueToolStatusLabel_ = issueEditorWidget_->issueToolStatusLabel();
    issueTableWidget_ = issueEditorWidget_->issueTable();
    issueDetailsGroupBox_ = issueEditorWidget_->issueDetailsGroupBox();
    issueDetailsLayout_ = issueEditorWidget_->issueDetailsLayout();
    issueTitleEdit_ = issueEditorWidget_->issueTitleEdit();
    issueCategoryComboBox_ = issueEditorWidget_->issueCategoryComboBox();
    issueSeverityComboBox_ = issueEditorWidget_->issueSeverityComboBox();
    issueStatusComboBox_ = issueEditorWidget_->issueStatusComboBox();
    issueRelatedTowerComboBox_ = issueEditorWidget_->issueRelatedTowerComboBox();
    issueImagePathEdit_ = issueEditorWidget_->issueImagePathEdit();
    issueLocationValueLabel_ = issueEditorWidget_->issueLocationValueLabel();
    issueCreatedAtValueLabel_ = issueEditorWidget_->issueCreatedAtValueLabel();
    issueDescriptionEdit_ = issueEditorWidget_->issueDescriptionEdit();

    if (issueToolBar_ != nullptr) {
        issueToolBar_->addAction(startIssueMarkAction_);
        issueToolBar_->addAction(cancelIssueToolAction_);
        issueToolBar_->addSeparator();
        issueToolBar_->addAction(focusIssueAction_);
        issueToolBar_->addAction(removeIssueAction_);
        issueToolBar_->addAction(clearIssuesAction_);
        issueToolBar_->addSeparator();
        issueToolBar_->addAction(exportIssuesCsvAction_);
        issueToolBar_->addAction(exportInspectionReportAction_);
    }

    issueActionsMenu_ = new QMenu(issueEditorWidget_);
    issueActionsMenu_->addAction(startIssueMarkAction_);
    issueActionsMenu_->addAction(cancelIssueToolAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(focusIssueAction_);
    issueActionsMenu_->addAction(removeIssueAction_);
    issueActionsMenu_->addAction(clearIssuesAction_);
    issueActionsMenu_->addSeparator();
    issueActionsMenu_->addAction(exportIssuesCsvAction_);
    issueActionsMenu_->addAction(exportInspectionReportAction_);
    if (issueMenuButton_ != nullptr) {
        issueMenuButton_->setMenu(issueActionsMenu_);
    }

    if (issueTableWidget_ != nullptr) {
        issueTableWidget_->setHorizontalHeaderLabels({ tr("Index"), tr("Title"), tr("Severity"), tr("Status"), tr("Tower"), tr("Category") });
    }

    if (issueCategoryComboBox_ != nullptr) {
        issueCategoryComboBox_->addItems({ tr("Vegetation"), tr("Insulator"), tr("Tower Body"), tr("Channel Risk"), tr("Other") });
    }
    if (issueSeverityComboBox_ != nullptr) {
        issueSeverityComboBox_->addItems({
            issueSeverityDisplayName(IssueSeverity::Info),
            issueSeverityDisplayName(IssueSeverity::Minor),
            issueSeverityDisplayName(IssueSeverity::Major),
            issueSeverityDisplayName(IssueSeverity::Critical)
        });
    }
    if (issueStatusComboBox_ != nullptr) {
        issueStatusComboBox_->addItems({
            issueStatusDisplayName(IssueStatus::Open),
            issueStatusDisplayName(IssueStatus::Monitoring),
            issueStatusDisplayName(IssueStatus::Resolved)
        });
    }

    renderingGroupBox_ = new QGroupBox(tr("Rendering Controls"), renderingTab.first);
    renderingLayout_ = new QFormLayout(renderingGroupBox_);
    renderingLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    renderingLayout_->setFormAlignment(Qt::AlignTop);

    pointSizeControl_ = createSliderControl(pointSizeSlider_, pointSizeValueLabel_, 1, 20, 1);
    pointOpacityControl_ = createSliderControl(pointOpacitySlider_, pointOpacityValueLabel_, 10, 100, 5);
    depthCueControl_ = createSliderControl(depthCueSlider_, depthCueValueLabel_, 0, 100, 5);
    edlStrengthControl_ = createSliderControl(edlStrengthSlider_, edlStrengthValueLabel_, 0, 100, 5);

    colorModeComboBox_ = new QComboBox(renderingGroupBox_);
    colorModeComboBox_->addItem(tr("RGB"));
    colorModeComboBox_->addItem(tr("Elevation Ramp"));
    colorModeComboBox_->addItem(tr("Single Color"));
    colorModeComboBox_->addItem(tr("Classification"));

    pointColorButton_ = new QPushButton(tr("Pick Color"), renderingGroupBox_);
    backgroundColorButton_ = new QPushButton(tr("Pick Background"), renderingGroupBox_);

    roundSplatsCheckBox_ = new QCheckBox(tr("Round splats (survey style)"), renderingGroupBox_);
    axesCheckBox_ = new QCheckBox(tr("Show XYZ axes"), renderingGroupBox_);
    boundingBoxCheckBox_ = new QCheckBox(tr("Show bounding box"), renderingGroupBox_);

    renderingLayout_->addRow(tr("Point Size"), pointSizeControl_);
    renderingLayout_->addRow(tr("Point Opacity"), pointOpacityControl_);
    renderingLayout_->addRow(tr("Depth Cue"), depthCueControl_);
    renderingLayout_->addRow(tr("EDL-style Shading"), edlStrengthControl_);
    renderingLayout_->addRow(tr("Color Mode"), colorModeComboBox_);
    renderingLayout_->addRow(tr("Single Color"), pointColorButton_);
    renderingLayout_->addRow(tr("Background"), backgroundColorButton_);
    renderingLayout_->addRow(QString(), roundSplatsCheckBox_);
    renderingLayout_->addRow(QString(), axesCheckBox_);
    renderingLayout_->addRow(QString(), boundingBoxCheckBox_);

    classificationColorsGroupBox_ = new QGroupBox(tr("Classification Mapping"), renderingTab.first);
    auto* classificationColorsLayout = new QVBoxLayout(classificationColorsGroupBox_);
    classificationColorsLayout->setContentsMargins(12, 12, 12, 12);
    classificationColorsLayout->setSpacing(8);

    classificationColorsTableWidget_ = new QTableWidget(classificationColorsGroupBox_);
    classificationColorsTableWidget_->setColumnCount(4);
    classificationColorsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    classificationColorsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    classificationColorsTableWidget_->setAlternatingRowColors(true);
    classificationColorsTableWidget_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
    classificationColorsTableWidget_->setHorizontalHeaderLabels({ tr("Show"), tr("Class ID"), tr("Class Name"), tr("Color") });
    classificationColorsTableWidget_->verticalHeader()->setVisible(false);
    classificationColorsTableWidget_->horizontalHeader()->setStretchLastSection(false);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    classificationColorsTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    classificationColorsTableWidget_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    classificationColorsTableWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    resetClassificationColorsButton_ = new QPushButton(tr("Reset Defaults"), classificationColorsGroupBox_);
    resetClassificationColorsButton_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    auto* classificationButtonRow = new QHBoxLayout();
    classificationButtonRow->addStretch(1);
    classificationButtonRow->addWidget(resetClassificationColorsButton_);

    classificationColorsLayout->addWidget(classificationColorsTableWidget_);
    classificationColorsLayout->addLayout(classificationButtonRow);

    profileClassificationGroupBox_ = new ProfileClassificationWidget(renderingTab.first);

    auto* measurementToolbarHost = new QWidget(measurementTab.first);
    auto* measurementToolbarHostLayout = new QHBoxLayout(measurementToolbarHost);
    measurementToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    measurementToolbarHostLayout->setSpacing(8);

    measurementToolBar_ = new QToolBar(measurementToolbarHost);
    measurementToolBar_->setIconSize(QSize(16, 16));
    measurementToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    measurementToolBar_->setMovable(false);
    measurementToolBar_->setFloatable(false);
    measurementToolBar_->addAction(measureAction_);
    measurementToolBar_->addAction(showProfileDockAction_);
    measurementToolBar_->addAction(clearMeasurementAction_);
    measurementToolBar_->addSeparator();
    measurementToolBar_->addAction(exportClearanceCsvAction_);
    measurementToolbarHostLayout->addWidget(measurementToolBar_, 1);

    measurementGroupBox_ = new QGroupBox(tr("Measurement"), measurementTab.first);
    measurementLayout_ = new QFormLayout(measurementGroupBox_);
    measurementLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    measurementLayout_->setFormAlignment(Qt::AlignTop);

    measurementToggleButton_ = new QPushButton(tr("Start Measurement"), measurementGroupBox_);
    measurementClearButton_ = new QPushButton(tr("Clear Measurement"), measurementGroupBox_);
    measurementStartValueLabel_ = new QLabel(measurementGroupBox_);
    measurementEndValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDistanceValueLabel_ = new QLabel(measurementGroupBox_);
    measurementHorizontalDistanceValueLabel_ = new QLabel(measurementGroupBox_);
    measurementDeltaZValueLabel_ = new QLabel(measurementGroupBox_);
    measurementSegmentsValueLabel_ = new QLabel(measurementGroupBox_);

    const QList<QLabel*> measurementLabels = {
        measurementStartValueLabel_,
        measurementEndValueLabel_,
        measurementDistanceValueLabel_,
        measurementHorizontalDistanceValueLabel_,
        measurementDeltaZValueLabel_,
        measurementSegmentsValueLabel_
    };
    for (QLabel* label : measurementLabels) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    measurementLayout_->addRow(QString(), measurementToggleButton_);
    measurementLayout_->addRow(QString(), measurementClearButton_);
    measurementLayout_->addRow(tr("Start Point"), measurementStartValueLabel_);
    measurementLayout_->addRow(tr("End Point"), measurementEndValueLabel_);
    measurementLayout_->addRow(tr("3D Distance"), measurementDistanceValueLabel_);
    measurementLayout_->addRow(tr("Horizontal Distance"), measurementHorizontalDistanceValueLabel_);
    measurementLayout_->addRow(tr("Height Delta"), measurementDeltaZValueLabel_);
    measurementLayout_->addRow(tr("Path Segments"), measurementSegmentsValueLabel_);

    clearanceGroupBox_ = new QGroupBox(tr("Clearance Analysis"), measurementTab.first);
    clearanceLayout_ = new QFormLayout(clearanceGroupBox_);
    clearanceLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    clearanceLayout_->setFormAlignment(Qt::AlignTop);

    clearanceThresholdSpinBox_ = new QDoubleSpinBox(clearanceGroupBox_);
    clearanceThresholdSpinBox_->setRange(0.0, 1000000.0);
    clearanceThresholdSpinBox_->setDecimals(2);
    clearanceThresholdSpinBox_->setSingleStep(0.5);
    clearanceThresholdSpinBox_->setKeyboardTracking(false);

    clearanceShortestValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceWarningCountValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceStatusValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceStatusValueLabel_->setWordWrap(true);

    for (QLabel* label : { clearanceShortestValueLabel_, clearanceWarningCountValueLabel_, clearanceStatusValueLabel_ }) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    clearanceRulePresetComboBox_ = new QComboBox(clearanceGroupBox_);
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::TransmissionCorridor), static_cast<int>(ClearanceRulePreset::TransmissionCorridor));
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::DistributionCorridor), static_cast<int>(ClearanceRulePreset::DistributionCorridor));
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::StructureApproach), static_cast<int>(ClearanceRulePreset::StructureApproach));
    clearanceRulePresetComboBox_->addItem(clearanceRulePresetDisplayName(ClearanceRulePreset::Custom), static_cast<int>(ClearanceRulePreset::Custom));
    clearanceRuleBandsValueLabel_ = new QLabel(clearanceGroupBox_);
    clearanceRuleBandsValueLabel_->setWordWrap(true);
    clearanceRuleBandsValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    clearanceLayout_->addRow(tr("Rule Preset"), clearanceRulePresetComboBox_);
    clearanceLayout_->addRow(tr("Critical Threshold"), clearanceThresholdSpinBox_);
    clearanceLayout_->addRow(tr("Risk Bands"), clearanceRuleBandsValueLabel_);
    clearanceLayout_->addRow(tr("Shortest Segment"), clearanceShortestValueLabel_);
    clearanceLayout_->addRow(tr("Risk Segments"), clearanceWarningCountValueLabel_);
    clearanceLayout_->addRow(tr("Status"), clearanceStatusValueLabel_);

    clearanceSegmentsGroupBox_ = new QGroupBox(tr("Path Segment Details"), measurementTab.first);
    auto* clearanceSegmentsLayout = new QVBoxLayout(clearanceSegmentsGroupBox_);
    clearanceSegmentsLayout->setContentsMargins(12, 12, 12, 12);
    clearanceSegmentsLayout->setSpacing(8);

    clearanceSegmentsSummaryLabel_ = new QLabel(clearanceSegmentsGroupBox_);
    clearanceSegmentsSummaryLabel_->setWordWrap(true);
    clearanceSegmentsSummaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    clearanceSegmentsTableWidget_ = new QTableWidget(clearanceSegmentsGroupBox_);
    clearanceSegmentsTableWidget_->setColumnCount(8);
    clearanceSegmentsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    clearanceSegmentsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    clearanceSegmentsTableWidget_->setAlternatingRowColors(true);
    clearanceSegmentsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
    clearanceSegmentsTableWidget_->verticalHeader()->setVisible(false);
    clearanceSegmentsTableWidget_->horizontalHeader()->setStretchLastSection(false);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    clearanceSegmentsTableWidget_->setMinimumHeight(220);

    clearanceSegmentsLayout->addWidget(clearanceSegmentsSummaryLabel_);
    clearanceSegmentsLayout->addWidget(clearanceSegmentsTableWidget_, 1);

    auto* analysisToolbarHost = new QWidget(analysisTab.first);
    auto* analysisToolbarHostLayout = new QHBoxLayout(analysisToolbarHost);
    analysisToolbarHostLayout->setContentsMargins(0, 0, 0, 0);
    analysisToolbarHostLayout->setSpacing(8);

    analysisToolBar_ = new QToolBar(analysisToolbarHost);
    analysisToolBar_->setIconSize(QSize(16, 16));
    analysisToolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    analysisToolBar_->setMovable(false);
    analysisToolBar_->setFloatable(false);
    analysisToolBar_->addAction(analyzeVegetationRisksAction_);
    analysisToolBar_->addAction(focusVegetationRiskAction_);
    analysisToolBar_->addSeparator();
    analysisToolBar_->addAction(createIssueFromRiskAction_);
    analysisToolBar_->addAction(createIssuesFromRisksAction_);
    analysisToolBar_->addAction(clearVegetationRisksAction_);
    analysisToolbarHostLayout->addWidget(analysisToolBar_, 1);

    analysisParametersGroupBox_ = new QGroupBox(tr("Vegetation Risk Analysis"), analysisTab.first);
    analysisParametersLayout_ = new QFormLayout(analysisParametersGroupBox_);
    analysisParametersLayout_->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    analysisParametersLayout_->setFormAlignment(Qt::AlignTop);

    vegetationSearchRadiusSpinBox_ = new QDoubleSpinBox(analysisParametersGroupBox_);
    vegetationSearchRadiusSpinBox_->setRange(1.0, 1000000.0);
    vegetationSearchRadiusSpinBox_->setDecimals(2);
    vegetationSearchRadiusSpinBox_->setSingleStep(1.0);
    vegetationSearchRadiusSpinBox_->setKeyboardTracking(false);

    vegetationClusterGapSpinBox_ = new QDoubleSpinBox(analysisParametersGroupBox_);
    vegetationClusterGapSpinBox_->setRange(0.5, 1000000.0);
    vegetationClusterGapSpinBox_->setDecimals(2);
    vegetationClusterGapSpinBox_->setSingleStep(0.5);
    vegetationClusterGapSpinBox_->setKeyboardTracking(false);

    vegetationClusterPointCountSpinBox_ = new QSpinBox(analysisParametersGroupBox_);
    vegetationClusterPointCountSpinBox_->setRange(1, 999999);

    preferVegetationClassificationCheckBox_ = new QCheckBox(tr("Prefer LAS vegetation classifications when available"), analysisParametersGroupBox_);

    vegetationRiskCountValueLabel_ = new QLabel(analysisParametersGroupBox_);
    vegetationRiskCountValueLabel_->setWordWrap(true);
    vegetationRiskStatusValueLabel_ = new QLabel(analysisParametersGroupBox_);
    vegetationRiskStatusValueLabel_->setWordWrap(true);
    vegetationRiskSummaryLabel_ = new QLabel(analysisParametersGroupBox_);
    vegetationRiskSummaryLabel_->setWordWrap(true);
    vegetationRiskSummaryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    analysisParametersLayout_->addRow(tr("Search Radius"), vegetationSearchRadiusSpinBox_);
    analysisParametersLayout_->addRow(tr("Cluster Gap"), vegetationClusterGapSpinBox_);
    analysisParametersLayout_->addRow(tr("Min Cluster Points"), vegetationClusterPointCountSpinBox_);
    analysisParametersLayout_->addRow(QString(), preferVegetationClassificationCheckBox_);
    analysisParametersLayout_->addRow(tr("Risk Count"), vegetationRiskCountValueLabel_);
    analysisParametersLayout_->addRow(tr("Status"), vegetationRiskStatusValueLabel_);
    analysisParametersLayout_->addRow(tr("Summary"), vegetationRiskSummaryLabel_);

    vegetationRisksGroupBox_ = new QGroupBox(tr("Detected Risk Clusters"), analysisTab.first);
    auto* vegetationRisksLayout = new QVBoxLayout(vegetationRisksGroupBox_);
    vegetationRisksLayout->setContentsMargins(12, 12, 12, 12);
    vegetationRisksLayout->setSpacing(8);

    vegetationRisksTableWidget_ = new QTableWidget(vegetationRisksGroupBox_);
    vegetationRisksTableWidget_->setColumnCount(7);
    vegetationRisksTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    vegetationRisksTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    vegetationRisksTableWidget_->setAlternatingRowColors(true);
    vegetationRisksTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vegetationRisksTableWidget_->setHorizontalHeaderLabels({
        tr("Index"),
        tr("Title"),
        tr("Severity"),
        tr("Min Distance"),
        tr("Chainage"),
        tr("Tower"),
        tr("Points")
    });
    vegetationRisksTableWidget_->verticalHeader()->setVisible(false);
    vegetationRisksTableWidget_->horizontalHeader()->setStretchLastSection(false);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    vegetationRisksTableWidget_->setMinimumHeight(220);
    vegetationRisksLayout->addWidget(vegetationRisksTableWidget_, 1);

    routePlanningGroupBox_ = new QGroupBox(tr("Inspection Route Planning"), analysisTab.first);
    auto* routePlanningLayout = new QFormLayout(routePlanningGroupBox_);
    routePlanningLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    routePlanningLayout->setFormAlignment(Qt::AlignTop);

    aircraftProfileComboBox_ = new QComboBox(routePlanningGroupBox_);
    for (const DjiAircraftProfile profile : supportedDjiAircraftProfiles()) {
        aircraftProfileComboBox_->addItem(djiAircraftProfileDisplayName(profile), static_cast<int>(profile));
    }
    {
        const int profileIndex = aircraftProfileComboBox_->findData(static_cast<int>(routePlanningOptions_.aircraftProfile));
        aircraftProfileComboBox_->setCurrentIndex(profileIndex >= 0 ? profileIndex : 0);
    }

    routeSafetyHeightSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeSafetyHeightSpinBox_->setRange(1.0, 1500.0);
    routeSafetyHeightSpinBox_->setDecimals(2);
    routeSafetyHeightSpinBox_->setValue(routePlanningOptions_.safety.safetyHeightMeters);

    routeWaypointSpeedSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeWaypointSpeedSpinBox_->setRange(0.5, 25.0);
    routeWaypointSpeedSpinBox_->setDecimals(2);
    routeWaypointSpeedSpinBox_->setValue(routePlanningOptions_.safety.defaultWaypointSpeedMps);

    routeWaypointSpacingSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeWaypointSpacingSpinBox_->setRange(1.0, 1000.0);
    routeWaypointSpacingSpinBox_->setDecimals(2);
    routeWaypointSpacingSpinBox_->setValue(routePlanningOptions_.generation.waypointSpacingMeters);

    routeSmoothingStrengthSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeSmoothingStrengthSpinBox_->setRange(0.0, 100.0);
    routeSmoothingStrengthSpinBox_->setDecimals(1);
    routeSmoothingStrengthSpinBox_->setValue(routePlanningOptions_.generation.smoothingStrengthPercent);

    routeHeightOffsetSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeHeightOffsetSpinBox_->setRange(0.0, 500.0);
    routeHeightOffsetSpinBox_->setDecimals(2);
    routeHeightOffsetSpinBox_->setValue(routePlanningOptions_.safety.heightOffsetMeters);

    routeRoamSpeedSpinBox_ = new QDoubleSpinBox(routePlanningGroupBox_);
    routeRoamSpeedSpinBox_->setRange(0.1, 80.0);
    routeRoamSpeedSpinBox_->setDecimals(1);
    routeRoamSpeedSpinBox_->setValue(viewer_ != nullptr ? viewer_->inspectionRouteRoamSpeedMetersPerSecond() : 2.0);

    routeRoamViewModeComboBox_ = new QComboBox(routePlanningGroupBox_);
    routeRoamViewModeComboBox_->addItem(tr("Third Person"), static_cast<int>(RouteRoamViewMode::ThirdPerson));
    routeRoamViewModeComboBox_->addItem(tr("First Person"), static_cast<int>(RouteRoamViewMode::FirstPerson));

    routeRoamControlsRow_ = new QWidget(routePlanningGroupBox_);
    auto* routeRoamButtonLayout = new QHBoxLayout(routeRoamControlsRow_);
    routeRoamButtonLayout->setContentsMargins(0, 0, 0, 0);
    routeRoamButtonLayout->setSpacing(6);
    routeRoamStartButton_ = new QPushButton(tr("Start Roam"), routeRoamControlsRow_);
    routeRoamPauseResumeButton_ = new QPushButton(tr("Pause Roam"), routeRoamControlsRow_);
    routeRoamStopButton_ = new QPushButton(tr("Stop Roam"), routeRoamControlsRow_);
    routeRoamButtonLayout->addWidget(routeRoamStartButton_);
    routeRoamButtonLayout->addWidget(routeRoamPauseResumeButton_);
    routeRoamButtonLayout->addWidget(routeRoamStopButton_);

    routeStatusValueLabel_ = new QLabel(routePlanningGroupBox_);
    routeStatusValueLabel_->setWordWrap(true);
    routeSummaryValueLabel_ = new QLabel(routePlanningGroupBox_);
    routeSummaryValueLabel_->setWordWrap(true);
    routeSummaryValueLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    routePlanningLayout->addRow(tr("DJI Profile"), aircraftProfileComboBox_);
    routePlanningLayout->addRow(tr("Safety Height"), routeSafetyHeightSpinBox_);
    routePlanningLayout->addRow(tr("Waypoint Speed"), routeWaypointSpeedSpinBox_);
    routePlanningLayout->addRow(tr("Waypoint Spacing"), routeWaypointSpacingSpinBox_);
    routePlanningLayout->addRow(tr("Smoothing"), routeSmoothingStrengthSpinBox_);
    routePlanningLayout->addRow(tr("Height Offset"), routeHeightOffsetSpinBox_);
    routePlanningLayout->addRow(tr("Roam Speed"), routeRoamSpeedSpinBox_);
    routePlanningLayout->addRow(tr("Roam View Mode"), routeRoamViewModeComboBox_);
    routePlanningLayout->addRow(tr("Roam Controls"), routeRoamControlsRow_);
    routePlanningLayout->addRow(tr("Status"), routeStatusValueLabel_);
    routePlanningLayout->addRow(tr("Summary"), routeSummaryValueLabel_);

    navigationSettingsWidget_ = new NavigationSettingsWidget(navigationTab.first);
    navigationGroupBox_ = navigationSettingsWidget_;
    navigationTipsLabel_ = navigationSettingsWidget_->tipsLabel();
    navigationToggleLayout_ = navigationSettingsWidget_->toggleLayout();
    invertOrbitCheckBox_ = navigationSettingsWidget_->invertOrbitCheckBox();
    invertPanCheckBox_ = navigationSettingsWidget_->invertPanCheckBox();
    invertWheelCheckBox_ = navigationSettingsWidget_->invertWheelCheckBox();
    wheelZoomSensitivityControl_ = navigationSettingsWidget_->wheelZoomSensitivityControl();
    wheelZoomSensitivitySlider_ = navigationSettingsWidget_->wheelZoomSensitivitySlider();
    wheelZoomSensitivityValueLabel_ = navigationSettingsWidget_->wheelZoomSensitivityValueLabel();

    overviewTab.second->addWidget(datasetSummaryWidget_);
    overviewTab.second->addStretch(1);
    towerTab.second->addWidget(towerEditorWidget_, 1);
    issueTab.second->addWidget(issueEditorWidget_, 1);
    renderingTab.second->addWidget(renderingGroupBox_);
    renderingTab.second->addWidget(classificationColorsGroupBox_);
    renderingTab.second->addStretch(1);
    measurementTab.second->addWidget(measurementToolbarHost);
    measurementTab.second->addWidget(measurementGroupBox_);
    measurementTab.second->addWidget(clearanceGroupBox_);
    measurementTab.second->addWidget(clearanceSegmentsGroupBox_);
    measurementTab.second->addStretch(1);
    analysisTab.second->addWidget(analysisToolbarHost);
    analysisTab.second->addWidget(analysisParametersGroupBox_);
    analysisTab.second->addWidget(vegetationRisksGroupBox_, 1);
    analysisTab.second->addWidget(routePlanningGroupBox_);
    analysisTab.second->addStretch(1);
    navigationTab.second->addWidget(navigationSettingsWidget_);
    navigationTab.second->addStretch(1);

    addDockWidget(Qt::RightDockWidgetArea, inspectorDock_);
}

void MainWindow::createRouteDetailsDock()
{
    routeDetailsDock_ = new RouteDetailsDock(this);
    routeDetailsTabWidget_ = routeDetailsDock_->tabWidget();
    auto* waypointsTabLayout = routeDetailsDock_->waypointsLayout();

    routeWaypointsGroupBox_ = new QGroupBox(tr("Route Waypoints"), routeDetailsTabWidget_);
    auto* routeWaypointsLayout = new QVBoxLayout(routeWaypointsGroupBox_);
    routeWaypointsLayout->setContentsMargins(10, 10, 10, 10);
    routeWaypointsLayout->setSpacing(8);

    auto* routeWaypointOptionsRow = new QWidget(routeWaypointsGroupBox_);
    auto* routeWaypointOptionsLayout = new QHBoxLayout(routeWaypointOptionsRow);
    routeWaypointOptionsLayout->setContentsMargins(0, 0, 0, 0);
    routeWaypointOptionsLayout->setSpacing(8);
    routeWaypointLabelModeComboBox_ = new QComboBox(routeWaypointOptionsRow);
    routeWaypointLabelModeComboBox_->setMinimumWidth(160);
    routeWaypointLabelModeComboBox_->addItem(tr("Name"), static_cast<int>(RouteLabelDisplayMode::Name));
    routeWaypointLabelModeComboBox_->addItem(tr("Index"), static_cast<int>(RouteLabelDisplayMode::Sequence));
    routeWaypointLabelModeComboBox_->addItem(tr("Compact Name"), static_cast<int>(RouteLabelDisplayMode::CompactName));
    routeWaypointLabelModeComboBox_->addItem(tr("Compact Index"), static_cast<int>(RouteLabelDisplayMode::CompactSequence));
    routeWaypointLabelModeComboBox_->addItem(tr("Hidden"), static_cast<int>(RouteLabelDisplayMode::Hidden));
    routeWaypointLabelModeComboBox_->setCurrentIndex(0);
    routeWaypointShowCoordinatesCheckBox_ = new QCheckBox(tr("Show Coordinates"), routeWaypointOptionsRow);
    routeWaypointShowCaptureAnglesCheckBox_ = new QCheckBox(tr("Show Capture Angles"), routeWaypointOptionsRow);
    routeWaypointShowCoordinatesCheckBox_->setChecked(true);
    routeWaypointShowCaptureAnglesCheckBox_->setChecked(true);
    routeTrajectoryColorButton_ = new QPushButton(tr("Trajectory Color"), routeWaypointOptionsRow);
    routeWaypointColorButton_ = new QPushButton(tr("Waypoint Color"), routeWaypointOptionsRow);
    routeTrajectoryColorButton_->setMinimumWidth(124);
    routeWaypointColorButton_->setMinimumWidth(124);
    routeWaypointOptionsLayout->addWidget(routeWaypointLabelModeComboBox_);
    routeWaypointOptionsLayout->addWidget(routeTrajectoryColorButton_);
    routeWaypointOptionsLayout->addWidget(routeWaypointColorButton_);
    routeWaypointOptionsLayout->addStretch(1);
    routeWaypointOptionsLayout->addWidget(routeWaypointShowCoordinatesCheckBox_);
    routeWaypointOptionsLayout->addWidget(routeWaypointShowCaptureAnglesCheckBox_);

    routeWaypointsTableWidget_ = new QTableWidget(routeWaypointsGroupBox_);
    routeWaypointsTableWidget_->setColumnCount(9);
    routeWaypointsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routeWaypointsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeWaypointsTableWidget_->setAlternatingRowColors(true);
    routeWaypointsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
    routeWaypointsTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* waypointHeader = routeWaypointsTableWidget_->horizontalHeader();
    waypointHeader->setStretchLastSection(false);
    waypointHeader->setSectionResizeMode(QHeaderView::Interactive);
    routeWaypointsTableWidget_->setColumnWidth(0, 68);
    routeWaypointsTableWidget_->setColumnWidth(1, 220);
    routeWaypointsTableWidget_->setColumnWidth(2, 98);
    routeWaypointsTableWidget_->setColumnWidth(3, 98);
    routeWaypointsTableWidget_->setColumnWidth(4, 98);
    routeWaypointsTableWidget_->setColumnWidth(5, 116);
    routeWaypointsTableWidget_->setColumnWidth(6, 116);
    routeWaypointsTableWidget_->setColumnWidth(7, 116);
    routeWaypointsTableWidget_->setColumnWidth(8, 116);
    routeWaypointsTableWidget_->setStyleSheet(routeTableStyleSheet());
    routeWaypointsTableWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    routeWaypointTargetsGroupBox_ = new QGroupBox(tr("Waypoint Targets"), routeWaypointsGroupBox_);
    auto* routeWaypointTargetsLayout = new QVBoxLayout(routeWaypointTargetsGroupBox_);
    routeWaypointTargetsLayout->setContentsMargins(8, 8, 8, 8);
    routeWaypointTargetsLayout->setSpacing(6);

    routeWaypointTargetsTableWidget_ = new QTableWidget(routeWaypointTargetsGroupBox_);
    routeWaypointTargetsTableWidget_->setColumnCount(6);
    routeWaypointTargetsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routeWaypointTargetsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeWaypointTargetsTableWidget_->setAlternatingRowColors(true);
    routeWaypointTargetsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routeWaypointTargetsTableWidget_->setHorizontalHeaderLabels({
        tr("Index"),
        tr("Part"),
        tr("Focal Ratio"),
        tr("Camera Yaw"),
        tr("Camera Pitch"),
        tr("Target Point")
    });
    routeWaypointTargetsTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* targetHeader = routeWaypointTargetsTableWidget_->horizontalHeader();
    targetHeader->setStretchLastSection(false);
    targetHeader->setSectionResizeMode(QHeaderView::Interactive);
    routeWaypointTargetsTableWidget_->setColumnWidth(0, 62);
    routeWaypointTargetsTableWidget_->setColumnWidth(1, 210);
    routeWaypointTargetsTableWidget_->setColumnWidth(2, 96);
    routeWaypointTargetsTableWidget_->setColumnWidth(3, 108);
    routeWaypointTargetsTableWidget_->setColumnWidth(4, 108);
    routeWaypointTargetsTableWidget_->setColumnWidth(5, 220);
    routeWaypointTargetsTableWidget_->setMinimumHeight(170);
    routeWaypointTargetsTableWidget_->setStyleSheet(routeTableStyleSheet());
    routeWaypointTargetsTableWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    routeWaypointTargetsLayout->addWidget(routeWaypointTargetsTableWidget_);

    routeWaypointsLayout->addWidget(routeWaypointOptionsRow);
    routeWaypointsLayout->addWidget(routeWaypointsTableWidget_, 1);
    routeWaypointsLayout->addWidget(routeWaypointTargetsGroupBox_, 0);
    waypointsTabLayout->addWidget(routeWaypointsGroupBox_, 1);

    auto* partPointsTabLayout = routeDetailsDock_->partPointsLayout();

    routePartsGroupBox_ = new QGroupBox(tr("Route Part Points"), routeDetailsTabWidget_);
    auto* routePartsLayout = new QVBoxLayout(routePartsGroupBox_);
    routePartsLayout->setContentsMargins(10, 10, 10, 10);
    routePartsLayout->setSpacing(8);

    auto* routePartOptionsRow = new QWidget(routePartsGroupBox_);
    auto* routePartOptionsLayout = new QHBoxLayout(routePartOptionsRow);
    routePartOptionsLayout->setContentsMargins(0, 0, 0, 0);
    routePartOptionsLayout->setSpacing(8);
    routePartLabelModeComboBox_ = new QComboBox(routePartOptionsRow);
    routePartLabelModeComboBox_->setMinimumWidth(160);
    routePartLabelModeComboBox_->addItem(tr("Name"), static_cast<int>(RouteLabelDisplayMode::Name));
    routePartLabelModeComboBox_->addItem(tr("Index"), static_cast<int>(RouteLabelDisplayMode::Sequence));
    routePartLabelModeComboBox_->addItem(tr("Compact Name"), static_cast<int>(RouteLabelDisplayMode::CompactName));
    routePartLabelModeComboBox_->addItem(tr("Compact Index"), static_cast<int>(RouteLabelDisplayMode::CompactSequence));
    routePartLabelModeComboBox_->addItem(tr("Hidden"), static_cast<int>(RouteLabelDisplayMode::Hidden));
    routePartLabelModeComboBox_->setCurrentIndex(0);
    routePartShowCoordinatesCheckBox_ = new QCheckBox(tr("Show Coordinates"), routePartOptionsRow);
    routePartShowCaptureAnglesCheckBox_ = new QCheckBox(tr("Show Capture Angles"), routePartOptionsRow);
    routePartShowCoordinatesCheckBox_->setChecked(true);
    routePartShowCaptureAnglesCheckBox_->setChecked(true);
    routePartPointColorButton_ = new QPushButton(tr("Part Point Color"), routePartOptionsRow);
    routePartPointColorButton_->setMinimumWidth(124);
    routePartOptionsLayout->addWidget(routePartLabelModeComboBox_);
    routePartOptionsLayout->addWidget(routePartPointColorButton_);
    routePartOptionsLayout->addStretch(1);
    routePartOptionsLayout->addWidget(routePartShowCoordinatesCheckBox_);
    routePartOptionsLayout->addWidget(routePartShowCaptureAnglesCheckBox_);

    routePartPointsTableWidget_ = new QTableWidget(routePartsGroupBox_);
    routePartPointsTableWidget_->setColumnCount(8);
    routePartPointsTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routePartPointsTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routePartPointsTableWidget_->setAlternatingRowColors(true);
    routePartPointsTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
    routePartPointsTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* routePartHeader = routePartPointsTableWidget_->horizontalHeader();
    routePartHeader->setStretchLastSection(false);
    routePartHeader->setSectionResizeMode(QHeaderView::Interactive);
    routePartPointsTableWidget_->setColumnWidth(0, 68);
    routePartPointsTableWidget_->setColumnWidth(1, 180);
    routePartPointsTableWidget_->setColumnWidth(2, 122);
    routePartPointsTableWidget_->setColumnWidth(3, 108);
    routePartPointsTableWidget_->setColumnWidth(4, 108);
    routePartPointsTableWidget_->setColumnWidth(5, 98);
    routePartPointsTableWidget_->setColumnWidth(6, 98);
    routePartPointsTableWidget_->setColumnWidth(7, 98);
    routePartPointsTableWidget_->setStyleSheet(routeTableStyleSheet());
    routePartPointsTableWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    routePartsLayout->addWidget(routePartOptionsRow);
    routePartsLayout->addWidget(routePartPointsTableWidget_, 1);
    partPointsTabLayout->addWidget(routePartsGroupBox_, 1);

    auto* routeQaTabLayout = routeDetailsDock_->routeQaLayout();

    routeQaGroupBox_ = new QGroupBox(tr("Route QA"), routeDetailsTabWidget_);
    auto* routeQaLayout = new QVBoxLayout(routeQaGroupBox_);
    routeQaLayout->setContentsMargins(10, 10, 10, 10);
    routeQaLayout->setSpacing(8);

    routeQaSummaryValueLabel_ = new QLabel(tr("Route QA will run automatically after route updates."), routeQaGroupBox_);
    routeQaSummaryValueLabel_->setWordWrap(true);
    routeQaSummaryValueLabel_->setStyleSheet(QStringLiteral("color: #166534; font-weight: 600;"));

    routeQaIssuesTableWidget_ = new QTableWidget(routeQaGroupBox_);
    routeQaIssuesTableWidget_->setColumnCount(5);
    routeQaIssuesTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    routeQaIssuesTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeQaIssuesTableWidget_->setAlternatingRowColors(true);
    routeQaIssuesTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    routeQaIssuesTableWidget_->setHorizontalHeaderLabels({
        tr("Severity"),
        tr("Issue"),
        tr("Location"),
        tr("Part"),
        tr("Description")
    });
    routeQaIssuesTableWidget_->verticalHeader()->setVisible(false);
    QHeaderView* routeQaHeader = routeQaIssuesTableWidget_->horizontalHeader();
    routeQaHeader->setStretchLastSection(false);
    routeQaHeader->setSectionResizeMode(QHeaderView::Interactive);
    routeQaIssuesTableWidget_->setColumnWidth(kRouteQaColumnSeverity, 96);
    routeQaIssuesTableWidget_->setColumnWidth(kRouteQaColumnType, 140);
    routeQaIssuesTableWidget_->setColumnWidth(kRouteQaColumnLocation, 120);
    routeQaIssuesTableWidget_->setColumnWidth(kRouteQaColumnPart, 170);
    routeQaIssuesTableWidget_->setColumnWidth(kRouteQaColumnDescription, 360);
    routeQaIssuesTableWidget_->setStyleSheet(routeTableStyleSheet());
    routeQaIssuesTableWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    routeQaLayout->addWidget(routeQaSummaryValueLabel_);
    routeQaLayout->addWidget(routeQaIssuesTableWidget_, 1);
    routeQaTabLayout->addWidget(routeQaGroupBox_, 1);

    if (viewer_ != nullptr) {
        setColorButtonAppearance(routeWaypointColorButton_, viewer_->inspectionRouteWaypointColor(), tr("Waypoint Color"));
        setColorButtonAppearance(routePartPointColorButton_, viewer_->inspectionRoutePartPointColor(), tr("Part Point Color"));
        setColorButtonAppearance(routeTrajectoryColorButton_, viewer_->inspectionRouteTrajectoryColor(), tr("Trajectory Color"));
    }
    applyRouteWaypointTableColumnVisibility();
    applyRoutePartTableColumnVisibility();

    addDockWidget(Qt::RightDockWidgetArea, routeDetailsDock_);
    if (inspectorDock_ != nullptr) {
        tabifyDockWidget(inspectorDock_, routeDetailsDock_);
    }
    routeDetailsDock_->hide();
}

void MainWindow::createProfileClassificationDock()
{
    profileClassificationDock_ = new ProfileClassificationDock(this);
    profileClassificationDock_->setContentWidget(profileClassificationGroupBox_);

    addDockWidget(Qt::LeftDockWidgetArea, profileClassificationDock_);
    if (projectDock_ != nullptr) {
        tabifyDockWidget(projectDock_, profileClassificationDock_);
    }
    profileClassificationDock_->hide();
}

void MainWindow::createProfileDock()
{
    profileDock_ = new SpanProfileDock(this);
    profilePlotWidget_ = profileDock_->plotWidget();
    addDockWidget(Qt::BottomDockWidgetArea, profileDock_);
    profileDock_->hide();
}

void MainWindow::createLogDock()
{
    logDock_ = new ApplicationLogDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, logDock_);
    logDock_->hide();
}

void MainWindow::createStatusBar()
{
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet(QStringLiteral(
        "QStatusBar {"
        "background-color: #eef2f7;"
        "color: #334155;"
        "border-top: 1px solid #d6dde8;"
        "}"));

    globalProgressBar_ = new QProgressBar(this);
    globalProgressBar_->setObjectName(QStringLiteral("globalOperationProgress"));
    globalProgressBar_->setRange(0, 1000);
    globalProgressBar_->setValue(0);
    globalProgressBar_->setTextVisible(true);
    globalProgressBar_->setFormat(QStringLiteral("%p%"));
    globalProgressBar_->setFixedWidth(220);
    globalProgressBar_->setVisible(false);
    globalProgressBar_->setStyleSheet(QStringLiteral(
        "QProgressBar#globalOperationProgress {"
        "background: #dbe4ef;"
        "color: #0f172a;"
        "border: 1px solid #c7d2e2;"
        "border-radius: 7px;"
        "text-align: center;"
        "padding: 1px;"
        "font-size: 11px;"
        "font-weight: 600;"
        "}"
        "QProgressBar#globalOperationProgress::chunk {"
        "background: #2563eb;"
        "border-radius: 6px;"
        "}"));
    statusBar()->addPermanentWidget(globalProgressBar_);
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
    addPointCloudAction_->setText(tr("Add LAS Files"));
    addPointCloudAction_->setToolTip(tr("Add one or more LAS or LAZ datasets to the current project"));
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
    projectCoordinateSystemsAction_->setText(tr("Project Properties"));
    projectCoordinateSystemsAction_->setToolTip(tr("Open project coordinate system settings"));
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
        backstageProjectPropertiesPage_->setWindowTitle(tr("Project Properties"));
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
        backstageSaveAction_->setIcon(saveProjectAction_->icon());
    }
    if (backstageSaveAsAction_ != nullptr) {
        backstageSaveAsAction_->setText(saveProjectAsAction_->text());
        backstageSaveAsAction_->setIcon(saveProjectAsAction_->icon());
    }
    if (backstageProjectPropertiesPageAction_ != nullptr) {
        backstageProjectPropertiesPageAction_->setText(tr("Project Properties"));
    }
    if (backstageApplicationSettingsPageAction_ != nullptr) {
        backstageApplicationSettingsPageAction_->setText(tr("Application Settings"));
    }
    if (backstageAboutPageAction_ != nullptr) {
        backstageAboutPageAction_->setText(tr("About"));
    }
    if (backstageExitAction_ != nullptr) {
        backstageExitAction_->setText(exitAction_->text());
        backstageExitAction_->setIcon(exitAction_->icon());
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
        backstageProjectPropertiesTitleLabel_->setText(tr("Project Properties"));
    }
    if (backstageProjectPropertiesSubtitleLabel_ != nullptr) {
        backstageProjectPropertiesSubtitleLabel_->setText(tr("Review the active project file and coordinate system configuration."));
    }
    if (backstageProjectPropertiesWidget_ != nullptr) {
        backstageProjectPropertiesWidget_->retranslateUi();
    }
    if (backstageApplicationSettingsTitleLabel_ != nullptr) {
        backstageApplicationSettingsTitleLabel_->setText(tr("Application Settings"));
    }
    if (backstageApplicationSettingsSubtitleLabel_ != nullptr) {
        backstageApplicationSettingsSubtitleLabel_->setText(tr("Adjust the office theme, interface language, and workspace panels."));
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
    if (backstageOpenProjectWidget_ != nullptr) {
        if (QGroupBox* recentProjectsGroup = backstageOpenProjectWidget_->recentProjectsGroup()) {
            recentProjectsGroup->setTitle(tr("Recent Projects"));
        }
        if (QGroupBox* projectFileGroup = backstageOpenProjectWidget_->projectFileGroup()) {
            projectFileGroup->setTitle(tr("Project File"));
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
    if (analysisPage_ != nullptr) {
        analysisPage_->setTitle(tr("Analysis"));
    }
    if (issuePage_ != nullptr) {
        issuePage_->setTitle(tr("Issue"));
    }
    if (towerPage_ != nullptr) {
        towerPage_->setTitle(tr("Tower"));
    }
    if (appearancePage_ != nullptr) {
        appearancePage_->setTitle(tr("Appearance"));
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
    if (measureRibbonGroup_ != nullptr) {
        measureRibbonGroup_->setTitle(tr("Measure"));
    }
    if (classificationRibbonGroup_ != nullptr) {
        classificationRibbonGroup_->setTitle(tr("Classification"));
    }
    if (vegetationRiskRibbonGroup_ != nullptr) {
        vegetationRiskRibbonGroup_->setTitle(tr("Vegetation Risks"));
    }
    if (routePlanningRibbonGroup_ != nullptr) {
        routePlanningRibbonGroup_->setTitle(tr("Route Planning"));
    }
    if (routeRoamRibbonGroup_ != nullptr) {
        routeRoamRibbonGroup_->setTitle(tr("Route Roam"));
    }
    if (routeFileRibbonGroup_ != nullptr) {
        routeFileRibbonGroup_->setTitle(tr("Route Files"));
    }
    if (routeExchangeRibbonGroup_ != nullptr) {
        routeExchangeRibbonGroup_->setTitle(tr("Route Exchange"));
    }
    if (issueRibbonGroup_ != nullptr) {
        issueRibbonGroup_->setTitle(tr("Inspection Issues"));
    }
    if (workspaceRibbonGroup_ != nullptr) {
        workspaceRibbonGroup_->setTitle(tr("Workspace"));
    }
    if (towerRibbonGroup_ != nullptr) {
        towerRibbonGroup_->setTitle(tr("Tower Editing"));
    }
    if (colorRibbonGroup_ != nullptr) {
        colorRibbonGroup_->setTitle(tr("Point Colors"));
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

void MainWindow::updateClassificationColorTable()
{
    if (classificationColorsTableWidget_ == nullptr || viewer_ == nullptr) {
        return;
    }

    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    const QSignalBlocker blocker(classificationColorsTableWidget_);
    updatingClassificationColorTable_ = true;

    QList<int> classificationCodes;
    classificationCodes.reserve(static_cast<int>(kClassificationDisplayItems.size()) + options.classificationColors.size());
    std::set<int> seenCodes;
    for (const ClassificationDisplayItem& item : kClassificationDisplayItems) {
        if (item.code >= 0) {
            classificationCodes.append(item.code);
            seenCodes.insert(item.code);
        }
    }

    const auto appendConfiguredCode = [&classificationCodes, &seenCodes](int code) {
        if (code < 0 || code > 255 || seenCodes.count(code) > 0) {
            return;
        }
        classificationCodes.append(code);
        seenCodes.insert(code);
    };
    for (auto it = options.classificationColors.constBegin(); it != options.classificationColors.constEnd(); ++it) {
        appendConfiguredCode(it.key());
    }
    for (auto it = options.classificationVisibility.constBegin(); it != options.classificationVisibility.constEnd(); ++it) {
        appendConfiguredCode(it.key());
    }
    for (auto it = classificationNameOverrides_.constBegin(); it != classificationNameOverrides_.constEnd(); ++it) {
        appendConfiguredCode(it.key());
    }
    classificationCodes.append(-1);

    classificationColorsTableWidget_->setRowCount(classificationCodes.size());
    for (int row = 0; row < classificationCodes.size(); ++row) {
        const int classificationCode = classificationCodes.at(row);
        const bool visible = options.classificationVisibility.value(
            classificationCode,
            options.classificationVisibility.value(-1, true));
        const QColor color = classificationCode < 0
            ? options.classificationFallbackColor
            : options.classificationColors.value(classificationCode, options.classificationFallbackColor);

        auto* visibleItem = classificationColorsTableWidget_->item(row, 0);
        if (visibleItem == nullptr) {
            visibleItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 0, visibleItem);
        }
        visibleItem->setFlags((visibleItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable) & ~Qt::ItemIsEditable);
        visibleItem->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
        visibleItem->setText(QString());
        visibleItem->setTextAlignment(Qt::AlignCenter);
        visibleItem->setData(Qt::UserRole, classificationCode);

        auto* classItem = classificationColorsTableWidget_->item(row, 1);
        if (classItem == nullptr) {
            classItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 1, classItem);
        }
        classItem->setFlags((classItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        classItem->setText(classificationCode < 0 ? tr("Other") : QLocale().toString(classificationCode));
        classItem->setTextAlignment(Qt::AlignCenter);
        classItem->setData(Qt::UserRole, classificationCode);

        auto* nameItem = classificationColorsTableWidget_->item(row, 2);
        if (nameItem == nullptr) {
            nameItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 2, nameItem);
        }
        nameItem->setFlags(nameItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
        nameItem->setText(classificationDisplayName(classificationCode, classificationNameOverrides_));
        nameItem->setData(Qt::UserRole, classificationCode);

        auto* colorItem = classificationColorsTableWidget_->item(row, 3);
        if (colorItem == nullptr) {
            colorItem = new QTableWidgetItem();
            classificationColorsTableWidget_->setItem(row, 3, colorItem);
        }
        colorItem->setFlags((colorItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        colorItem->setText(color.name(QColor::HexRgb).toUpper());
        colorItem->setTextAlignment(Qt::AlignCenter);
        colorItem->setData(Qt::UserRole, classificationCode);
        colorItem->setBackground(color);
        const int luminance = static_cast<int>(std::lround(0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue()));
        colorItem->setForeground(luminance < 140 ? QColor(248, 250, 252) : QColor(15, 23, 42));
        colorItem->setToolTip(tr("Double-click to change this class color."));
    }

    classificationColorsTableWidget_->resizeRowsToContents();
    adjustClassificationColorTableHeight();
    if (classificationColorsGroupBox_ != nullptr) {
        classificationColorsGroupBox_->setEnabled(viewer_->hasPointCloud());
    }
    if (resetClassificationColorsButton_ != nullptr) {
        resetClassificationColorsButton_->setEnabled(viewer_->hasPointCloud());
    }

    updatingClassificationColorTable_ = false;
}

void MainWindow::adjustClassificationColorTableHeight()
{
    if (classificationColorsTableWidget_ == nullptr) {
        return;
    }

    int contentHeight = classificationColorsTableWidget_->frameWidth() * 2;
    if (classificationColorsTableWidget_->horizontalHeader() != nullptr
        && !classificationColorsTableWidget_->horizontalHeader()->isHidden()) {
        contentHeight += classificationColorsTableWidget_->horizontalHeader()->height();
    }

    for (int row = 0; row < classificationColorsTableWidget_->rowCount(); ++row) {
        contentHeight += classificationColorsTableWidget_->rowHeight(row);
    }

    contentHeight += 4;
    classificationColorsTableWidget_->setMinimumHeight(contentHeight);
    classificationColorsTableWidget_->setMaximumHeight(contentHeight);
}

void MainWindow::updateProfileClassificationPanel()
{
    if (profileClassificationController_ != nullptr) {
        profileClassificationController_->refreshPanel(classificationEditsDirty_);
    }
}

bool MainWindow::saveProfileClassificationEditsToLas()
{
    if (savingProfileClassificationEdits_) {
        return false;
    }

    savingProfileClassificationEdits_ = true;
    struct SaveFlagResetGuard
    {
        bool* flag = nullptr;
        ~SaveFlagResetGuard()
        {
            if (flag != nullptr) {
                *flag = false;
            }
        }
    } saveFlagResetGuard { &savingProfileClassificationEdits_ };

    if (viewer_ == nullptr || viewer_->classificationEditedPointCount() <= 0) {
        classificationEditsDirty_ = false;
        updateProfileClassificationPanel();
        updateActionState();
        return true;
    }

#ifndef LAS_VIEWER_HAS_LASLIB
    const QString errorMessage = tr("This build does not support writing LAS/LAZ files.");
    showUserMessage(LogLevel::Error, errorMessage, 5000);
    showLightStyledMessageBox(
        this,
        QMessageBox::Warning,
        tr("Save Classification Results"),
        errorMessage,
        QMessageBox::Ok);
    return false;
#else
    const ClassificationEditStore::StoreMap editsByDataset = viewer_->classificationEditStore().editsByDataset();
    if (editsByDataset.isEmpty()) {
        classificationEditsDirty_ = false;
        updateProfileClassificationPanel();
        updateActionState();
        return true;
    }

    QHash<QString, qint64> datasetPointCounts;
    for (const PointCloudDatasetInfo& datasetInfo : viewer_->pointCloudDatasets()) {
        datasetPointCounts.insert(datasetInfo.filePath.toLower(), static_cast<qint64>(datasetInfo.pointCount));
    }

    int datasetCountToWrite = 0;
    qint64 totalPointsToWrite = 0;
    for (auto it = editsByDataset.constBegin(); it != editsByDataset.constEnd(); ++it) {
        if (it->isEmpty()) {
            continue;
        }

        ++datasetCountToWrite;
        const qint64 estimatedPointCount = datasetPointCounts.value(
            it.key().toLower(),
            static_cast<qint64>(it->size()));
        totalPointsToWrite += std::max<qint64>(estimatedPointCount, 1);
    }

    if (datasetCountToWrite <= 0) {
        classificationEditsDirty_ = false;
        updateProfileClassificationPanel();
        updateActionState();
        return true;
    }

    const int progressMaximum = static_cast<int>(std::min<qint64>(
        std::max<qint64>(totalPointsToWrite, 1),
        2000000000LL));
    qint64 processedPoints = 0;
    QStringList writtenDatasetNames;

    beginOperationProgress(tr("Saving classification results to LAS files..."));
    updateOperationProgress(tr("Preparing LAS write tasks..."), 0, progressMaximum);

    const auto failWithMessage = [this](const QString& message) -> bool {
        endOperationProgress();
        showUserMessage(LogLevel::Error, message, 7000);
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Save Classification Results"),
            message,
            QMessageBox::Ok);
        return false;
    };

    for (auto datasetIt = editsByDataset.constBegin(); datasetIt != editsByDataset.constEnd(); ++datasetIt) {
        const QString datasetPath = datasetIt.key();
        const ClassificationEditStore::DatasetEditMap& edits = datasetIt.value();
        if (edits.isEmpty()) {
            continue;
        }

        const QFileInfo datasetInfo(datasetPath);
        if (!datasetInfo.exists() || !datasetInfo.isFile()) {
            return failWithMessage(tr("Failed to save classification result: dataset file not found (%1).")
                .arg(datasetPath));
        }

        const QString suffix = datasetInfo.suffix().isEmpty() ? QStringLiteral("las") : datasetInfo.suffix();
        const QString tempFilePath = datasetInfo.absolutePath()
            + QDir::separator()
            + QStringLiteral("%1.__classify_tmp_%2.%3")
                .arg(datasetInfo.completeBaseName())
                .arg(QString::number(QDateTime::currentMSecsSinceEpoch()))
                .arg(suffix);
        const QString backupFilePath = datasetPath + QStringLiteral(".__classify_backup");
        QFile::remove(tempFilePath);
        QFile::remove(backupFilePath);

        LASreadOpener readOpener;
        const QByteArray datasetPathNative = QDir::toNativeSeparators(datasetPath).toLocal8Bit();
        readOpener.set_file_name(datasetPathNative.constData());
        std::unique_ptr<LASreader> reader(readOpener.open());
        if (!reader) {
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to open dataset for write-back (%1).")
                .arg(datasetInfo.fileName()));
        }

        LASwriteOpener writeOpener;
        const QByteArray tempFilePathNative = QDir::toNativeSeparators(tempFilePath).toLocal8Bit();
        writeOpener.set_file_name(tempFilePathNative.constData());
        std::unique_ptr<LASwriter> writer(writeOpener.open(&reader->header));
        const auto closeLasHandles = [&reader, &writer]() {
            if (writer) {
                writer->close();
                writer.reset();
            }
            if (reader) {
                reader->close();
                reader.reset();
            }
        };
        if (!writer) {
            closeLasHandles();
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to create output LAS file (%1).")
                .arg(datasetInfo.fileName()));
        }

        quint32 pointIndex = 0;
        const qint64 datasetTotalPoints = reader->npoints > 0
            ? static_cast<qint64>(reader->npoints)
            : std::max<qint64>(static_cast<qint64>(edits.size()), 1);
        while (reader->read_point()) {
            const auto editIt = edits.constFind(pointIndex);
            if (editIt != edits.constEnd()) {
                reader->point.set_classification(static_cast<U8>(std::clamp(editIt.value(), 0, 255)));
            }

            if (!writer->write_point(&reader->point)) {
                closeLasHandles();
                QFile::remove(tempFilePath);
                return failWithMessage(tr("Failed while writing classification result (%1).")
                    .arg(datasetInfo.fileName()));
            }

            ++pointIndex;
            ++processedPoints;
            if ((pointIndex & 0x1FFFu) == 0u) {
                const int progressValue = static_cast<int>(std::min<qint64>(processedPoints, progressMaximum));
                updateOperationProgress(
                    tr("Writing %1 (%2/%3 points)")
                        .arg(datasetInfo.fileName())
                        .arg(QLocale().toString(static_cast<qlonglong>(pointIndex)))
                        .arg(QLocale().toString(static_cast<qlonglong>(datasetTotalPoints))),
                    progressValue,
                    progressMaximum);
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }

        closeLasHandles();

        if (!QFile::rename(datasetPath, backupFilePath)) {
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to replace dataset while saving (%1).")
                .arg(datasetInfo.fileName()));
        }

        if (!QFile::rename(tempFilePath, datasetPath)) {
            QFile::rename(backupFilePath, datasetPath);
            QFile::remove(tempFilePath);
            return failWithMessage(tr("Failed to finalize LAS save (%1).")
                .arg(datasetInfo.fileName()));
        }
        QFile::remove(backupFilePath);

        writtenDatasetNames.append(datasetInfo.fileName());
        const int progressValue = static_cast<int>(std::min<qint64>(processedPoints, progressMaximum));
        updateOperationProgress(
            tr("Saved %1 (%2/%3 files)")
                .arg(datasetInfo.fileName())
                .arg(QLocale().toString(writtenDatasetNames.size()))
                .arg(QLocale().toString(datasetCountToWrite)),
            progressValue,
            progressMaximum);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    endOperationProgress();
    viewer_->commitClassificationEditsToPointCloudData();
    classificationEditsDirty_ = false;
    updateProfileClassificationPanel();
    updateActionState();

    const QString completionMessage = tr("Classification results were written to %1 LAS file(s).")
        .arg(QLocale().toString(writtenDatasetNames.size()));
    showUserMessage(LogLevel::Info, completionMessage, 5000);
    showLightStyledMessageBox(
        this,
        QMessageBox::Information,
        tr("Save Classification Results"),
        writtenDatasetNames.isEmpty()
            ? completionMessage
            : tr("%1\n\nSaved files: %2")
                .arg(completionMessage)
                .arg(writtenDatasetNames.join(QStringLiteral(", "))),
        QMessageBox::Ok);
    return true;
#endif
}

void MainWindow::promptSaveProfileClassificationEditsIfNeeded()
{
    if (handlingProfileClassificationExitPrompt_ || viewer_ == nullptr) {
        return;
    }

    if (!classificationEditsDirty_ || viewer_->classificationEditedPointCount() <= 0) {
        return;
    }

    handlingProfileClassificationExitPrompt_ = true;
    const QMessageBox::StandardButton choice = showLightStyledMessageBox(
        this,
        QMessageBox::Question,
        tr("Save Classification Results"),
        tr("Profile classification results are not saved. Write them to LAS files now?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);
    handlingProfileClassificationExitPrompt_ = false;

    if (choice == QMessageBox::Yes) {
        saveProfileClassificationEditsToLas();
    }
}

void MainWindow::createConnections()
{
    createControllerConnections();
    createWindowAndViewerConnections();
}

void MainWindow::createControllerConnections()
{
    if (projectExplorerController_ != nullptr) {
        connect(projectExplorerController_, &ProjectExplorerController::openRequested, this, [this]() {
            openProjectExplorerFile();
        });
        connect(projectExplorerController_, &ProjectExplorerController::addPointCloudRequested, this, [this]() {
            addPointCloudFiles();
        });
        connect(projectExplorerController_, &ProjectExplorerController::removeDatasetRequested, this, [this]() {
            removeSelectedDataset();
        });
        connect(projectExplorerController_, &ProjectExplorerController::locateSelectedRequested, this, [this]() {
            const QTreeWidgetItem* currentItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
            const QString filePath = projectTreeItemFilePath(currentItem);
            if (filePath.isEmpty()) {
                return;
            }

            const QString folderPath = QFileInfo(filePath).absolutePath();
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the selected file folder."), 3000);
            }
        });
        connect(projectExplorerController_, &ProjectExplorerController::copySelectedPathRequested, this, [this]() {
            const QTreeWidgetItem* currentItem = projectTreeWidget_ != nullptr ? projectTreeWidget_->currentItem() : nullptr;
            const QString filePath = projectTreeItemFilePath(currentItem);
            if (filePath.isEmpty()) {
                return;
            }

            if (QGuiApplication::clipboard() != nullptr) {
                QGuiApplication::clipboard()->setText(filePath);
                showUserMessage(LogLevel::Info, tr("Selected path copied."), 2000);
            }
        });
    }
    connect(openProjectAction_, &QAction::triggered, this, [this]() { openProject(); });
    connect(saveProjectAction_, &QAction::triggered, this, [this]() { saveProject(); });
    connect(saveProjectAsAction_, &QAction::triggered, this, [this]() { saveProjectAs(); });
    connect(projectCoordinateSystemsAction_, &QAction::triggered, this, [this]() { openBackstagePage(backstageProjectPropertiesPage_); });
    connect(clearAction_, &QAction::triggered, this, [this]() { clearPointCloud(); });
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);

    connect(fitSceneAction_, &QAction::triggered, viewer_, &PointCloudViewer::resetView);
    connect(topViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Top); });
    connect(frontViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Front); });
    connect(rightViewAction_, &QAction::triggered, this, [this]() { viewer_->setViewPreset(PointCloudViewPreset::Right); });

    visualizationPanelController_ = new VisualizationPanelController(
        viewer_,
        showAxesAction_,
        showBoundingBoxAction_,
        darkBackgroundAction_,
        lightBackgroundAction_,
        rgbColorAction_,
        elevationColorAction_,
        singleColorAction_,
        classificationColorAction_,
        pointSizeSlider_,
        pointSizeValueLabel_,
        pointOpacitySlider_,
        pointOpacityValueLabel_,
        depthCueSlider_,
        depthCueValueLabel_,
        edlStrengthSlider_,
        edlStrengthValueLabel_,
        colorModeComboBox_,
        pointColorButton_,
        backgroundColorButton_,
        [this]() { choosePointColor(); },
        [this]() { chooseBackgroundColor(); },
        this);

    connect(themeColorfulAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016Colorful); });
    connect(themeWhiteAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016White); });
    connect(themeDarkGrayAction_, &QAction::triggered, this, [this]() { applyOfficeTheme(Qtitan::RibbonStyle::Office2016DarkGray); });

    profileClassificationController_ = new ProfileClassificationController(
        profileClassificationGroupBox_,
        viewer_,
        profileClassificationAction_,
        saveProfileClassificationEditsAction_,
        undoProfileClassificationAction_,
        redoProfileClassificationAction_,
        clearProfileClassificationEditsAction_,
        [this](int classificationCode) {
            return classificationDisplayName(classificationCode, classificationNameOverrides_);
        },
        this);

    QList<int> profileClassificationCodes;
    profileClassificationCodes.reserve(static_cast<int>(kClassificationDisplayItems.size()));
    for (const ClassificationDisplayItem& item : kClassificationDisplayItems) {
        if (item.code >= 0) {
            profileClassificationCodes.append(item.code);
        }
    }
    profileClassificationController_->initializeClassificationItems(profileClassificationCodes);

    connect(profileClassificationController_, &ProfileClassificationController::saveRequested, this, [this]() {
        saveProfileClassificationEditsToLas();
    });
    connect(profileClassificationController_, &ProfileClassificationController::modeChanged, this, [this](bool enabled) {
        if (enabled && profileClassificationDock_ != nullptr) {
            profileClassificationDock_->show();
            profileClassificationDock_->raise();
        }
        if (!enabled) {
            promptSaveProfileClassificationEditsIfNeeded();
        }
    });
    connect(profileClassificationController_, &ProfileClassificationController::editsDirtyChanged, this, [this](bool dirty) {
        classificationEditsDirty_ = dirty;
    });
    connect(profileClassificationController_, &ProfileClassificationController::stateChanged, this, [this]() {
        updateActionState();
    });

    connect(showProfileClassificationDockAction_, &QAction::toggled, this, [this](bool visible) {
        if (profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible() != visible) {
            profileClassificationDock_->setVisible(visible);
        }
    });

    const auto exportClearanceCsv = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const ClearanceAnalysisResult analysisResult = analyzeClearancePath(
            viewer_->measurementResult().points,
            static_cast<float>(clearanceWarningThresholdMeters_));
        if (!analysisResult.isValid()) {
            showUserMessage(LogLevel::Warning, tr("Add at least two measured points before exporting clearance details."), 3000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Clearance CSV"),
            QStringLiteral("clearance_segments.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        const ClearanceRuleEvaluationResult ruleEvaluation = evaluateClearanceRules(
            analysisResult,
            { clearanceRulePreset_, static_cast<float>(clearanceWarningThresholdMeters_) });
        if (!ClearanceReportExporter::exportSegmentsCsv(filePath, analysisResult, &ruleEvaluation, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Clearance CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };

    connect(showProfileDockAction_, &QAction::toggled, this, [this](bool visible) {
        if (profileDock_ != nullptr && profileDock_->isVisible() != visible) {
            profileDock_->setVisible(visible);
        }
    });
    const auto analyzeCurrentVegetationRisks = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before running vegetation risk analysis."), 3000);
            return;
        }

        const ClearanceAnalysisResult pathAnalysis = analyzeClearancePath(
            viewer_->measurementResult().points,
            static_cast<float>(clearanceWarningThresholdMeters_));
        if (!pathAnalysis.isValid()) {
            showUserMessage(LogLevel::Warning, tr("Add at least two measured points before analyzing corridor risks."), 3500);
            return;
        }

        VegetationRiskAnalysisParameters parameters;
        parameters.searchRadius = static_cast<float>(vegetationSearchRadiusMeters_);
        parameters.criticalThreshold = static_cast<float>(clearanceWarningThresholdMeters_);
        parameters.clusterGap = static_cast<float>(vegetationClusterGapMeters_);
        parameters.minimumClusterPoints = vegetationClusterPointCount_;
        parameters.preferVegetationClassification = preferVegetationClassification_;
        parameters.preset = clearanceRulePreset_;

        vegetationRiskResults_ = analyzeVegetationRisks(
            *viewer_->pointCloudData(),
            pathAnalysis,
            viewer_->towerMarkers(),
            parameters).records;
        selectedVegetationRiskIndex_ = vegetationRiskResults_.isEmpty() ? -1 : 0;
        updateVegetationRiskPanel();
        rebuildProjectTree();
        updateActionState();
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(5);
        }
        showUserMessage(
            LogLevel::Info,
            vegetationRiskResults_.isEmpty()
                ? tr("Vegetation risk analysis completed. No clusters were found near the current corridor.")
                : tr("Vegetation risk analysis completed. %1 cluster(s) detected.")
                    .arg(QLocale().toString(vegetationRiskResults_.size())),
            4000);
    };
    const auto createIssueFromRisk = [this](int riskIndex) -> bool {
        if (viewer_ == nullptr || riskIndex < 0 || riskIndex >= vegetationRiskResults_.size()) {
            return false;
        }

        const VegetationRiskRecord& risk = vegetationRiskResults_.at(riskIndex);
        InspectionIssue issue;
        issue.id = issueDefaultId();
        issue.title = risk.title.trimmed().isEmpty() ? nextDefaultIssueTitle() : risk.title;
        issue.category = tr("Vegetation");
        issue.severity = issueSeverityFromAnalysisSeverity(risk.severity);
        issue.status = IssueStatus::Open;
        issue.point = risk.point;
        issue.relatedTowerIndex = risk.nearestTowerIndex;
        issue.relatedTowerName = risk.nearestTowerName;
        issue.description = tr("%1\nRule: %2\nMin distance: %3 m\nChainage: %4 - %5 m")
            .arg(risk.notes)
            .arg(risk.sourceRule)
            .arg(formatCoordinate(risk.minimumDistance))
            .arg(formatCoordinate(risk.chainageStart))
            .arg(formatCoordinate(risk.chainageEnd));
        issue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        return viewer_->addInspectionIssue(issue);
    };

    measurementAnalysisController_ = new MeasurementAnalysisController(
        viewer_,
        measureAction_,
        clearMeasurementAction_,
        exportClearanceCsvAction_,
        analyzeVegetationRisksAction_,
        focusVegetationRiskAction_,
        createIssueFromRiskAction_,
        createIssuesFromRisksAction_,
        clearVegetationRisksAction_,
        measurementToggleButton_,
        measurementClearButton_,
        clearanceThresholdSpinBox_,
        clearanceRulePresetComboBox_,
        vegetationSearchRadiusSpinBox_,
        vegetationClusterGapSpinBox_,
        vegetationClusterPointCountSpinBox_,
        preferVegetationClassificationCheckBox_,
        clearanceSegmentsTableWidget_,
        vegetationRisksTableWidget_,
        [this](bool enabled) {
            syncProfileDockForMeasurementMode(enabled);
        },
        exportClearanceCsv,
        analyzeCurrentVegetationRisks,
        [this]() {
            if (viewer_ == nullptr || selectedVegetationRiskIndex_ < 0 || selectedVegetationRiskIndex_ >= vegetationRiskResults_.size()) {
                return;
            }
            viewer_->focusOnPoint(vegetationRiskResults_.at(selectedVegetationRiskIndex_).point);
        },
        [this, createIssueFromRisk]() {
            if (createIssueFromRisk(selectedVegetationRiskIndex_)) {
                if (inspectorTabWidget_ != nullptr) {
                    inspectorTabWidget_->setCurrentIndex(2);
                }
                updateIssuePanel();
                showUserMessage(LogLevel::Info, tr("Created an inspection issue from the selected vegetation risk."), 3000);
            }
        },
        [this, createIssueFromRisk]() {
            int createdCount = 0;
            for (int riskIndex = 0; riskIndex < vegetationRiskResults_.size(); ++riskIndex) {
                if (createIssueFromRisk(riskIndex)) {
                    ++createdCount;
                }
            }
            updateIssuePanel();
            showUserMessage(
                LogLevel::Info,
                createdCount <= 0
                    ? tr("No vegetation risks were converted into issues.")
                    : tr("Created %1 inspection issue(s) from vegetation risks.").arg(QLocale().toString(createdCount)),
                3500);
        },
        [this]() {
            vegetationRiskResults_.clear();
            selectedVegetationRiskIndex_ = -1;
            updateVegetationRiskPanel();
            rebuildProjectTree();
            updateActionState();
            showUserMessage(LogLevel::Info, tr("Vegetation risk results cleared."), 2500);
        },
        [this](double value) {
            clearanceWarningThresholdMeters_ = value;
            persistMeasurementSettings();
            updateMeasurementPanel();
        },
        [this](int index) {
            if (index < 0 || clearanceRulePresetComboBox_ == nullptr) {
                return;
            }
            clearanceRulePreset_ = static_cast<ClearanceRulePreset>(clearanceRulePresetComboBox_->itemData(index).toInt());
            persistMeasurementSettings();
            updateMeasurementPanel();
            updateVegetationRiskPanel();
        },
        [this](double value) {
            vegetationSearchRadiusMeters_ = value;
            persistMeasurementSettings();
            updateVegetationRiskPanel();
        },
        [this](double value) {
            vegetationClusterGapMeters_ = value;
            persistMeasurementSettings();
        },
        [this](int value) {
            vegetationClusterPointCount_ = value;
            persistMeasurementSettings();
        },
        [this](bool checked) {
            preferVegetationClassification_ = checked;
            persistMeasurementSettings();
            updateVegetationRiskPanel();
        },
        [this](int currentRow) {
            if (profilePlotWidget_ != nullptr) {
                profilePlotWidget_->setSelectedSegmentIndex(currentRow);
            }
        },
        [this](int currentRow) {
            selectedVegetationRiskIndex_ = (currentRow >= 0 && currentRow < vegetationRiskResults_.size()) ? currentRow : -1;
            updateVegetationRiskPanel();
        },
        this);

    const auto syncRoutePlanningOptionsFromUi = [this]() {
        syncRoutePlanningFromProjectCoordinateSystems();
        if (aircraftProfileComboBox_ != nullptr) {
            const QVariant profileValue = aircraftProfileComboBox_->currentData();
            const int profileIndex = profileValue.isValid() ? profileValue.toInt() : static_cast<int>(routePlanningOptions_.aircraftProfile);
            routePlanningOptions_.aircraftProfile = static_cast<DjiAircraftProfile>(profileIndex);
        }
        if (routeSafetyHeightSpinBox_ != nullptr) {
            routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(routeSafetyHeightSpinBox_->value());
        }
        routePlanningOptions_.safety.globalTransitionalSpeedMps = 8.0f;
        routePlanningOptions_.safety.globalRthHeightMeters = 60.0f;
        routePlanningOptions_.safety.defaultGimbalPitchDeg = -45.0f;
        if (routeWaypointSpeedSpinBox_ != nullptr) {
            routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(routeWaypointSpeedSpinBox_->value());
        }
        if (routeWaypointSpacingSpinBox_ != nullptr) {
            routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(routeWaypointSpacingSpinBox_->value());
        }
        if (routeSmoothingStrengthSpinBox_ != nullptr) {
            routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(routeSmoothingStrengthSpinBox_->value());
        }
        if (routeHeightOffsetSpinBox_ != nullptr) {
            routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(routeHeightOffsetSpinBox_->value());
        }
    };

    const auto regenerateInspectionRoute = [this, syncRoutePlanningOptionsFromUi]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before generating an inspection route."), 3000);
            return;
        }
        if (vegetationRiskResults_.isEmpty()) {
            showUserMessage(LogLevel::Warning, tr("Run vegetation risk analysis before generating an inspection route."), 3500);
            return;
        }

        syncRoutePlanningOptionsFromUi();
        const InspectionRoute generatedRoute = generateInspectionRouteFromRisks(
            vegetationRiskResults_,
            viewer_->towerMarkers(),
            routePlanningOptions_.generation,
            routePlanningOptions_.safety);
        currentPowerlineRoute_ = createPowerlineRouteFromInspectionRoute(
            generatedRoute,
            tr("Generated Inspection Route"));
        selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
        selectedRouteWaypointTargetIndex_ = -1;
        applyCurrentRouteToViewer();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(
            LogLevel::Info,
            currentPowerlineRoute_.waypoints.isEmpty()
                ? tr("No route waypoints were generated.")
                : tr("Generated inspection route with %1 waypoint(s).")
                    .arg(QLocale().toString(currentPowerlineRoute_.waypoints.size())),
            3500);
    };

    const auto clearInspectionRoute = [this]() {
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        linkedRouteFilePath_.clear();
        setRouteEditingEnabled(false, false);
        viewer_->clearInspectionRouteWaypoints();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showUserMessage(LogLevel::Info, tr("Inspection route cleared."), 2500);
    };

    const auto syncRouteRoamControls = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const bool hasRoute = !currentPowerlineRoute_.waypoints.isEmpty();
        const bool roamReady = hasRoute && viewer_->hasPointCloud() && viewer_->inspectionRouteVisible();
        const bool roamActive = viewer_->inspectionRouteRoamActive();
        const bool roamPaused = viewer_->inspectionRouteRoamPaused();

        if (routeRoamSpeedSpinBox_ != nullptr) {
            routeRoamSpeedSpinBox_->setEnabled(hasRoute);
            const QSignalBlocker blocker(routeRoamSpeedSpinBox_);
            routeRoamSpeedSpinBox_->setValue(viewer_->inspectionRouteRoamSpeedMetersPerSecond());
        }
        if (routeRoamViewModeComboBox_ != nullptr) {
            routeRoamViewModeComboBox_->setEnabled(hasRoute);
            const QSignalBlocker blocker(routeRoamViewModeComboBox_);
            const int modeIndex = routeRoamViewModeComboBox_->findData(static_cast<int>(viewer_->inspectionRouteRoamViewMode()));
            routeRoamViewModeComboBox_->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
        }

        if (startInspectionRouteRoamAction_ != nullptr) {
            startInspectionRouteRoamAction_->setEnabled(roamReady && !roamActive);
        }
        if (pauseInspectionRouteRoamAction_ != nullptr) {
            pauseInspectionRouteRoamAction_->setEnabled(roamReady && roamActive);
            pauseInspectionRouteRoamAction_->setText(roamPaused ? tr("Resume Roam") : tr("Pause Roam"));
        }
        if (stopInspectionRouteRoamAction_ != nullptr) {
            stopInspectionRouteRoamAction_->setEnabled(roamReady && roamActive);
        }

        if (routeRoamStartButton_ != nullptr) {
            routeRoamStartButton_->setEnabled(roamReady && !roamActive);
        }
        if (routeRoamPauseResumeButton_ != nullptr) {
            routeRoamPauseResumeButton_->setEnabled(roamReady && roamActive);
            routeRoamPauseResumeButton_->setText(roamPaused ? tr("Resume Roam") : tr("Pause Roam"));
        }
        if (routeRoamStopButton_ != nullptr) {
            routeRoamStopButton_->setEnabled(roamReady && roamActive);
        }
        syncRouteRoamFloatingDialog();
    };

    const auto startInspectionRouteRoam = [this, syncRouteRoamControls]() {
        if (viewer_ == nullptr || currentPowerlineRoute_.waypoints.isEmpty()) {
            return;
        }
        if (!viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before starting route roam."), 3200);
            return;
        }
        if (!viewer_->inspectionRouteVisible()) {
            showUserMessage(LogLevel::Warning, tr("Show the inspection route before starting camera roam."), 3200);
            return;
        }
        const int startIndex = std::clamp(selectedRouteWaypointIndex_, 0, currentPowerlineRoute_.waypoints.size() - 1);
        routeRoamLastCaptureSummary_.clear();
        viewer_->startInspectionRouteRoam(startIndex);
        syncRouteRoamControls();
    };
    const auto pauseResumeInspectionRouteRoam = [this, syncRouteRoamControls]() {
        if (viewer_ == nullptr || !viewer_->inspectionRouteRoamActive()) {
            return;
        }
        if (viewer_->inspectionRouteRoamPlaying()) {
            viewer_->pauseInspectionRouteRoam();
        } else {
            viewer_->resumeInspectionRouteRoam();
        }
        syncRouteRoamControls();
    };
    const auto stopInspectionRouteRoam = [this, syncRouteRoamControls]() {
        if (viewer_ == nullptr) {
            return;
        }
        routeRoamLastCaptureSummary_.clear();
        viewer_->stopInspectionRouteRoam(true);
        syncRouteRoamControls();
    };

    const auto handleInspectionRouteRoamStateChanged = [this, syncRouteRoamControls]() {
        syncRouteRoamControls();
        updateRoutePlanningPanel();
        updateActionState();
    };

    const auto focusSelectedRouteWaypoint = [this]() {
        if (viewer_ == nullptr
            || selectedRouteWaypointIndex_ < 0
            || selectedRouteWaypointIndex_ >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }
        viewer_->focusOnPoint(currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).localPoint, 0.22);
    };

    const auto importRouteFileFromDisk = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before importing route files."), 3000);
            return;
        }
        const QString filePath = showStyledOpenFileNameDialog(
            this,
            tr("Import Route File"),
            QString(),
            tr("Route JSON Files (*.json);;All Files (*.*)"));
        if (filePath.isEmpty()) {
            return;
        }
        importRouteFile(filePath, true, true);
    };

    const auto saveRouteFileToDisk = [this]() {
        if (currentPowerlineRoute_.waypoints.isEmpty()) {
            return;
        }
        if (linkedRouteFilePath_.trimmed().isEmpty()) {
            const QString filePath = showStyledSaveFileNameDialog(
                this,
                tr("Save Route File"),
                QStringLiteral("route.json"),
                tr("Route JSON Files (*.json)"));
            if (filePath.isEmpty()) {
                return;
            }
            exportRouteFile(filePath, true, true);
            return;
        }
        exportRouteFile(linkedRouteFilePath_, true, true);
    };

    const auto saveRouteFileAsToDisk = [this]() {
        if (currentPowerlineRoute_.waypoints.isEmpty()) {
            return;
        }
        const QString initialPath = linkedRouteFilePath_.trimmed().isEmpty()
            ? QStringLiteral("route.json")
            : linkedRouteFilePath_;
        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Save Route File As"),
            initialPath,
            tr("Route JSON Files (*.json)"));
        if (filePath.isEmpty()) {
            return;
        }
        exportRouteFile(filePath, true, true);
    };

    const auto reloadLinkedRouteFileFromDisk = [this]() {
        reloadLinkedRouteFile(true);
    };

    const auto importRouteKmlFromDisk = [this, syncRoutePlanningOptionsFromUi]() {
        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before importing route KML."), 4500);
            return;
        }

        const QString filePath = showStyledOpenFileNameDialog(
            this,
            tr("Import Route KML"),
            QString(),
            tr("KML Files (*.kml)"));
        if (filePath.isEmpty()) {
            return;
        }

        InspectionRoute importedWgs84;
        QString errorMessage;
        if (!importRouteKml(filePath, &importedWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        InspectionRoute importedLocal;
        if (!transformRouteFromWgs84(importedWgs84, projectCoordinateSystems_, &importedLocal, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        currentPowerlineRoute_ = createPowerlineRouteFromInspectionRoute(importedLocal, QFileInfo(filePath).baseName());
        selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
        selectedRouteWaypointTargetIndex_ = -1;
        linkedRouteFilePath_.clear();
        applyCurrentRouteToViewer();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        showRouteDetailsDock(0);
        showUserMessage(LogLevel::Info, tr("Imported route KML: %1").arg(QFileInfo(filePath).fileName()), 3500);
    };

    const auto exportRouteKmlToDisk = [this, syncRoutePlanningOptionsFromUi]() {
        if (currentPowerlineRoute_.waypoints.isEmpty()) {
            showUserMessage(LogLevel::Warning, tr("Generate a route before exporting KML."), 3000);
            return;
        }

        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before exporting route KML."), 4500);
            return;
        }

        const InspectionRoute routeLocal = toInspectionRouteExportView(currentPowerlineRoute_);
        InspectionRoute routeWgs84;
        QString errorMessage;
        if (!transformRouteToWgs84(routeLocal, projectCoordinateSystems_, &routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Route KML"),
            QStringLiteral("inspection_route.kml"),
            tr("KML Files (*.kml)"));
        if (filePath.isEmpty()) {
            return;
        }

        if (!exportRouteKml(filePath, routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("Route KML exported: %1").arg(QFileInfo(filePath).fileName()), 3500);
    };

    const auto exportRouteDjiKmzToDisk = [this, syncRoutePlanningOptionsFromUi]() {
        if (currentPowerlineRoute_.waypoints.size() < 2) {
            showUserMessage(LogLevel::Warning, tr("Route needs at least 2 waypoints for DJI KMZ export."), 3500);
            return;
        }

        updateRoutePlanningPanel();
        if (routeQaReport_.hasBlockingIssues()) {
            showRouteDetailsDock(2);
            showUserMessage(
                LogLevel::Error,
                tr("Route QA found %1 blocking issue(s). Fix them before exporting DJI KMZ.")
                    .arg(QLocale().toString(routeQaReport_.blockingIssueCount)),
                5200);
            return;
        }

        if (routeQaReport_.hasWarnings()) {
            showRouteDetailsDock(2);
            const QMessageBox::StandardButton choice = showLightStyledMessageBox(
                this,
                QMessageBox::Warning,
                tr("Route QA Warning"),
                tr("Route QA found %1 warning issue(s). Continue exporting DJI KMZ?")
                    .arg(QLocale().toString(routeQaReport_.warningIssueCount)),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (choice != QMessageBox::Yes) {
                return;
            }
        }

        syncRoutePlanningOptionsFromUi();
        if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
            showUserMessage(LogLevel::Error, tr("Set the project point cloud CRS before exporting DJI KMZ."), 4500);
            return;
        }

        const InspectionRoute routeLocal = toInspectionRouteExportView(currentPowerlineRoute_);
        InspectionRoute routeWgs84;
        QString errorMessage;
        if (!transformRouteToWgs84(routeLocal, projectCoordinateSystems_, &routeWgs84, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export DJI KMZ"),
            QStringLiteral("inspection_route.kmz"),
            tr("DJI Wayline KMZ (*.kmz)"));
        if (filePath.isEmpty()) {
            return;
        }

        if (!exportRouteDjiKmz(filePath, routeWgs84, routePlanningOptions_, &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("DJI KMZ exported: %1").arg(QFileInfo(filePath).fileName()), 3500);
    };

    routeController_ = new RouteController(
        viewer_,
        generateInspectionRouteAction_,
        regenerateInspectionRouteAction_,
        clearInspectionRouteAction_,
        toggleRouteEditingAction_,
        startInspectionRouteRoamAction_,
        pauseInspectionRouteRoamAction_,
        stopInspectionRouteRoamAction_,
        focusRouteWaypointAction_,
        importRouteFileAction_,
        saveRouteFileAction_,
        saveRouteFileAsAction_,
        reloadRouteFileAction_,
        importRouteKmlAction_,
        exportRouteKmlAction_,
        exportRouteDjiKmzAction_,
        routeRoamStartButton_,
        routeRoamPauseResumeButton_,
        routeRoamStopButton_,
        routeRoamSpeedSpinBox_,
        routeRoamViewModeComboBox_,
        regenerateInspectionRoute,
        clearInspectionRoute,
        [this](bool enabled) {
            setRouteEditingEnabled(enabled, true);
        },
        startInspectionRouteRoam,
        pauseResumeInspectionRouteRoam,
        stopInspectionRouteRoam,
        [this](double speed) {
            if (viewer_ == nullptr) {
                return;
            }
            viewer_->setInspectionRouteRoamSpeedMetersPerSecond(speed);
            persistWindowSettings();
        },
        [this](int) {
            if (viewer_ == nullptr || routeRoamViewModeComboBox_ == nullptr) {
                return;
            }
            viewer_->setInspectionRouteRoamViewMode(static_cast<RouteRoamViewMode>(routeRoamViewModeComboBox_->currentData().toInt()));
            persistWindowSettings();
        },
        focusSelectedRouteWaypoint,
        importRouteFileFromDisk,
        saveRouteFileToDisk,
        saveRouteFileAsToDisk,
        reloadLinkedRouteFileFromDisk,
        importRouteKmlFromDisk,
        exportRouteKmlToDisk,
        exportRouteDjiKmzToDisk,
        handleInspectionRouteRoamStateChanged,
        [this](int waypointIndex, int targetIndex, const QString& targetLabel, int captureCount) {
            handleRouteRoamPhotoCaptured(waypointIndex, targetIndex, targetLabel, captureCount);
        },
        this);

    if (routeWaypointLabelModeComboBox_ != nullptr) {
        connect(routeWaypointLabelModeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
            if (viewer_ == nullptr || routeWaypointLabelModeComboBox_ == nullptr) {
                return;
            }
            viewer_->setInspectionRouteWaypointLabelDisplayMode(static_cast<RouteLabelDisplayMode>(
                routeWaypointLabelModeComboBox_->currentData().toInt()));
            persistWindowSettings();
        });
    }

    if (routePartLabelModeComboBox_ != nullptr) {
        connect(routePartLabelModeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
            if (viewer_ == nullptr || routePartLabelModeComboBox_ == nullptr) {
                return;
            }
            viewer_->setInspectionRoutePartLabelDisplayMode(static_cast<RouteLabelDisplayMode>(
                routePartLabelModeComboBox_->currentData().toInt()));
            persistWindowSettings();
        });
    }

    if (routeWaypointShowCoordinatesCheckBox_ != nullptr) {
        connect(routeWaypointShowCoordinatesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRouteWaypointTableColumnVisibility();
            persistWindowSettings();
        });
    }

    if (routeWaypointShowCaptureAnglesCheckBox_ != nullptr) {
        connect(routeWaypointShowCaptureAnglesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRouteWaypointTableColumnVisibility();
            persistWindowSettings();
        });
    }

    if (routePartShowCoordinatesCheckBox_ != nullptr) {
        connect(routePartShowCoordinatesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRoutePartTableColumnVisibility();
            persistWindowSettings();
        });
    }

    if (routePartShowCaptureAnglesCheckBox_ != nullptr) {
        connect(routePartShowCaptureAnglesCheckBox_, &QCheckBox::toggled, this, [this](bool) {
            applyRoutePartTableColumnVisibility();
            persistWindowSettings();
        });
    }

    if (routeWaypointColorButton_ != nullptr) {
        connect(routeWaypointColorButton_, &QPushButton::clicked, this, [this]() { chooseRouteWaypointColor(); });
    }

    if (routePartPointColorButton_ != nullptr) {
        connect(routePartPointColorButton_, &QPushButton::clicked, this, [this]() { chooseRoutePartPointColor(); });
    }

    if (routeTrajectoryColorButton_ != nullptr) {
        connect(routeTrajectoryColorButton_, &QPushButton::clicked, this, [this]() { chooseRouteTrajectoryColor(); });
    }

    if (routePartPointsTableWidget_ != nullptr) {
        routePartPointsTableWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(routePartPointsTableWidget_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            if (routePartPointsTableWidget_ == nullptr) {
                return;
            }

            const QModelIndex index = routePartPointsTableWidget_->indexAt(pos);
            if (index.isValid()) {
                routePartPointsTableWidget_->setCurrentCell(index.row(), kRoutePartColumnPartName);
            }

            QMenu menu(routePartPointsTableWidget_);
            QAction* focusAction = menu.addAction(tr("Focus Part Point"));
            QAction* removeAction = menu.addAction(tr("Delete Part Point"));
            removeAction->setEnabled(selectedRoutePartIndex_ > 0 && routeEditingEnabled_);
            QAction* chosenAction = menu.exec(routePartPointsTableWidget_->viewport()->mapToGlobal(pos));
            if (chosenAction == focusAction) {
                focusRoutePartPoint(selectedRoutePartIndex_);
            } else if (chosenAction == removeAction) {
                removeRoutePartPoint(selectedRoutePartIndex_, true);
            }
        });
        connect(routePartPointsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_ || routePartPointsTableWidget_ == nullptr) {
                return;
            }

            if (currentRow >= 0 && currentRow < routePartPointsTableWidget_->rowCount()) {
                QTableWidgetItem* indexItem = routePartPointsTableWidget_->item(currentRow, 0);
                selectedRoutePartIndex_ = indexItem != nullptr ? indexItem->data(Qt::UserRole).toInt() : -1;
            } else {
                selectedRoutePartIndex_ = -1;
            }
        });
        connect(routePartPointsTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            if (routePartPointsTableWidget_ == nullptr || row < 0 || row >= routePartPointsTableWidget_->rowCount()) {
                return;
            }

            QTableWidgetItem* indexItem = routePartPointsTableWidget_->item(row, 0);
            focusRoutePartPoint(indexItem != nullptr ? indexItem->data(Qt::UserRole).toInt() : -1);
        });
    }

    if (routeWaypointsTableWidget_ != nullptr) {
        routeWaypointsTableWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(routeWaypointsTableWidget_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            if (routeWaypointsTableWidget_ == nullptr) {
                return;
            }

            const QModelIndex index = routeWaypointsTableWidget_->indexAt(pos);
            const int contextWaypointIndex = index.isValid() ? index.row() : selectedRouteWaypointIndex_;
            if (index.isValid()) {
                routeWaypointsTableWidget_->setCurrentCell(index.row(), kRouteWaypointColumnPart);
            }

            QMenu menu(routeWaypointsTableWidget_);
            QAction* editAction = menu.addAction(tr("Edit Waypoint"));
            QAction* focusAction = menu.addAction(tr("Focus Waypoint"));
            QAction* removeAction = menu.addAction(tr("Delete Waypoint"));
            editAction->setEnabled(contextWaypointIndex >= 0 && contextWaypointIndex < currentPowerlineRoute_.waypoints.size());
            focusAction->setEnabled(contextWaypointIndex >= 0 && contextWaypointIndex < currentPowerlineRoute_.waypoints.size());
            removeAction->setEnabled(contextWaypointIndex >= 0 && contextWaypointIndex < currentPowerlineRoute_.waypoints.size() && routeEditingEnabled_);
            QAction* chosenAction = menu.exec(routeWaypointsTableWidget_->viewport()->mapToGlobal(pos));
            if (chosenAction == editAction) {
                editRouteWaypoint(contextWaypointIndex);
            } else if (chosenAction == focusAction) {
                focusRouteWaypoint(contextWaypointIndex);
            } else if (chosenAction == removeAction) {
                removeRouteWaypoint(contextWaypointIndex, true);
            }
        });
        connect(routeWaypointsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_) {
                return;
            }

            selectedRouteWaypointIndex_ =
                (currentRow >= 0 && currentRow < currentPowerlineRoute_.waypoints.size())
                    ? currentRow
                    : -1;
            if (selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()) {
                const int targetCount = currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.size();
                selectedRouteWaypointTargetIndex_ =
                    targetCount > 0 ? std::clamp(selectedRouteWaypointTargetIndex_, 0, targetCount - 1) : -1;
            } else {
                selectedRouteWaypointTargetIndex_ = -1;
            }
            if (viewer_ != nullptr) {
                viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
                viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
            }

            if (routePartPointsTableWidget_ != nullptr) {
                const int partIndex =
                    selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()
                    ? routeWaypointRepresentativePartIndex(currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_))
                    : -1;
                if (partIndex > 0) {
                    for (int row = 0; row < routePartPointsTableWidget_->rowCount(); ++row) {
                        QTableWidgetItem* item = routePartPointsTableWidget_->item(row, 0);
                        if (item != nullptr && item->data(Qt::UserRole).toInt() == partIndex) {
                            routePartPointsTableWidget_->setCurrentCell(row, kRoutePartColumnPartName);
                            break;
                        }
                    }
                } else {
                    selectedRoutePartIndex_ = -1;
                    routePartPointsTableWidget_->clearSelection();
                }
            }

            updateRoutePlanningPanel();
            updateActionState();
        });
        connect(routeWaypointsTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            focusRouteWaypoint(row);
        });
    }

    if (routeWaypointTargetsTableWidget_ != nullptr) {
        connect(routeWaypointTargetsTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_ || selectedRouteWaypointIndex_ < 0 || selectedRouteWaypointIndex_ >= currentPowerlineRoute_.waypoints.size()) {
                return;
            }

            const int targetCount = currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.size();
            selectedRouteWaypointTargetIndex_ =
                (currentRow >= 0 && currentRow < targetCount) ? currentRow : -1;
            if (viewer_ != nullptr) {
                viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
            }
            updateActionState();
        });
    }

    if (routeQaIssuesTableWidget_ != nullptr) {
        connect(routeQaIssuesTableWidget_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
            if (updatingRouteTables_ || routeQaIssuesTableWidget_ == nullptr) {
                return;
            }

            selectedRouteQaIssueIndex_ =
                (currentRow >= 0 && currentRow < routeQaReport_.issues.size())
                    ? currentRow
                    : -1;
        });

        connect(routeQaIssuesTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            focusRouteQaIssue(row);
        });
    }

    connect(viewer_, &PointCloudViewer::selectedInspectionRouteWaypointChanged, this, [this](int index) {
        selectedRouteWaypointIndex_ = index;
        if (viewer_ != nullptr) {
            selectedRouteWaypointTargetIndex_ = viewer_->selectedInspectionRouteWaypointTargetIndex();
        }
        updateRoutePlanningPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::inspectionRouteWaypointDoubleClicked, this, [this](int index) {
        editRouteWaypoint(index);
    });
    connect(viewer_, &PointCloudViewer::inspectionRouteWaypointDragFinished, this, [this](int index, const PointRecord& point) {
        if (!ensureRouteEditingEnabled(true)) {
            applyCurrentRouteToViewer();
            updateRoutePlanningPanel();
            return;
        }

        if (index < 0 || index >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& waypoint = currentPowerlineRoute_.waypoints[index];
        waypoint.localPoint = point;
        waypoint.dh = point.z;
        waypoint.height = point.z;

        QString geographicWarning;
        if (projectCoordinateSystems_.pointCloudCrs.code > 0 && projectCoordinateSystems_.geographicCrs.code > 0) {
            QPointF geographicPoint;
            QString errorMessage;
            if (CrsTransformService::transformPoint(
                    projectCoordinateSystems_.pointCloudCrs,
                    projectCoordinateSystems_.geographicCrs,
                    QPointF(point.x, point.y),
                    &geographicPoint,
                    &errorMessage)) {
                waypoint.longitude = geographicPoint.x();
                waypoint.latitude = geographicPoint.y();
            } else {
                geographicWarning = errorMessage.isEmpty()
                    ? tr("Waypoint local position was updated, but geographic coordinates could not be synchronized.")
                    : errorMessage;
            }
        }

        selectedRouteWaypointIndex_ = index;
        selectedRouteWaypointTargetIndex_ = waypoint.captureTargets.isEmpty()
            ? -1
            : std::clamp(selectedRouteWaypointTargetIndex_, 0, waypoint.captureTargets.size() - 1);
        applyCurrentRouteToViewer();
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();

        if (!geographicWarning.isEmpty()) {
            showUserMessage(LogLevel::Warning, geographicWarning, 4500);
        } else {
            showUserMessage(
                LogLevel::Info,
                tr("Updated route waypoint #%1.").arg(QLocale().toString(index + 1)),
                2200);
        }
    });
    connect(aircraftProfileComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const QVariant profileValue = aircraftProfileComboBox_->currentData();
        const int profileIndex = profileValue.isValid() ? profileValue.toInt() : static_cast<int>(routePlanningOptions_.aircraftProfile);
        routePlanningOptions_.aircraftProfile = static_cast<DjiAircraftProfile>(profileIndex);
        updateRoutePlanningPanel();
        persistWindowSettings();
    });
    connect(routeSafetyHeightSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(value);
        updateRoutePlanningPanel();
        persistWindowSettings();
    });
    connect(routeWaypointSpeedSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(value);
        updateRoutePlanningPanel();
        persistWindowSettings();
    });
    connect(routeWaypointSpacingSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(value);
        updateRoutePlanningPanel();
        persistWindowSettings();
    });
    connect(routeSmoothingStrengthSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(value);
        updateRoutePlanningPanel();
        persistWindowSettings();
    });
    connect(routeHeightOffsetSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(value);
        updateRoutePlanningPanel();
        persistWindowSettings();
    });

    connect(resetClassificationColorsButton_, &QPushButton::clicked, this, [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        classificationNameOverrides_.clear();
        viewer_->resetClassificationColors();
        updateClassificationColorTable();
        updateProfileClassificationPanel();
    });
    connect(classificationColorsTableWidget_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (viewer_ == nullptr || item == nullptr || updatingClassificationColorTable_) {
            return;
        }

        const int classificationCode = item->data(Qt::UserRole).toInt();
        if (item->column() == 0) {
            viewer_->setClassificationVisible(classificationCode, item->checkState() == Qt::Checked);
            return;
        }

        if (item->column() != 2) {
            return;
        }

        const QString trimmedName = item->text().trimmed();
        const QString defaultName = defaultClassificationDisplayName(classificationCode);
        if (trimmedName.isEmpty() || trimmedName == defaultName) {
            classificationNameOverrides_.remove(classificationCode);
        } else {
            classificationNameOverrides_.insert(classificationCode, trimmedName);
        }
        persistVisualizationSettings();
        updateClassificationColorTable();
        updateProfileClassificationPanel();
    });
    connect(classificationColorsTableWidget_, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (viewer_ == nullptr || classificationColorsTableWidget_ == nullptr || updatingClassificationColorTable_) {
            return;
        }
        if (column != 3) {
            return;
        }

        QTableWidgetItem* colorItem = classificationColorsTableWidget_->item(row, 3);
        if (colorItem == nullptr) {
            return;
        }

        const int classificationCode = colorItem->data(Qt::UserRole).toInt();
        const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
        const QColor currentColor = classificationCode < 0
            ? options.classificationFallbackColor
            : options.classificationColors.value(classificationCode, options.classificationFallbackColor);
        const QColor selectedColor = showStyledColorDialog(this, currentColor, tr("Choose Classification Color"));
        if (!selectedColor.isValid()) {
            return;
        }

        if (classificationCode < 0) {
            viewer_->setClassificationFallbackColor(selectedColor);
        } else {
            viewer_->setClassificationColor(classificationCode, selectedColor);
        }

        if (viewer_->visualizationOptions().colorMode != PointCloudColorMode::Classification) {
            viewer_->setColorMode(PointCloudColorMode::Classification);
        }
        updateClassificationColorTable();
    });
    connect(roundSplatsCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setUseRoundSplats);
    connect(axesCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowAxes);
    connect(boundingBoxCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setShowBoundingBox);
    connect(invertOrbitCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertOrbitDrag);
    connect(invertPanCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertPanDrag);
    connect(invertWheelCheckBox_, &QCheckBox::toggled, viewer_, &PointCloudViewer::setInvertWheelZoom);
    connect(wheelZoomSensitivitySlider_, &QSlider::sliderMoved, this, [this](int value) {
        if (wheelZoomSensitivityValueLabel_ != nullptr) {
            wheelZoomSensitivityValueLabel_->setText(tr("%1%").arg(QLocale().toString(value)));
        }
    });
    connect(wheelZoomSensitivitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (wheelZoomSensitivityValueLabel_ != nullptr) {
            wheelZoomSensitivityValueLabel_->setText(tr("%1%").arg(QLocale().toString(value)));
        }
        viewer_->setWheelZoomSensitivityPercent(value);
    });

    const auto beginAddTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before adding tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }
        viewer_->beginTowerAddMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower add mode enabled. Click points continuously to add tower markers, or cancel the tool when finished."), 4500);
    };
    const auto beginInsertTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before inserting tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before inserting a new one."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerInsertMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to insert a tower marker before the current one."), 4000);
    };
    const auto beginMoveTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before moving tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before moving it."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerMoveMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to move the current tower marker."), 4000);
    };
    const auto editCurrentTower = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before moving tower markers."), 3000);
            return;
        }
        if (!towerEditingEnabled_) {
            setTowerEditingEnabled(true);
        }

        const int currentRow = viewer_ != nullptr ? viewer_->selectedTowerIndex() : -1;
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            showUserMessage(LogLevel::Warning, tr("Select the current tower before moving it."), 3000);
            return;
        }

        viewer_->setSelectedTowerIndex(currentRow);
        viewer_->beginTowerMoveMode(currentRow);
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Click a point in the view to move the current tower marker."), 4000);
    };
    const auto focusSelectedTower = [this]() {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->towerMarkers().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->towerMarkers().at(currentRow).point);
    };
    const auto removeSelectedTower = [this]() {
        if (viewer_ == nullptr || towerTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = towerTableWidget_->currentRow();
        if (!viewer_->removeTowerMarker(currentRow)) {
            return;
        }

        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower marker removed."), 2500);
    };
    const auto clearAllTowers = [this]() {
        if (viewer_ == nullptr || viewer_->towerMarkers().isEmpty()) {
            return;
        }

        viewer_->clearTowerMarkers();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower markers cleared."), 2500);
    };
    const auto cancelTowerTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelTowerEditMode();
        updateTowerPanel();
        showUserMessage(LogLevel::Info, tr("Tower tool cancelled."), 2500);
    };
    const auto importTowerFileFromDialog = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before importing tower files."), 3000);
            return;
        }
        const QString filePath = showStyledOpenFileNameDialog(
            this,
            tr("Import Tower File"),
            QString(),
            tr("LiTower Files (*.LiTower);;CSV Files (*.csv);;All Files (*.*)"));
        if (filePath.isEmpty()) {
            return;
        }
        importTowerFile(filePath, true, true);
    };
    const auto saveTowerFileToLinkedPath = [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        if (linkedTowerFilePath_.trimmed().isEmpty()) {
            const QString filePath = showStyledSaveFileNameDialog(
                this,
                tr("Save Tower File"),
                QStringLiteral("tower.LiTower"),
                tr("LiTower Files (*.LiTower);;CSV Files (*.csv)"));
            if (filePath.isEmpty()) {
                return;
            }
            exportTowerFile(filePath, true, true);
            return;
        }
        exportTowerFile(linkedTowerFilePath_, true, true);
    };
    const auto saveTowerFileAs = [this]() {
        if (viewer_ == nullptr) {
            return;
        }
        const QString initialPath = linkedTowerFilePath_.trimmed().isEmpty()
            ? QStringLiteral("tower.LiTower")
            : linkedTowerFilePath_;
        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Save Tower File As"),
            initialPath,
            tr("LiTower Files (*.LiTower);;CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }
        exportTowerFile(filePath, true, true);
    };
    const auto reloadTowerFileFromLinkedPath = [this]() {
        reloadLinkedTowerFile(true);
    };
    const auto startTowerEditing = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before editing tower markers."), 3000);
            return;
        }

        setTowerEditingEnabled(true);
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(1);
        }
        showUserMessage(LogLevel::Info, tr("Tower editing started. Use the tools in the right dock to add, insert, move, rename, or remove tower markers."), 4500);
    };
    const auto finishTowerEditing = [this]() {
        setTowerEditingEnabled(false);
        showUserMessage(LogLevel::Info, tr("Tower editing finished."), 2500);
    };

    const auto commitTowerDetails = [this]() {
        if (updatingTowerDetails_ || viewer_ == nullptr || towerTypeComboBox_ == nullptr) {
            return;
        }

        const int selectedTowerIndex = viewer_->selectedTowerIndex();
        if (selectedTowerIndex < 0 || selectedTowerIndex >= viewer_->towerMarkers().size()) {
            return;
        }

        TowerRecord towerRecord = viewer_->towerMarkers().at(selectedTowerIndex);
        towerRecord.code = towerCodeEdit_->text().trimmed();
        towerRecord.lineName = towerLineNameEdit_->text().trimmed();
        towerRecord.voltageLevel = towerVoltageLevelEdit_->text().trimmed();
        towerRecord.towerType = static_cast<TowerType>(towerTypeComboBox_->currentData().toInt());
        towerRecord.structureType = towerStructureTypeEdit_->text().trimmed();
        towerRecord.inspectionDate = towerInspectionDateEdit_->text().trimmed();
        towerRecord.status = towerStatusEdit_->text().trimmed();
        towerRecord.notes = towerNotesEdit_->toPlainText().trimmed();
        if (viewer_->setTowerRecord(selectedTowerIndex, towerRecord)) {
            updateTowerPanel();
        }
    };

    towerController_ = new TowerController(
        startTowerEditAction_,
        finishTowerEditAction_,
        addTowerAction_,
        insertTowerAction_,
        moveTowerAction_,
        editCurrentTowerAction_,
        focusTowerAction_,
        removeTowerAction_,
        clearTowersAction_,
        cancelTowerToolAction_,
        importTowerFileAction_,
        saveTowerFileAction_,
        saveTowerFileAsAction_,
        reloadTowerFileAction_,
        showTowerXAction_,
        showTowerYAction_,
        showTowerZAction_,
        towerTableWidget_,
        towerCodeEdit_,
        towerLineNameEdit_,
        towerVoltageLevelEdit_,
        towerTypeComboBox_,
        towerStructureTypeEdit_,
        towerInspectionDateEdit_,
        towerStatusEdit_,
        towerNotesEdit_,
        startTowerEditing,
        finishTowerEditing,
        beginAddTower,
        beginInsertTower,
        beginMoveTower,
        editCurrentTower,
        focusSelectedTower,
        removeSelectedTower,
        clearAllTowers,
        cancelTowerTool,
        importTowerFileFromDialog,
        saveTowerFileToLinkedPath,
        saveTowerFileAs,
        reloadTowerFileFromLinkedPath,
        [this](bool) {
            persistWindowSettings();
        },
        [this](bool) {
            persistWindowSettings();
        },
        [this](bool) {
            persistWindowSettings();
        },
        [this](int currentRow) {
            if (viewer_ != nullptr) {
                viewer_->setSelectedTowerIndex(currentRow);
            }
            updateActionState();
            updateTowerPanel();
        },
        [this](int row, const QString& towerName) {
            if (viewer_ == nullptr) {
                return;
            }

            if (!towerEditingEnabled_) {
                updateTowerPanel();
                return;
            }

            if (!viewer_->setTowerMarkerName(row, towerName)) {
                showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
                updateTowerPanel();
                return;
            }

            updateTowerPanel();
        },
        commitTowerDetails,
        this);

    const auto beginIssueMarking = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            showUserMessage(LogLevel::Warning, tr("Load a point cloud before marking issues."), 3000);
            return;
        }

        viewer_->beginIssueAddMode();
        if (inspectorTabWidget_ != nullptr) {
            inspectorTabWidget_->setCurrentIndex(2);
        }
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue marking enabled. Click a point in the view to add an issue, or right-click to cancel."), 4500);
    };
    const auto cancelIssueTool = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        viewer_->cancelIssueEditMode();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Issue tool cancelled."), 2500);
    };
    const auto focusSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        const int currentRow = issueTableWidget_->currentRow();
        if (currentRow < 0 || currentRow >= viewer_->inspectionIssues().size()) {
            return;
        }

        viewer_->focusOnPoint(viewer_->inspectionIssues().at(currentRow).point, 0.2);
    };
    const auto removeSelectedIssue = [this]() {
        if (viewer_ == nullptr || issueTableWidget_ == nullptr) {
            return;
        }

        if (viewer_->removeInspectionIssue(issueTableWidget_->currentRow())) {
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue removed."), 2500);
        }
    };
    const auto clearAllIssues = [this]() {
        if (viewer_ == nullptr || viewer_->inspectionIssues().isEmpty()) {
            return;
        }

        viewer_->clearInspectionIssues();
        updateIssuePanel();
        showUserMessage(LogLevel::Info, tr("Inspection issues cleared."), 2500);
    };
    const auto exportIssuesCsv = [this]() {
        if (viewer_ == nullptr) {
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Issues CSV"),
            QStringLiteral("inspection_issues.csv"),
            tr("CSV Files (*.csv)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        if (!InspectionReportExporter::exportIssuesCsv(filePath, viewer_->inspectionIssues(), &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }
        showUserMessage(LogLevel::Info, tr("Issue CSV exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    const auto exportInspectionReport = [this]() {
        if (viewer_ == nullptr || !viewer_->hasPointCloud()) {
            return;
        }

        const QString filePath = showStyledSaveFileNameDialog(
            this,
            tr("Export Inspection Report"),
            QStringLiteral("inspection_report.html"),
            tr("HTML Files (*.html)"));
        if (filePath.isEmpty()) {
            return;
        }

        QString errorMessage;
        const QString projectName = currentProjectFilePath_.isEmpty()
            ? tr("Current Project")
            : QFileInfo(currentProjectFilePath_).completeBaseName();
        if (!InspectionReportExporter::exportProjectHtml(
                filePath,
                projectName,
                viewer_->currentFilePaths(),
                viewer_->towerMarkers(),
                viewer_->inspectionIssues(),
                &errorMessage)) {
            showUserMessage(LogLevel::Error, errorMessage, 5000);
            return;
        }

        showUserMessage(LogLevel::Info, tr("Inspection report exported: %1").arg(QFileInfo(filePath).fileName()), 3000);
    };
    const auto commitIssueDetails = [this]() {
        if (updatingIssueDetails_ || viewer_ == nullptr) {
            return;
        }

        const int selectedIssueIndex = viewer_->selectedIssueIndex();
        if (selectedIssueIndex < 0 || selectedIssueIndex >= viewer_->inspectionIssues().size()) {
            return;
        }

        InspectionIssue issue = viewer_->inspectionIssues().at(selectedIssueIndex);
        issue.title = issueTitleEdit_->text().trimmed();
        issue.category = issueCategoryComboBox_->currentText().trimmed();
        issue.severity = static_cast<IssueSeverity>(issueSeverityComboBox_->currentIndex());
        issue.status = static_cast<IssueStatus>(issueStatusComboBox_->currentIndex());
        issue.relatedTowerIndex = issueRelatedTowerComboBox_->currentData().toInt();
        issue.relatedTowerName = issueRelatedTowerComboBox_->currentText().trimmed();
        issue.imagePath = issueImagePathEdit_->text().trimmed();
        issue.description = issueDescriptionEdit_->toPlainText().trimmed();
        if (viewer_->updateInspectionIssue(selectedIssueIndex, issue)) {
            updateIssuePanel();
        }
    };

    issueController_ = new IssueController(
        startIssueMarkAction_,
        cancelIssueToolAction_,
        focusIssueAction_,
        removeIssueAction_,
        clearIssuesAction_,
        exportIssuesCsvAction_,
        exportInspectionReportAction_,
        issueTableWidget_,
        issueTitleEdit_,
        issueCategoryComboBox_,
        issueSeverityComboBox_,
        issueStatusComboBox_,
        issueRelatedTowerComboBox_,
        issueImagePathEdit_,
        issueDescriptionEdit_,
        beginIssueMarking,
        cancelIssueTool,
        focusSelectedIssue,
        removeSelectedIssue,
        clearAllIssues,
        exportIssuesCsv,
        exportInspectionReport,
        [this](int currentRow) {
            if (viewer_ != nullptr) {
                viewer_->setSelectedIssueIndex(currentRow);
            }
            updateActionState();
            updateIssuePanel();
        },
        commitIssueDetails,
        this);
    if (projectExplorerController_ != nullptr) {
        connect(projectExplorerController_, &ProjectExplorerController::searchTextChanged, this, [this](const QString&) {
            refreshProjectTreeFilter();
        });
        connect(projectExplorerController_, &ProjectExplorerController::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            applyProjectTreeItemCheckState(item);
        });
        connect(projectExplorerController_, &ProjectExplorerController::currentItemChanged, this, [this](QTreeWidgetItem* currentItem, QTreeWidgetItem*) {
            if (viewer_ == nullptr || currentItem == nullptr) {
                updateActionState();
                return;
            }

            const QString itemType = currentItem->data(0, kProjectTreeItemTypeRole).toString();
            if (itemType == QStringLiteral("imageItem")) {
                const int issueIndex = currentItem->data(0, kProjectTreeIssueIndexRole).toInt();
                viewer_->setSelectedIssueIndex(issueIndex);
                updateIssuePanel();
            } else if (itemType == QStringLiteral("trajectoryItem")) {
                selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
                selectedRouteWaypointTargetIndex_ = -1;
                viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
                viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
                updateRoutePlanningPanel();
            } else if (itemType == QStringLiteral("pointCloudItem")) {
                viewer_->setSelectedIssueIndex(-1);
                updateIssuePanel();
            } else {
                viewer_->setSelectedIssueIndex(-1);
                updateIssuePanel();
            }
            updateActionState();
        });
        connect(projectExplorerController_, &ProjectExplorerController::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
            focusProjectTreeItem(item);
        });
        connect(projectExplorerController_, &ProjectExplorerController::customContextMenuRequested, this, [this](const QPoint& pos) {
            showProjectTreeContextMenu(pos);
        });
    }

}

void MainWindow::createWindowAndViewerConnections()
{
    connect(languageEnglishAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::English); });
    connect(languageChineseAction_, &QAction::triggered, this, [this]() { applyLanguage(UiLanguage::Chinese); });

    if (logDock_ != nullptr) {
        connect(logDock_, &ApplicationLogDock::filterStateChanged, this, [this]() {
            persistWindowSettings();
        });
        connect(logDock_, &ApplicationLogDock::autoScrollToggled, this, [this](bool) {
            persistWindowSettings();
        });
        connect(logDock_, &ApplicationLogDock::entriesClearedByUser, this, [this]() {
            if (statusBar() != nullptr) {
                statusBar()->showMessage(tr("Log entries cleared."), 2500);
            }
        });
        connect(logDock_, &ApplicationLogDock::exportRequested, this, [this]() {
            exportLogEntries();
        });
    }

    if (logDock_ != nullptr) {
        auto* focusLogSearchShortcut = new QShortcut(QKeySequence::Find, this);
        focusLogSearchShortcut->setContext(Qt::WindowShortcut);
        connect(focusLogSearchShortcut, &QShortcut::activated, this, [this]() {
            if (logDock_ == nullptr || logDock_->searchLineEdit() == nullptr) {
                return;
            }

            if (!logDock_->isVisible()) {
                if (showLogAction_ != nullptr) {
                    showLogAction_->setChecked(true);
                } else {
                    logDock_->show();
                }
            }

            logDock_->raise();
            logDock_->searchLineEdit()->setFocus(Qt::ShortcutFocusReason);
            logDock_->searchLineEdit()->selectAll();
        });

        auto* clearLogSearchShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), logDock_);
        clearLogSearchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(clearLogSearchShortcut, &QShortcut::activated, this, [this]() {
            if (logDock_ == nullptr || logDock_->searchLineEdit() == nullptr) {
                return;
            }

            if (!logDock_->searchLineEdit()->text().isEmpty()) {
                logDock_->searchLineEdit()->clear();
            } else if (logDock_->searchLineEdit()->hasFocus()) {
                logDock_->searchLineEdit()->clearFocus();
            }
        });
    }

    connect(showLogAction_, &QAction::toggled, this, [this](bool visible) {
        if (logDock_ != nullptr) {
            if (visible) {
                logDock_->show();
                logDock_->raise();
                resizeDocks({logDock_}, {280}, Qt::Vertical);
                logDock_->refreshEntries();
            } else {
                logDock_->hide();
            }
            persistWindowSettings();
        }
    });
    connect(logDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showLogAction_ != nullptr && showLogAction_->isChecked() != visible) {
            showLogAction_->setChecked(visible);
        }
        persistWindowSettings();
    });
    connect(profileDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showProfileDockAction_ != nullptr && showProfileDockAction_->isChecked() != visible) {
            showProfileDockAction_->setChecked(visible);
        }
        persistWindowSettings();
    });
    connect(profileClassificationDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (showProfileClassificationDockAction_ != nullptr && showProfileClassificationDockAction_->isChecked() != visible) {
            const QSignalBlocker blocker(showProfileClassificationDockAction_);
            showProfileClassificationDockAction_->setChecked(visible);
        }
        persistWindowSettings();
    });
    const auto persistDockState = [this]() {
        persistWindowSettings();
    };
    connect(projectDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(inspectorDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(profileClassificationDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(profileDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(logDock_, &QDockWidget::dockLocationChanged, this, persistDockState);
    connect(projectDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(inspectorDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileClassificationDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(profileDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    connect(logDock_, &QDockWidget::topLevelChanged, this, persistDockState);
    if (inspectorTabWidget_ != nullptr) {
        connect(inspectorTabWidget_, &QTabWidget::currentChanged, this, [this](int) {
            persistWindowSettings();
        });
    }
    if (routeDetailsTabWidget_ != nullptr) {
        connect(routeDetailsTabWidget_, &QTabWidget::currentChanged, this, [this](int) {
            persistWindowSettings();
        });
    }

    connect(viewer_, &PointCloudViewer::pointCloudLoadingStarted, this, [this](const QString& message) {
        beginOperationProgress(message);
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingProgress, this, [this](const QString& message, int value, int maximum) {
        updateOperationProgress(message, value, maximum);
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoadingFinished, this, [this]() {
        endOperationProgress();
    });
    connect(viewer_, &PointCloudViewer::pointCloudLoaded, this, [this]() {
        endOperationProgress();
        classificationEditsDirty_ = false;
        rebuildProjectTree();
        syncUiFromViewer();
    });
    connect(viewer_, &PointCloudViewer::pointCloudCleared, this, [this]() {
        endOperationProgress();
        classificationEditsDirty_ = false;
        linkedTowerFilePath_.clear();
        linkedRouteFilePath_.clear();
        setTowerEditingEnabled(false);
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Scene cleared."), 3000);
    });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { syncUiFromViewer(); });
    connect(viewer_, &PointCloudViewer::visualizationOptionsChanged, this, [this]() { persistVisualizationSettings(); });
    connect(viewer_, &PointCloudViewer::interactionOptionsChanged, this, [this]() {
        persistInteractionSettings();
        syncUiFromViewer();
        updateNavigationHelpText();
        showUserMessage(LogLevel::Info, tr("Navigation preferences updated."), 2500);
    });
    connect(viewer_, &PointCloudViewer::measurementChanged, this, [this]() {
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        linkedRouteFilePath_.clear();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::measurementModeChanged, this, [this]() {
        if (!viewer_->measurementEnabled()) {
            vegetationRiskResults_.clear();
            selectedVegetationRiskIndex_ = -1;
            currentPowerlineRoute_ = PowerlineRouteDocument();
            linkedRouteFilePath_.clear();
            selectedRouteWaypointIndex_ = -1;
            selectedRouteWaypointTargetIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
        }
        syncProfileDockForMeasurementMode(viewer_->measurementEnabled());
        syncUiFromViewer();
        updateMeasurementPanel();
    });
    connect(viewer_, &PointCloudViewer::towerMarkersChanged, this, [this]() {
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        linkedRouteFilePath_.clear();
        selectedRouteWaypointIndex_ = -1;
        selectedRouteWaypointTargetIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        updateMeasurementPanel();
        updateTowerPanel();
        updateVegetationRiskPanel();
    });
    connect(viewer_, &PointCloudViewer::selectedTowerChanged, this, [this](int index) {
        if (towerTableWidget_ != nullptr && towerTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(towerTableWidget_);
            if (index >= 0) {
                towerTableWidget_->setCurrentCell(index, 1);
            } else {
                towerTableWidget_->clearSelection();
                towerTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateTowerPanel();
    });
    connect(viewer_, &PointCloudViewer::towerEditModeChanged, this, [this]() {
        updateTowerPanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::towerEditRequested, this, [this](const PointRecord& point, int modeValue, int targetIndex) {
        const TowerEditMode mode = static_cast<TowerEditMode>(modeValue);
        if (viewer_ == nullptr) {
            return;
        }

        if (mode == TowerEditMode::MoveSelected) {
            if (viewer_->moveTowerMarker(targetIndex, point)) {
                updateTowerPanel();
                showUserMessage(LogLevel::Info, tr("Tower marker moved."), 2500);
            }
            return;
        }

        const QString towerName = nextDefaultTowerName();

        const bool inserted = mode == TowerEditMode::InsertBeforeSelected
            ? viewer_->insertTowerMarker(targetIndex, towerName, point)
            : viewer_->addTowerMarker(towerName, point);
        if (!inserted) {
            showUserMessage(LogLevel::Warning, tr("Tower marker name cannot be empty."), 3000);
            return;
        }

        updateTowerPanel();
        showUserMessage(
            LogLevel::Info,
            mode == TowerEditMode::AddAfterLast
                ? tr("Tower marker added. Continue clicking points to add more, or cancel the tool when finished.")
                : tr("Tower marker added."),
            mode == TowerEditMode::AddAfterLast ? 3500 : 2500);
    });
    connect(viewer_, &PointCloudViewer::inspectionIssuesChanged, this, [this]() {
        rebuildProjectTree();
        syncUiFromViewer();
        updateMeasurementPanel();
        updateIssuePanel();
        updateVegetationRiskPanel();
    });
    connect(viewer_, &PointCloudViewer::selectedIssueChanged, this, [this](int index) {
        if (issueTableWidget_ != nullptr && issueTableWidget_->currentRow() != index) {
            const QSignalBlocker blocker(issueTableWidget_);
            if (index >= 0) {
                issueTableWidget_->setCurrentCell(index, 1);
            } else {
                issueTableWidget_->clearSelection();
                issueTableWidget_->setCurrentItem(nullptr);
            }
        }
        updateActionState();
        updateMeasurementPanel();
        updateIssuePanel();
    });
    connect(viewer_, &PointCloudViewer::issueEditModeChanged, this, [this]() {
        updateIssuePanel();
        updateActionState();
    });
    connect(viewer_, &PointCloudViewer::issueEditRequested, this, [this](const PointRecord& point) {
        if (viewer_ == nullptr) {
            return;
        }

        InspectionIssue issue;
        issue.id = issueDefaultId();
        issue.title = nextDefaultIssueTitle();
        issue.category = tr("Other");
        issue.severity = IssueSeverity::Major;
        issue.status = IssueStatus::Open;
        issue.point = point;
        issue.relatedTowerIndex = viewer_->selectedTowerIndex();
        if (issue.relatedTowerIndex >= 0 && issue.relatedTowerIndex < viewer_->towerMarkers().size()) {
            issue.relatedTowerName = viewer_->towerMarkers().at(issue.relatedTowerIndex).name;
        }
        issue.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
        if (viewer_->addInspectionIssue(issue)) {
            if (inspectorTabWidget_ != nullptr) {
                inspectorTabWidget_->setCurrentIndex(2);
            }
            updateIssuePanel();
            showUserMessage(LogLevel::Info, tr("Inspection issue added. Continue clicking points to add more, or right-click to cancel."), 3500);
        }
    });
    connect(viewer_, &PointCloudViewer::measurementMessage, this, [this](const QString& message, bool error) {
        showUserMessage(error ? LogLevel::Error : LogLevel::Info, message, error ? 4000 : 3000);
    });
}

void MainWindow::openBackstagePage(QWidget* page)
{
    if (backstageView_ == nullptr || page == nullptr) {
        return;
    }

    if (page == backstageOpenProjectPage_) {
        refreshBackstageRecentProjects();
    } else if (page == backstageProjectPropertiesPage_) {
        refreshBackstageProjectPropertiesPage();
    } else if (page == backstageApplicationSettingsPage_) {
        refreshBackstageApplicationSettingsPage();
    } else if (page == backstageAboutPage_) {
        refreshBackstageAboutPage();
    }

    backstageView_->setActivePage(page);
    backstageView_->open();
}

void MainWindow::hideBackstageView()
{
    if (backstageView_ != nullptr && backstageView_->isVisible()) {
        backstageView_->hide();
    }
}

void MainWindow::refreshBackstageRecentProjects()
{
    if (backstageRecentProjectsListWidget_ == nullptr || backstageProjectPathLineEdit_ == nullptr) {
        return;
    }

    QSettings settings;
    const QString lastOpenedProject = normalizedProjectFilePath(
        settings.value(settingskeys::kProjectLastOpenedProject).toString());
    const QStringList recentProjects = normalizedRecentProjectFiles(
        settings.value(settingskeys::kProjectRecentProjects).toStringList(),
        lastOpenedProject);
    settings.setValue(settingskeys::kProjectRecentProjects, recentProjects);

    const QSignalBlocker listBlocker(backstageRecentProjectsListWidget_);
    backstageRecentProjectsListWidget_->clear();

    for (const QString& recentProject : recentProjects) {
        const QString displayName = QFileInfo(recentProject).fileName();
        auto* item = new QListWidgetItem(
            displayName.isEmpty() ? QDir::toNativeSeparators(recentProject) : displayName,
            backstageRecentProjectsListWidget_);
        item->setData(Qt::UserRole, recentProject);
        item->setToolTip(QDir::toNativeSeparators(recentProject));
    }

    if (backstageRecentProjectsListWidget_->count() == 0) {
        auto* item = new QListWidgetItem(tr("No recent projects"), backstageRecentProjectsListWidget_);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
    }

    if (!recentProjects.isEmpty()) {
        const int defaultRecentIndex = lastOpenedProject.isEmpty()
            ? 0
            : indexOfProjectFilePath(recentProjects, lastOpenedProject);
        backstageRecentProjectsListWidget_->setCurrentRow(defaultRecentIndex >= 0 ? defaultRecentIndex : 0);
        backstageProjectPathLineEdit_->setText(
            defaultRecentIndex >= 0 ? recentProjects.at(defaultRecentIndex) : recentProjects.constFirst());
    } else if (!currentProjectFilePath_.isEmpty()) {
        backstageProjectPathLineEdit_->setText(currentProjectFilePath_);
    } else {
        backstageProjectPathLineEdit_->clear();
    }

    if (backstageProjectOpenButton_ != nullptr) {
        backstageProjectOpenButton_->setEnabled(!normalizedProjectFilePath(backstageProjectPathLineEdit_->text()).isEmpty());
    }
}

void MainWindow::refreshBackstageProjectPropertiesPage()
{
    if (backstageProjectFileValueLabel_ == nullptr
        || backstageProjectDatasetCountValueLabel_ == nullptr
        || backstageProjectCoordinateSystemsValueLabel_ == nullptr) {
        return;
    }

    const QString projectPath = currentProjectFilePath_.trimmed().isEmpty()
        ? tr("Unsaved project")
        : QDir::toNativeSeparators(currentProjectFilePath_);

    backstageProjectFileValueLabel_->setText(projectPath);
    backstageProjectDatasetCountValueLabel_->setText(
        viewer_ != nullptr
            ? QLocale().toString(viewer_->currentFilePaths().size())
            : QStringLiteral("0"));
    backstageProjectCoordinateSystemsValueLabel_->setText(
        formatProjectCoordinateSystemsSummary(projectCoordinateSystems_));
}

void MainWindow::refreshBackstageApplicationSettingsPage()
{
    if (backstageShowLogCheckBox_ != nullptr && showLogAction_ != nullptr) {
        const QSignalBlocker blocker(backstageShowLogCheckBox_);
        backstageShowLogCheckBox_->setChecked(showLogAction_->isChecked());
    }
}

void MainWindow::refreshBackstageAboutPage()
{
    if (backstageAboutBodyLabel_ == nullptr) {
        return;
    }

    backstageAboutBodyLabel_->setText(tr(
        "Version: %1\n"
        "Frameworks: Qt %2, OpenSceneGraph, Qtitan Ribbon\n"
        "Point cloud stack: LASlib / LASzip, optional PROJ / GDAL support\n"
        "Repository: LAS Point Cloud Viewer for transmission line inspection workflows.")
            .arg(QCoreApplication::applicationVersion(), QString::fromLatin1(qVersion())));
}

void MainWindow::openProjectFromBackstage()
{
    if (backstageProjectPathLineEdit_ == nullptr) {
        return;
    }

    const QString projectPath = normalizedProjectFilePath(backstageProjectPathLineEdit_->text());
    if (projectPath.isEmpty()) {
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Open Project"),
            tr("Select an existing project file."),
            QMessageBox::Ok);
        return;
    }
    if (!QFileInfo::exists(projectPath)) {
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Open Project"),
            tr("Project file does not exist."),
            QMessageBox::Ok);
        return;
    }
    if (!isSupportedProjectFilePath(projectPath)) {
        showLightStyledMessageBox(
            this,
            QMessageBox::Warning,
            tr("Open Project"),
            tr("Choose a .json or .lpproj project file."),
            QMessageBox::Ok);
        return;
    }

    hideBackstageView();
    loadProjectFile(projectPath);
}

void MainWindow::openProjectExplorerFile()
{
    hideBackstageView();
    const QString filePath = showStyledOpenFileNameDialog(
        this,
        tr("Open"),
        QString(),
        tr("Supported Files (*.las *.laz *.lpproj *.json);;LAS Files (*.las *.laz);;Route JSON Files (*.json);;LiDAR Power Projects (*.lpproj *.json);;All Files (*.*)"));

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

void MainWindow::syncProjectCoordinateSystemsFromRoutePlanning()
{
    if (projectCoordinateSystems_.pointCloudCrs.authName.trimmed().isEmpty()) {
        projectCoordinateSystems_.pointCloudCrs.authName = QStringLiteral("EPSG");
    }

    projectCoordinateSystems_.pointCloudCrs.code = routePlanningOptions_.crs.sourceEpsg;
    if (projectCoordinateSystems_.pointCloudCrs.code <= 0) {
        projectCoordinateSystems_.pointCloudCrs.displayName.clear();
        projectCoordinateSystems_.pointCloudCrs.wkt.clear();
    } else {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.pointCloudCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.pointCloudCrs = normalized;
        }
    }
    projectCoordinateSystems_.pointCloudCrs.kind = CoordinateSystemKind::Projected;

    if (projectCoordinateSystems_.geographicCrs.code <= 0) {
        projectCoordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
    } else {
        CoordinateSystemRef normalized;
        if (CrsAuthorityService::normalizeCoordinateSystem(projectCoordinateSystems_.geographicCrs, &normalized, nullptr)) {
            projectCoordinateSystems_.geographicCrs = normalized;
        }
    }
    projectCoordinateSystems_.geographicCrs.kind = CoordinateSystemKind::Geographic;
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

bool MainWindow::editRouteWaypoint(int waypointIndex)
{
    if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
        return false;
    }

    if (!ensureRouteEditingEnabled(true)) {
        return false;
    }

    if (QDialog* existingDialog = findChild<QDialog*>(QStringLiteral("routeWaypointEditDialog"), Qt::FindDirectChildrenOnly);
        existingDialog != nullptr) {
        existingDialog->close();
    }

    struct RouteWaypointEditState
    {
        RouteWaypoint originalWaypoint;
        int selectedTargetIndex = -1;
        int originalTargetIndex = -1;
        bool hasCaptureTargets = false;
    };

    RouteWaypoint& waypoint = currentPowerlineRoute_.waypoints[waypointIndex];
    const RouteWaypoint originalWaypoint = waypoint;
    const bool hasCaptureTargets = !waypoint.captureTargets.isEmpty();
    const int originalTargetIndex = hasCaptureTargets
        ? std::clamp(selectedRouteWaypointTargetIndex_, 0, waypoint.captureTargets.size() - 1)
        : -1;
    const auto state = std::make_shared<RouteWaypointEditState>();
    state->originalWaypoint = originalWaypoint;
    state->selectedTargetIndex = originalTargetIndex;
    state->originalTargetIndex = originalTargetIndex;
    state->hasCaptureTargets = hasCaptureTargets;
    const RouteCaptureTarget selectedTarget =
        (hasCaptureTargets && state->selectedTargetIndex >= 0 && state->selectedTargetIndex < originalWaypoint.captureTargets.size())
            ? originalWaypoint.captureTargets.at(state->selectedTargetIndex)
            : RouteCaptureTarget();

    QHash<int, RoutePartPoint> partPointByIndex;
    for (const RoutePartPoint& partPoint : currentPowerlineRoute_.partPoints) {
        if (partPoint.partIndex > 0) {
            partPointByIndex.insert(partPoint.partIndex, partPoint);
        }
    }

    auto* dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("routeWaypointEditDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    dialog->setWindowTitle(tr("Edit Route Waypoint"));
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->resize(560, 500);
    dialog->setStyleSheet(QStringLiteral(
        "QDialog {"
        "background-color: #f3f7fb;"
        "}"
        "QFrame#routeEditCard {"
        "background-color: rgba(255, 255, 255, 0.97);"
        "border: 1px solid rgba(148, 163, 184, 0.22);"
        "border-radius: 16px;"
        "}"
        "QLabel#routeEditTitle {"
        "color: #0f172a;"
        "font-size: 20px;"
        "font-weight: 700;"
        "}"
        "QLabel#routeEditSubtitle {"
        "color: #475569;"
        "font-size: 13px;"
        "line-height: 1.4em;"
        "}"
        "QLabel#routeEditHint {"
        "color: #92400e;"
        "background-color: #fffbeb;"
        "border: 1px solid #fcd34d;"
        "border-radius: 10px;"
        "padding: 8px 10px;"
        "}"
        "QLabel {"
        "color: #0f172a;"
        "}"
        "QDoubleSpinBox {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 8px;"
        "padding: 6px 10px;"
        "min-height: 28px;"
        "}"
        "QDoubleSpinBox:disabled {"
        "background-color: #f8fafc;"
        "color: #94a3b8;"
        "}"
        "QTableWidget {"
        "background-color: #ffffff;"
        "alternate-background-color: #f8fafc;"
        "gridline-color: #e2e8f0;"
        "color: #0f172a;"
        "selection-background-color: #dbeafe;"
        "selection-color: #0f172a;"
        "border: 1px solid #dbe3ee;"
        "border-radius: 10px;"
        "}"
        "QHeaderView::section {"
        "background-color: #e2e8f0;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "padding: 4px 8px;"
        "font-weight: 600;"
        "}"
        "QDialogButtonBox QPushButton {"
        "min-width: 96px;"
        "padding: 8px 18px;"
        "border-radius: 10px;"
        "border: 1px solid #cbd5e1;"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "font-weight: 600;"
        "}"
        "QDialogButtonBox QPushButton:hover {"
        "background-color: #eff6ff;"
        "border-color: #93c5fd;"
        "}"));

    auto* rootLayout = new QVBoxLayout(dialog);
    rootLayout->setContentsMargins(24, 22, 24, 20);
    rootLayout->setSpacing(16);

    auto* headerCard = new QFrame(dialog);
    headerCard->setObjectName(QStringLiteral("routeEditCard"));
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(22, 18, 22, 18);
    headerLayout->setSpacing(8);

    auto* titleLabel = new QLabel(
        tr("Waypoint #%1").arg(QLocale().toString(waypointIndex + 1)),
        headerCard);
    titleLabel->setObjectName(QStringLiteral("routeEditTitle"));
    headerLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(
        tr("Linked part: %1").arg(routeWaypointPartSummary(waypoint, partPointByIndex)),
        headerCard);
    subtitleLabel->setObjectName(QStringLiteral("routeEditSubtitle"));
    subtitleLabel->setWordWrap(true);
    headerLayout->addWidget(subtitleLabel);

    auto* targetCountLabel = new QLabel(
        tr("Linked targets: %1").arg(QLocale().toString(waypoint.captureTargets.size())),
        headerCard);
    targetCountLabel->setObjectName(QStringLiteral("routeEditSubtitle"));
    targetCountLabel->setWordWrap(true);
    headerLayout->addWidget(targetCountLabel);

    if (waypoint.captureTargets.isEmpty()) {
        auto* hintLabel = new QLabel(
            tr("This waypoint has no linked capture target. Camera yaw and camera pitch are read-only for this edit."),
            headerCard);
        hintLabel->setObjectName(QStringLiteral("routeEditHint"));
        hintLabel->setWordWrap(true);
        headerLayout->addWidget(hintLabel);
    }

    rootLayout->addWidget(headerCard);

    QTableWidget* targetTableWidget = nullptr;
    if (hasCaptureTargets) {
        auto* targetsCard = new QFrame(dialog);
        targetsCard->setObjectName(QStringLiteral("routeEditCard"));
        auto* targetsLayout = new QVBoxLayout(targetsCard);
        targetsLayout->setContentsMargins(22, 16, 22, 16);
        targetsLayout->setSpacing(8);

        auto* targetsTitleLabel = new QLabel(tr("Capture Targets"), targetsCard);
        targetsTitleLabel->setObjectName(QStringLiteral("routeEditSubtitle"));
        targetsLayout->addWidget(targetsTitleLabel);

        targetTableWidget = new QTableWidget(targetsCard);
        targetTableWidget->setColumnCount(5);
        targetTableWidget->setAlternatingRowColors(true);
        targetTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        targetTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        targetTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        targetTableWidget->verticalHeader()->setVisible(false);
        targetTableWidget->setHorizontalHeaderLabels({
            tr("#"),
            tr("Part"),
            tr("Focal Ratio"),
            tr("Camera Yaw"),
            tr("Camera Pitch")
        });
        targetTableWidget->horizontalHeader()->setStretchLastSection(false);
        targetTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        targetTableWidget->setColumnWidth(0, 54);
        targetTableWidget->setColumnWidth(1, 210);
        targetTableWidget->setColumnWidth(2, 108);
        targetTableWidget->setColumnWidth(3, 112);
        targetTableWidget->setColumnWidth(4, 112);

        targetTableWidget->setRowCount(waypoint.captureTargets.size());
        auto createReadOnlyItem = [](const QString& text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            item->setTextAlignment(alignment);
            return item;
        };

        for (int targetIndex = 0; targetIndex < waypoint.captureTargets.size(); ++targetIndex) {
            const RouteCaptureTarget& captureTarget = waypoint.captureTargets.at(targetIndex);
            const QString targetName = routeCaptureTargetDisplayName(captureTarget, partPointByIndex, targetIndex + 1);
            targetTableWidget->setItem(
                targetIndex,
                0,
                createReadOnlyItem(QLocale().toString(targetIndex + 1), Qt::AlignCenter));
            targetTableWidget->setItem(
                targetIndex,
                1,
                createReadOnlyItem(targetName));
            targetTableWidget->setItem(
                targetIndex,
                2,
                createReadOnlyItem(QLocale().toString(captureTarget.focalLengthRatio, 'f', 2), Qt::AlignRight | Qt::AlignVCenter));
            targetTableWidget->setItem(
                targetIndex,
                3,
                createReadOnlyItem(QLocale().toString(captureTarget.cameraYawDeg, 'f', 2), Qt::AlignRight | Qt::AlignVCenter));
            targetTableWidget->setItem(
                targetIndex,
                4,
                createReadOnlyItem(QLocale().toString(captureTarget.cameraPitchDeg, 'f', 2), Qt::AlignRight | Qt::AlignVCenter));
        }

        targetTableWidget->setCurrentCell(std::max(0, state->selectedTargetIndex), 1);
        targetsLayout->addWidget(targetTableWidget, 1);
        rootLayout->addWidget(targetsCard, 1);
    }

    auto* formCard = new QFrame(dialog);
    formCard->setObjectName(QStringLiteral("routeEditCard"));
    auto* formLayout = new QFormLayout(formCard);
    formLayout->setContentsMargins(22, 18, 22, 18);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(12);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto configureCoordinateSpin = [](QDoubleSpinBox* spinBox, double value) {
        spinBox->setRange(-1000000000.0, 1000000000.0);
        spinBox->setDecimals(3);
        spinBox->setSingleStep(0.5);
        spinBox->setValue(value);
    };
    auto configureAngleSpin = [](QDoubleSpinBox* spinBox, double minimum, double maximum, double value) {
        spinBox->setRange(minimum, maximum);
        spinBox->setDecimals(2);
        spinBox->setSingleStep(1.0);
        spinBox->setValue(value);
    };
    auto configureFocalRatioSpin = [](QDoubleSpinBox* spinBox, double value) {
        spinBox->setRange(0.1, 64.0);
        spinBox->setDecimals(2);
        spinBox->setSingleStep(0.1);
        spinBox->setValue(normalizedRouteFocalLengthRatio(value));
    };

    auto* xSpinBox = new QDoubleSpinBox(formCard);
    auto* ySpinBox = new QDoubleSpinBox(formCard);
    auto* zSpinBox = new QDoubleSpinBox(formCard);
    auto* aircraftYawSpinBox = new QDoubleSpinBox(formCard);
    auto* gimbalPitchSpinBox = new QDoubleSpinBox(formCard);
    auto* focalLengthRatioSpinBox = new QDoubleSpinBox(formCard);
    auto* cameraYawSpinBox = new QDoubleSpinBox(formCard);
    auto* cameraPitchSpinBox = new QDoubleSpinBox(formCard);

    configureCoordinateSpin(xSpinBox, waypoint.localPoint.x);
    configureCoordinateSpin(ySpinBox, waypoint.localPoint.y);
    configureCoordinateSpin(zSpinBox, waypoint.localPoint.z);
    configureAngleSpin(aircraftYawSpinBox, -360.0, 360.0, waypoint.aircraftYawDeg);
    configureAngleSpin(gimbalPitchSpinBox, -180.0, 180.0, waypoint.gimbalPitchDeg);
    configureFocalRatioSpin(focalLengthRatioSpinBox, selectedTarget.focalLengthRatio);
    configureAngleSpin(cameraYawSpinBox, -360.0, 360.0, selectedTarget.cameraYawDeg);
    configureAngleSpin(cameraPitchSpinBox, -180.0, 180.0, selectedTarget.cameraPitchDeg);

    focalLengthRatioSpinBox->setEnabled(hasCaptureTargets);
    cameraYawSpinBox->setEnabled(hasCaptureTargets);
    cameraPitchSpinBox->setEnabled(hasCaptureTargets);

    formLayout->addRow(tr("X"), xSpinBox);
    formLayout->addRow(tr("Y"), ySpinBox);
    formLayout->addRow(tr("Z"), zSpinBox);
    formLayout->addRow(tr("Aircraft Yaw"), aircraftYawSpinBox);
    formLayout->addRow(tr("Gimbal Pitch"), gimbalPitchSpinBox);
    formLayout->addRow(tr("Focal Ratio"), focalLengthRatioSpinBox);
    formLayout->addRow(tr("Camera Yaw"), cameraYawSpinBox);
    formLayout->addRow(tr("Camera Pitch"), cameraPitchSpinBox);
    rootLayout->addWidget(formCard, 1);

    const auto refreshTargetTableRow = [this, targetTableWidget, waypointIndex](int row) {
        if (targetTableWidget == nullptr
            || waypointIndex < 0
            || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        const RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints.at(waypointIndex);
        if (row < 0 || row >= currentWaypoint.captureTargets.size()) {
            return;
        }

        const RouteCaptureTarget& captureTarget = currentWaypoint.captureTargets.at(row);
        if (QTableWidgetItem* focalItem = targetTableWidget->item(row, 2); focalItem != nullptr) {
            focalItem->setText(QLocale().toString(captureTarget.focalLengthRatio, 'f', 2));
        }
        if (QTableWidgetItem* yawItem = targetTableWidget->item(row, 3); yawItem != nullptr) {
            yawItem->setText(QLocale().toString(captureTarget.cameraYawDeg, 'f', 2));
        }
        if (QTableWidgetItem* pitchItem = targetTableWidget->item(row, 4); pitchItem != nullptr) {
            pitchItem->setText(QLocale().toString(captureTarget.cameraPitchDeg, 'f', 2));
        }
    };

    const auto applyCurrentEditorValues = [this,
                                           waypointIndex,
                                           state,
                                           xSpinBox,
                                           ySpinBox,
                                           zSpinBox,
                                           aircraftYawSpinBox,
                                           gimbalPitchSpinBox,
                                           focalLengthRatioSpinBox,
                                           cameraYawSpinBox,
                                           cameraPitchSpinBox,
                                           refreshTargetTableRow]() {
        if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints[waypointIndex];
        currentWaypoint.localPoint.x = xSpinBox->value();
        currentWaypoint.localPoint.y = ySpinBox->value();
        currentWaypoint.localPoint.z = zSpinBox->value();
        currentWaypoint.dh = zSpinBox->value();
        currentWaypoint.height = zSpinBox->value();
        currentWaypoint.aircraftYawDeg = aircraftYawSpinBox->value();
        currentWaypoint.gimbalPitchDeg = gimbalPitchSpinBox->value();

        if (state->hasCaptureTargets) {
            for (RouteCaptureTarget& captureTarget : currentWaypoint.captureTargets) {
                captureTarget.aircraftYawDeg = currentWaypoint.aircraftYawDeg;
                captureTarget.gimbalPitchDeg = currentWaypoint.gimbalPitchDeg;
            }

            if (state->selectedTargetIndex >= 0 && state->selectedTargetIndex < currentWaypoint.captureTargets.size()) {
                RouteCaptureTarget& activeTarget = currentWaypoint.captureTargets[state->selectedTargetIndex];
                activeTarget.focalLengthRatio = normalizedRouteFocalLengthRatio(focalLengthRatioSpinBox->value());
                activeTarget.cameraYawDeg = cameraYawSpinBox->value();
                activeTarget.cameraPitchDeg = cameraPitchSpinBox->value();
                refreshTargetTableRow(state->selectedTargetIndex);
            }
        }

        selectedRouteWaypointIndex_ = waypointIndex;
        selectedRouteWaypointTargetIndex_ = state->selectedTargetIndex;
        applyCurrentRouteToViewer();
        if (viewer_ != nullptr) {
            viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
        }
        updateRoutePlanningPanel();
    };

    const auto syncSelectedTargetFromTable = [this,
                                              waypointIndex,
                                              state,
                                              targetTableWidget,
                                              focalLengthRatioSpinBox,
                                              cameraYawSpinBox,
                                              cameraPitchSpinBox]() {
        if (!state->hasCaptureTargets
            || targetTableWidget == nullptr
            || waypointIndex < 0
            || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        const RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints.at(waypointIndex);
        int row = targetTableWidget->currentRow();
        if (row < 0 || row >= currentWaypoint.captureTargets.size()) {
            row = 0;
        }

        state->selectedTargetIndex = row;
        const RouteCaptureTarget& captureTarget = currentWaypoint.captureTargets.at(state->selectedTargetIndex);
        const QSignalBlocker focalRatioBlocker(focalLengthRatioSpinBox);
        const QSignalBlocker yawBlocker(cameraYawSpinBox);
        const QSignalBlocker pitchBlocker(cameraPitchSpinBox);
        focalLengthRatioSpinBox->setValue(normalizedRouteFocalLengthRatio(captureTarget.focalLengthRatio));
        cameraYawSpinBox->setValue(captureTarget.cameraYawDeg);
        cameraPitchSpinBox->setValue(captureTarget.cameraPitchDeg);

        selectedRouteWaypointTargetIndex_ = state->selectedTargetIndex;
        if (viewer_ != nullptr) {
            viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
        }
    };

    if (targetTableWidget != nullptr) {
        connect(targetTableWidget, &QTableWidget::currentCellChanged, dialog, [syncSelectedTargetFromTable](int, int, int, int) {
            syncSelectedTargetFromTable();
        });
        syncSelectedTargetFromTable();
    }

    const auto connectRealtimePreview = [this, dialog, applyCurrentEditorValues](QDoubleSpinBox* spinBox) {
        QObject::connect(
            spinBox,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            dialog,
            [this, applyCurrentEditorValues](double) {
            applyCurrentEditorValues();
        });
    };

    connectRealtimePreview(xSpinBox);
    connectRealtimePreview(ySpinBox);
    connectRealtimePreview(zSpinBox);
    connectRealtimePreview(aircraftYawSpinBox);
    connectRealtimePreview(gimbalPitchSpinBox);
    connectRealtimePreview(focalLengthRatioSpinBox);
    connectRealtimePreview(cameraYawSpinBox);
    connectRealtimePreview(cameraPitchSpinBox);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
    QPushButton* resetButton = buttonBox->addButton(tr("Reset"), QDialogButtonBox::ResetRole);
    connect(resetButton, &QPushButton::clicked, dialog, [this,
                                                          waypointIndex,
                                                          state,
                                                          targetTableWidget,
                                                          xSpinBox,
                                                          ySpinBox,
                                                          zSpinBox,
                                                          aircraftYawSpinBox,
                                                          gimbalPitchSpinBox,
                                                          focalLengthRatioSpinBox,
                                                          cameraYawSpinBox,
                                                          cameraPitchSpinBox,
                                                          refreshTargetTableRow,
                                                          applyCurrentEditorValues]() {
        if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints[waypointIndex];
        currentWaypoint = state->originalWaypoint;
        state->selectedTargetIndex = state->originalTargetIndex;

        if (targetTableWidget != nullptr && state->selectedTargetIndex >= 0) {
            const QSignalBlocker tableBlocker(targetTableWidget);
            targetTableWidget->setCurrentCell(state->selectedTargetIndex, 1);
        }

        const QSignalBlocker xBlocker(xSpinBox);
        const QSignalBlocker yBlocker(ySpinBox);
        const QSignalBlocker zBlocker(zSpinBox);
        const QSignalBlocker aircraftYawBlocker(aircraftYawSpinBox);
        const QSignalBlocker gimbalPitchBlocker(gimbalPitchSpinBox);
        const QSignalBlocker focalRatioBlocker(focalLengthRatioSpinBox);
        const QSignalBlocker cameraYawBlocker(cameraYawSpinBox);
        const QSignalBlocker cameraPitchBlocker(cameraPitchSpinBox);

        xSpinBox->setValue(state->originalWaypoint.localPoint.x);
        ySpinBox->setValue(state->originalWaypoint.localPoint.y);
        zSpinBox->setValue(state->originalWaypoint.localPoint.z);
        aircraftYawSpinBox->setValue(state->originalWaypoint.aircraftYawDeg);
        gimbalPitchSpinBox->setValue(state->originalWaypoint.gimbalPitchDeg);
        if (state->hasCaptureTargets
            && state->selectedTargetIndex >= 0
            && state->selectedTargetIndex < currentWaypoint.captureTargets.size()) {
            const RouteCaptureTarget& captureTarget = currentWaypoint.captureTargets.at(state->selectedTargetIndex);
            focalLengthRatioSpinBox->setValue(normalizedRouteFocalLengthRatio(captureTarget.focalLengthRatio));
            cameraYawSpinBox->setValue(captureTarget.cameraYawDeg);
            cameraPitchSpinBox->setValue(captureTarget.cameraPitchDeg);
        } else {
            focalLengthRatioSpinBox->setValue(1.0);
            cameraYawSpinBox->setValue(0.0);
            cameraPitchSpinBox->setValue(0.0);
        }

        if (targetTableWidget != nullptr) {
            for (int row = 0; row < currentWaypoint.captureTargets.size(); ++row) {
                refreshTargetTableRow(row);
            }
        }

        applyCurrentEditorValues();
    });
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    rootLayout->addWidget(buttonBox);
    enforceLightDialogButtonStyles(dialog);

    connect(dialog, &QDialog::finished, this, [this, waypointIndex, state, applyCurrentEditorValues](int result) {
        if (waypointIndex < 0 || waypointIndex >= currentPowerlineRoute_.waypoints.size()) {
            return;
        }

        RouteWaypoint& currentWaypoint = currentPowerlineRoute_.waypoints[waypointIndex];
        if (result != QDialog::Accepted) {
            currentWaypoint = state->originalWaypoint;
            selectedRouteWaypointIndex_ = waypointIndex;
            selectedRouteWaypointTargetIndex_ = state->originalTargetIndex;
            applyCurrentRouteToViewer();
            if (viewer_ != nullptr) {
                viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
            }
            updateRoutePlanningPanel();
            return;
        }

        applyCurrentEditorValues();

        QString geographicWarning;
        if (projectCoordinateSystems_.pointCloudCrs.code > 0 && projectCoordinateSystems_.geographicCrs.code > 0) {
            QPointF geographicPoint;
            QString errorMessage;
            if (CrsTransformService::transformPoint(
                    projectCoordinateSystems_.pointCloudCrs,
                    projectCoordinateSystems_.geographicCrs,
                    QPointF(currentWaypoint.localPoint.x, currentWaypoint.localPoint.y),
                    &geographicPoint,
                    &errorMessage)) {
                currentWaypoint.longitude = geographicPoint.x();
                currentWaypoint.latitude = geographicPoint.y();
            } else {
                geographicWarning = errorMessage.isEmpty()
                    ? tr("Waypoint local position was updated, but geographic coordinates could not be synchronized.")
                    : errorMessage;
            }
        }

        selectedRouteWaypointIndex_ = waypointIndex;
        selectedRouteWaypointTargetIndex_ = state->selectedTargetIndex;
        applyCurrentRouteToViewer();
        if (viewer_ != nullptr) {
            viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
        }
        updateRoutePlanningPanel();
        rebuildProjectTree();
        updateActionState();
        if (viewer_ != nullptr) {
            viewer_->focusOnPoint(currentWaypoint.localPoint, 0.2);
        }

        if (!geographicWarning.isEmpty()) {
            showUserMessage(LogLevel::Warning, geographicWarning, 4500);
        } else {
            showUserMessage(
                LogLevel::Info,
                tr("Updated route waypoint #%1.").arg(QLocale().toString(waypointIndex + 1)),
                3000);
        }
    });

    dialog->open();
    dialog->raise();
    dialog->activateWindow();
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
            waypoint.primaryPartIndex = waypoint.captureTargets.isEmpty()
                ? -1
                : waypoint.captureTargets.first().partIndex;
        }

        if (waypoint.primaryPartIndex > 0 && !waypoint.captureTargets.isEmpty()) {
            const int candidateFileId = waypoint.captureTargets.first().partFileId;
            if (candidateFileId > 0) {
                waypoint.rawKeyId = candidateFileId;
            }
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

    selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty()
        ? -1
        : std::clamp(waypointIndex, 0, currentPowerlineRoute_.waypoints.size() - 1);
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

    selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty()
        ? -1
        : std::clamp(selectedRouteWaypointIndex_, 0, currentPowerlineRoute_.waypoints.size() - 1);
    viewer_->setSelectedInspectionRouteWaypointIndex(selectedRouteWaypointIndex_);
    if (selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()) {
        const int targetCount = currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_).captureTargets.size();
        selectedRouteWaypointTargetIndex_ = targetCount > 0
            ? std::clamp(selectedRouteWaypointTargetIndex_, 0, targetCount - 1)
            : -1;
    } else {
        selectedRouteWaypointTargetIndex_ = -1;
    }
    viewer_->setSelectedInspectionRouteWaypointTargetIndex(selectedRouteWaypointTargetIndex_);
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

bool MainWindow::importTowerFile(const QString& filePath, bool updateLink, bool notify)
{
    if (viewer_ == nullptr) {
        return false;
    }

    QList<TowerRecord> towers;
    QString errorMessage;
    if (!importTowerLiTowerFile(filePath, &towers, &errorMessage)) {
        if (notify) {
            showUserMessage(
                LogLevel::Error,
                errorMessage.isEmpty() ? tr("Failed to import tower file.") : errorMessage,
                5000);
        }
        return false;
    }

    viewer_->setTowerMarkers(towers);
    if (updateLink) {
        linkedTowerFilePath_ = QFileInfo(filePath).absoluteFilePath();
    }
    if (notify) {
        showUserMessage(
            LogLevel::Info,
            tr("Imported tower file: %1").arg(QFileInfo(filePath).fileName()),
            3500);
    }
    updateActionState();
    return true;
}

bool MainWindow::exportTowerFile(const QString& filePath, bool updateLink, bool notify)
{
    if (viewer_ == nullptr) {
        return false;
    }

    QString normalizedPath = filePath;
    if (QFileInfo(normalizedPath).suffix().isEmpty()) {
        normalizedPath += QStringLiteral(".LiTower");
    }

    QString errorMessage;
    if (!exportTowerLiTowerFile(normalizedPath, viewer_->towerMarkers(), &errorMessage)) {
        if (notify) {
            showUserMessage(
                LogLevel::Error,
                errorMessage.isEmpty() ? tr("Failed to save tower file.") : errorMessage,
                5000);
        }
        return false;
    }

    if (updateLink) {
        linkedTowerFilePath_ = QFileInfo(normalizedPath).absoluteFilePath();
    }
    if (notify) {
        showUserMessage(
            LogLevel::Info,
            tr("Tower file saved: %1").arg(QFileInfo(normalizedPath).fileName()),
            3000);
    }
    updateActionState();
    return true;
}

bool MainWindow::reloadLinkedTowerFile(bool notify)
{
    const QString linkedPath = linkedTowerFilePath_.trimmed();
    if (linkedPath.isEmpty()) {
        if (notify) {
            showUserMessage(LogLevel::Warning, tr("No linked tower file to reload."), 3000);
        }
        return false;
    }

    if (!QFileInfo::exists(linkedPath)) {
        if (notify) {
            showUserMessage(LogLevel::Error, tr("Linked tower file was not found."), 4000);
        }
        return false;
    }

    if (!importTowerFile(linkedPath, false, false)) {
        return false;
    }
    if (notify) {
        showUserMessage(
            LogLevel::Info,
            tr("Reloaded tower file: %1").arg(QFileInfo(linkedPath).fileName()),
            3000);
    }
    return true;
}

bool MainWindow::loadProjectFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        showUserMessage(LogLevel::Error, tr("Failed to open project file."), 5000);
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) {
        showUserMessage(LogLevel::Error, tr("Project file format is invalid."), 5000);
        return false;
    }

    const QJsonObject projectObject = document.object();
    linkedTowerFilePath_.clear();
    QStringList pointCloudFilePaths;
    const QJsonArray pointCloudFilesArray = projectObject.value(QStringLiteral("pointCloudFilePaths")).toArray();
    for (const QJsonValue& pathValue : pointCloudFilesArray) {
        const QString resolvedPath = resolveProjectPath(filePath, pathValue.toString());
        if (!resolvedPath.isEmpty()) {
            pointCloudFilePaths.append(resolvedPath);
        }
    }
    if (pointCloudFilePaths.isEmpty()) {
        const QString legacyPointCloudPath = resolveProjectPath(
            filePath,
            projectObject.value(QStringLiteral("pointCloudFilePath")).toString());
        if (!legacyPointCloudPath.isEmpty()) {
            pointCloudFilePaths.append(legacyPointCloudPath);
        }
    }
    if (pointCloudFilePaths.isEmpty()) {
        showUserMessage(LogLevel::Error, tr("Project file does not contain any point cloud paths."), 5000);
        return false;
    }

    QString errorMessage;
    if (!viewer_->loadPointCloudFiles(pointCloudFilePaths, &errorMessage)) {
        syncUiFromViewer();
        showUserMessage(LogLevel::Error, errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage, 6000);
        return false;
    }

    const QJsonObject visualizationObject = projectObject.value(QStringLiteral("visualization")).toObject();
    const PointCloudVisualizationOptions defaults = viewer_->visualizationOptions();
    viewer_->setPointSize(visualizationObject.value(QStringLiteral("pointSize")).toInt(static_cast<int>(defaults.pointSize)));
    viewer_->setPointOpacity(visualizationObject.value(QStringLiteral("pointOpacity")).toInt(static_cast<int>(defaults.pointOpacity * 100.0f)));
    viewer_->setDepthCueStrength(visualizationObject.value(QStringLiteral("depthCueStrength")).toInt(static_cast<int>(defaults.depthCueStrength * 100.0f)));
    viewer_->setEdlStrength(visualizationObject.value(QStringLiteral("edlStrength")).toInt(static_cast<int>(defaults.edlStrength * 100.0f)));
    viewer_->setClassificationColorMap(classificationColorMapFromJson(
        visualizationObject.value(QStringLiteral("classificationColors")).toObject(),
        defaults.classificationColors));
    viewer_->setClassificationVisibilityMap(classificationVisibilityMapFromJson(
        visualizationObject.value(QStringLiteral("classificationVisibility")).toObject(),
        defaults.classificationVisibility));
    viewer_->setClassificationFallbackColor(colorFromJson(
        visualizationObject.value(QStringLiteral("classificationFallbackColor")).toObject(),
        defaults.classificationFallbackColor));
    classificationNameOverrides_ = classificationNameMapFromJson(
        visualizationObject.value(QStringLiteral("classificationNameOverrides")).toObject());
    viewer_->setColorMode(visualizationObject.value(QStringLiteral("colorMode")).toInt(static_cast<int>(defaults.colorMode)));
    viewer_->setSingleColor(colorFromJson(visualizationObject.value(QStringLiteral("singleColor")).toObject(), defaults.singleColor));
    viewer_->setBackgroundColor(colorFromJson(visualizationObject.value(QStringLiteral("backgroundColor")).toObject(), defaults.backgroundColor));
    viewer_->setInspectionRouteWaypointColor(colorFromJson(
        visualizationObject.value(QStringLiteral("routeWaypointColor")).toObject(),
        viewer_->inspectionRouteWaypointColor()));
    viewer_->setInspectionRoutePartPointColor(colorFromJson(
        visualizationObject.value(QStringLiteral("routePartPointColor")).toObject(),
        viewer_->inspectionRoutePartPointColor()));
    viewer_->setInspectionRouteTrajectoryColor(colorFromJson(
        visualizationObject.value(QStringLiteral("routeTrajectoryColor")).toObject(),
        viewer_->inspectionRouteTrajectoryColor()));
    viewer_->setUseRoundSplats(visualizationObject.value(QStringLiteral("useRoundSplats")).toBool(defaults.useRoundSplats));
    viewer_->setShowAxes(visualizationObject.value(QStringLiteral("showAxes")).toBool(defaults.showAxes));
    viewer_->setShowBoundingBox(visualizationObject.value(QStringLiteral("showBoundingBox")).toBool(defaults.showBoundingBox));

    InteractionOptions interactionOptions = viewer_->interactionOptions();
    const QJsonObject interactionObject = projectObject.value(QStringLiteral("interaction")).toObject();
    interactionOptions.invertOrbitDrag = interactionObject.value(QStringLiteral("invertOrbitDrag")).toBool(interactionOptions.invertOrbitDrag);
    interactionOptions.invertPanDrag = interactionObject.value(QStringLiteral("invertPanDrag")).toBool(interactionOptions.invertPanDrag);
    interactionOptions.invertWheelZoom = interactionObject.value(QStringLiteral("invertWheelZoom")).toBool(interactionOptions.invertWheelZoom);
    interactionOptions.wheelZoomSensitivityPercent = interactionObject.value(QStringLiteral("wheelZoomSensitivityPercent"))
        .toInt(interactionOptions.wheelZoomSensitivityPercent);
    viewer_->setInteractionOptions(interactionOptions);

    const QJsonObject measurementObject = projectObject.value(QStringLiteral("measurement")).toObject();
    clearanceWarningThresholdMeters_ = measurementObject.value(QStringLiteral("clearanceThresholdMeters")).toDouble(clearanceWarningThresholdMeters_);
    if (clearanceThresholdSpinBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceThresholdSpinBox_);
        clearanceThresholdSpinBox_->setValue(clearanceWarningThresholdMeters_);
    }
    const QJsonObject analysisObject = projectObject.value(QStringLiteral("analysis")).toObject();
    clearanceRulePreset_ = static_cast<ClearanceRulePreset>(analysisObject.value(QStringLiteral("clearanceRulePreset")).toInt(static_cast<int>(clearanceRulePreset_)));
    vegetationSearchRadiusMeters_ = analysisObject.value(QStringLiteral("vegetationSearchRadiusMeters")).toDouble(vegetationSearchRadiusMeters_);
    vegetationClusterGapMeters_ = analysisObject.value(QStringLiteral("vegetationClusterGapMeters")).toDouble(vegetationClusterGapMeters_);
    vegetationClusterPointCount_ = analysisObject.value(QStringLiteral("vegetationClusterPointCount")).toInt(vegetationClusterPointCount_);
    preferVegetationClassification_ = analysisObject.value(QStringLiteral("preferVegetationClassification")).toBool(preferVegetationClassification_);
    if (clearanceRulePresetComboBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceRulePresetComboBox_);
        const int presetIndex = clearanceRulePresetComboBox_->findData(static_cast<int>(clearanceRulePreset_));
        clearanceRulePresetComboBox_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 0);
    }
    if (vegetationSearchRadiusSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationSearchRadiusSpinBox_);
        vegetationSearchRadiusSpinBox_->setValue(vegetationSearchRadiusMeters_);
    }
    if (vegetationClusterGapSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterGapSpinBox_);
        vegetationClusterGapSpinBox_->setValue(vegetationClusterGapMeters_);
    }
    if (vegetationClusterPointCountSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterPointCountSpinBox_);
        vegetationClusterPointCountSpinBox_->setValue(vegetationClusterPointCount_);
    }
    if (preferVegetationClassificationCheckBox_ != nullptr) {
        const QSignalBlocker blocker(preferVegetationClassificationCheckBox_);
        preferVegetationClassificationCheckBox_->setChecked(preferVegetationClassification_);
    }

    routePlanningOptions_ = routePlanningOptionsFromJson(projectObject.value(QStringLiteral("routePlanning")).toObject());
    syncProjectCoordinateSystemsFromRoutePlanning();

    const QJsonObject projectPropertiesObject = projectObject.value(QStringLiteral("projectProperties")).toObject();
    const QJsonObject coordinateSystemsObject = projectPropertiesObject.value(QStringLiteral("coordinateSystems")).toObject();
    if (!coordinateSystemsObject.isEmpty()) {
        projectCoordinateSystems_.pointCloudCrs = coordinateSystemRefFromJson(
            coordinateSystemsObject.value(QStringLiteral("pointCloudCrs")).toObject(),
            projectCoordinateSystems_.pointCloudCrs);
        projectCoordinateSystems_.geographicCrs = coordinateSystemRefFromJson(
            coordinateSystemsObject.value(QStringLiteral("geographicCrs")).toObject(),
            projectCoordinateSystems_.geographicCrs);
        if (projectCoordinateSystems_.geographicCrs.code <= 0) {
            projectCoordinateSystems_.geographicCrs = defaultGeographicCoordinateSystem();
        }
    }
    syncRoutePlanningFromProjectCoordinateSystems();

    const QJsonObject classificationEditsObject =
        projectObject.value(QStringLiteral("classificationEdits")).toObject();
    viewer_->setClassificationEditStore(classificationEditsFromJson(
        classificationEditsObject,
        [&filePath](const QString& storedDatasetPath) {
            return resolveProjectPath(filePath, storedDatasetPath);
        }));

    QList<TowerMarker> towerMarkers;
    const QJsonArray towersArray = projectObject.value(QStringLiteral("towerMarkers")).toArray();
    for (const QJsonValue& towerValue : towersArray) {
        const TowerRecord towerRecord = towerRecordFromJson(towerValue.toObject());
        if (towerRecord.name.isEmpty()) {
            continue;
        }
        towerMarkers.append(towerRecord);
    }
    viewer_->setTowerMarkers(towerMarkers);

    const QJsonObject towerFileObject = projectObject.value(QStringLiteral("towerFile")).toObject();
    const QString storedTowerRelativePath = towerFileObject.value(QStringLiteral("relativePath")).toString().trimmed();
    if (!storedTowerRelativePath.isEmpty()) {
        linkedTowerFilePath_ = resolveProjectPath(filePath, storedTowerRelativePath);
        if (!QFileInfo::exists(linkedTowerFilePath_)) {
            showUserMessage(
                LogLevel::Warning,
                tr("Linked tower file is missing: %1")
                    .arg(QFileInfo(storedTowerRelativePath).fileName()),
                5000);
        }
    }

    QList<InspectionIssue> inspectionIssues;
    const QJsonArray issuesArray = projectObject.value(QStringLiteral("inspectionIssues")).toArray();
    for (const QJsonValue& issueValue : issuesArray) {
        const InspectionIssue issue = inspectionIssueFromJson(issueValue.toObject());
        if (issue.title.trimmed().isEmpty()) {
            continue;
        }
        inspectionIssues.append(issue);
    }
    viewer_->setInspectionIssues(inspectionIssues);

    vegetationRiskResults_.clear();
    const QJsonArray vegetationRisksArray = analysisObject.value(QStringLiteral("vegetationRisks")).toArray();
    for (const QJsonValue& riskValue : vegetationRisksArray) {
        const VegetationRiskRecord riskRecord = vegetationRiskRecordFromJson(riskValue.toObject());
        if (riskRecord.title.trimmed().isEmpty()) {
            continue;
        }
        vegetationRiskResults_.append(riskRecord);
    }
    selectedVegetationRiskIndex_ = vegetationRiskResults_.isEmpty() ? -1 : 0;

    currentPowerlineRoute_ = PowerlineRouteDocument();
    linkedRouteFilePath_.clear();
    const QJsonObject routeFileObject = projectObject.value(QStringLiteral("routeFile")).toObject();
    const QString storedRouteRelativePath = routeFileObject.value(QStringLiteral("relativePath")).toString().trimmed();
    if (!storedRouteRelativePath.isEmpty()) {
        linkedRouteFilePath_ = resolveProjectPath(filePath, storedRouteRelativePath);
        if (QFileInfo::exists(linkedRouteFilePath_)) {
            importRouteFile(linkedRouteFilePath_, false, false);
        } else {
            showUserMessage(
                LogLevel::Warning,
                tr("Linked route file is missing: %1").arg(QFileInfo(storedRouteRelativePath).fileName()),
                5000);
        }
    } else {
        const QJsonArray routesArray = projectObject.value(QStringLiteral("routes")).toArray();
        if (!routesArray.isEmpty()) {
            const InspectionRoute legacyRoute = inspectionRouteFromJson(routesArray.first().toObject());
            if (!legacyRoute.waypoints.isEmpty()) {
                currentPowerlineRoute_ = createPowerlineRouteFromInspectionRoute(legacyRoute, legacyRoute.name);
            }
        }
        selectedRouteWaypointIndex_ = currentPowerlineRoute_.waypoints.isEmpty() ? -1 : 0;
        selectedRouteWaypointTargetIndex_ = -1;
        applyCurrentRouteToViewer();
    }

    currentProjectFilePath_ = filePath;
    recordRecentProjectFilePath(filePath);
    classificationEditsDirty_ = false;
    setTowerEditingEnabled(false);
    const QString languageCode = projectObject.value(QStringLiteral("language")).toString();
    if (languageCode == QStringLiteral("zh_CN")) {
        applyLanguage(UiLanguage::Chinese);
    } else if (languageCode == QStringLiteral("en")) {
        applyLanguage(UiLanguage::English);
    }
    syncUiFromViewer();
    updateTowerPanel();
    updateRoutePlanningPanel();
    showUserMessage(LogLevel::Info, tr("Project loaded: %1").arg(QFileInfo(filePath).fileName()), 4000);
    return true;
}

bool MainWindow::saveProjectFile(const QString& filePath)
{
    if (viewer_ == nullptr || viewer_->currentFilePaths().isEmpty()) {
        showUserMessage(LogLevel::Warning, tr("Load a point cloud before saving a project."), 4000);
        return false;
    }

    syncRoutePlanningFromProjectCoordinateSystems();
    if (aircraftProfileComboBox_ != nullptr) {
        const QVariant profileValue = aircraftProfileComboBox_->currentData();
        const int profileIndex = profileValue.isValid() ? profileValue.toInt() : static_cast<int>(routePlanningOptions_.aircraftProfile);
        routePlanningOptions_.aircraftProfile = static_cast<DjiAircraftProfile>(profileIndex);
    }
    if (routeSafetyHeightSpinBox_ != nullptr) {
        routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(routeSafetyHeightSpinBox_->value());
    }
    if (routeWaypointSpeedSpinBox_ != nullptr) {
        routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(routeWaypointSpeedSpinBox_->value());
    }
    if (routeWaypointSpacingSpinBox_ != nullptr) {
        routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(routeWaypointSpacingSpinBox_->value());
    }
    if (routeSmoothingStrengthSpinBox_ != nullptr) {
        routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(routeSmoothingStrengthSpinBox_->value());
    }
    if (routeHeightOffsetSpinBox_ != nullptr) {
        routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(routeHeightOffsetSpinBox_->value());
    }

    QJsonObject visualizationObject {
        { QStringLiteral("pointSize"), static_cast<int>(std::lround(viewer_->visualizationOptions().pointSize)) },
        { QStringLiteral("pointOpacity"), static_cast<int>(std::lround(viewer_->visualizationOptions().pointOpacity * 100.0f)) },
        { QStringLiteral("depthCueStrength"), static_cast<int>(std::lround(viewer_->visualizationOptions().depthCueStrength * 100.0f)) },
        { QStringLiteral("edlStrength"), static_cast<int>(std::lround(viewer_->visualizationOptions().edlStrength * 100.0f)) },
        { QStringLiteral("colorMode"), static_cast<int>(viewer_->visualizationOptions().colorMode) },
        { QStringLiteral("singleColor"), colorToJson(viewer_->visualizationOptions().singleColor) },
        { QStringLiteral("classificationColors"), classificationColorMapToJson(viewer_->visualizationOptions().classificationColors) },
        { QStringLiteral("classificationVisibility"), classificationVisibilityMapToJson(viewer_->visualizationOptions().classificationVisibility) },
        { QStringLiteral("classificationNameOverrides"), classificationNameMapToJson(classificationNameOverrides_) },
        { QStringLiteral("classificationFallbackColor"), colorToJson(viewer_->visualizationOptions().classificationFallbackColor) },
        { QStringLiteral("backgroundColor"), colorToJson(viewer_->visualizationOptions().backgroundColor) },
        { QStringLiteral("routeWaypointColor"), colorToJson(viewer_->inspectionRouteWaypointColor()) },
        { QStringLiteral("routePartPointColor"), colorToJson(viewer_->inspectionRoutePartPointColor()) },
        { QStringLiteral("routeTrajectoryColor"), colorToJson(viewer_->inspectionRouteTrajectoryColor()) },
        { QStringLiteral("useRoundSplats"), viewer_->visualizationOptions().useRoundSplats },
        { QStringLiteral("showAxes"), viewer_->visualizationOptions().showAxes },
        { QStringLiteral("showBoundingBox"), viewer_->visualizationOptions().showBoundingBox }
    };

    QJsonObject interactionObject {
        { QStringLiteral("invertOrbitDrag"), viewer_->interactionOptions().invertOrbitDrag },
        { QStringLiteral("invertPanDrag"), viewer_->interactionOptions().invertPanDrag },
        { QStringLiteral("invertWheelZoom"), viewer_->interactionOptions().invertWheelZoom },
        { QStringLiteral("wheelZoomSensitivityPercent"), viewer_->interactionOptions().wheelZoomSensitivityPercent }
    };

    QJsonObject measurementObject {
        { QStringLiteral("clearanceThresholdMeters"), clearanceWarningThresholdMeters_ }
    };
    QJsonObject analysisObject {
        { QStringLiteral("clearanceRulePreset"), static_cast<int>(clearanceRulePreset_) },
        { QStringLiteral("vegetationSearchRadiusMeters"), vegetationSearchRadiusMeters_ },
        { QStringLiteral("vegetationClusterGapMeters"), vegetationClusterGapMeters_ },
        { QStringLiteral("vegetationClusterPointCount"), vegetationClusterPointCount_ },
        { QStringLiteral("preferVegetationClassification"), preferVegetationClassification_ }
    };
    QJsonObject routePlanningObject = routePlanningOptionsToJson(routePlanningOptions_);
    QJsonObject classificationEditsObject = classificationEditsToJson(
        viewer_->classificationEditStore(),
        [&filePath](const QString& datasetPath) {
            return projectRelativePathFor(filePath, datasetPath);
        });
    QJsonObject projectPropertiesObject {
        { QStringLiteral("coordinateSystems"), projectCoordinateSystemsToJson(projectCoordinateSystems_) }
    };
    QJsonObject towerFileObject;
    if (!linkedTowerFilePath_.trimmed().isEmpty()) {
        towerFileObject.insert(QStringLiteral("format"), QStringLiteral("LiTower"));
        towerFileObject.insert(
            QStringLiteral("relativePath"),
            projectRelativePathFor(filePath, linkedTowerFilePath_));
    }

    QJsonArray towersArray;
    for (const TowerMarker& towerMarker : viewer_->towerMarkers()) {
        towersArray.append(towerRecordToJson(towerMarker));
    }

    QJsonArray inspectionIssuesArray;
    for (const InspectionIssue& issue : viewer_->inspectionIssues()) {
        inspectionIssuesArray.append(inspectionIssueToJson(issue));
    }
    QJsonArray vegetationRisksArray;
    for (const VegetationRiskRecord& riskRecord : vegetationRiskResults_) {
        vegetationRisksArray.append(vegetationRiskRecordToJson(riskRecord));
    }
    analysisObject.insert(QStringLiteral("vegetationRisks"), vegetationRisksArray);

    QJsonObject routeFileObject;
    if (!linkedRouteFilePath_.trimmed().isEmpty()) {
        routeFileObject.insert(QStringLiteral("format"), QStringLiteral("PowerlineJson"));
        routeFileObject.insert(
            QStringLiteral("relativePath"),
            projectRelativePathFor(filePath, linkedRouteFilePath_));
    }

    QJsonArray routesArray;
    if (!currentPowerlineRoute_.waypoints.isEmpty()) {
        routesArray.append(inspectionRouteToJson(toInspectionRouteExportView(currentPowerlineRoute_)));
    }

    QJsonArray pointCloudFilesArray;
    for (const QString& pointCloudFilePath : viewer_->currentFilePaths()) {
        pointCloudFilesArray.append(projectRelativePathFor(filePath, pointCloudFilePath));
    }

    QJsonObject projectObject {
        { QStringLiteral("version"), 8 },
        { QStringLiteral("pointCloudFilePaths"), pointCloudFilesArray },
        { QStringLiteral("pointCloudFilePath"), viewer_->currentFilePath().isEmpty() ? QString() : projectRelativePathFor(filePath, viewer_->currentFilePath()) },
        { QStringLiteral("language"), languageCodeFor(currentLanguage_) },
        { QStringLiteral("projectProperties"), projectPropertiesObject },
        { QStringLiteral("visualization"), visualizationObject },
        { QStringLiteral("interaction"), interactionObject },
        { QStringLiteral("measurement"), measurementObject },
        { QStringLiteral("analysis"), analysisObject },
        { QStringLiteral("routePlanning"), routePlanningObject },
        { QStringLiteral("classificationEdits"), classificationEditsObject },
        { QStringLiteral("routes"), routesArray },
        { QStringLiteral("routeFile"), routeFileObject },
        { QStringLiteral("towerFile"), towerFileObject },
        { QStringLiteral("towerMarkers"), towersArray },
        { QStringLiteral("inspectionIssues"), inspectionIssuesArray }
    };

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        showUserMessage(LogLevel::Error, tr("Failed to save project file."), 5000);
        return false;
    }

    file.write(QJsonDocument(projectObject).toJson(QJsonDocument::Indented));
    file.close();

    bool routeFileSyncOk = true;
    QString routeFileSyncError;
    if (!linkedRouteFilePath_.trimmed().isEmpty() && !currentPowerlineRoute_.waypoints.isEmpty()) {
        if (!exportRouteFile(linkedRouteFilePath_, false, false)) {
            routeFileSyncOk = false;
            routeFileSyncError = tr("Failed to sync linked route JSON.");
        }
    }

    bool towerFileSyncOk = true;
    QString towerFileSyncError;
    if (!linkedTowerFilePath_.trimmed().isEmpty()) {
        if (!exportTowerLiTowerFile(linkedTowerFilePath_, viewer_->towerMarkers(), &towerFileSyncError)) {
            towerFileSyncOk = false;
        }
    }

    currentProjectFilePath_ = filePath;
    recordRecentProjectFilePath(filePath);
    classificationEditsDirty_ = false;
    rebuildProjectTree();
    if (towerFileSyncOk && routeFileSyncOk) {
        showUserMessage(LogLevel::Info, tr("Project saved: %1").arg(QFileInfo(filePath).fileName()), 3000);
    } else if (!routeFileSyncOk && towerFileSyncOk) {
        showUserMessage(
            LogLevel::Warning,
            tr("Project saved, but route file sync failed: %1")
                .arg(routeFileSyncError.isEmpty() ? tr("Unknown error") : routeFileSyncError),
            5000);
    } else {
        showUserMessage(
            LogLevel::Warning,
            tr("Project saved, but tower file sync failed: %1")
                .arg(towerFileSyncError.isEmpty() ? tr("Unknown error") : towerFileSyncError),
            5000);
    }
    return true;
}

void MainWindow::openPointCloud()
{
    hideBackstageView();
    const QStringList filePaths = showStyledOpenFileNamesDialog(
        this,
        tr("Open LAS Point Clouds"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Open cancelled."), 2000);
        return;
    }

    loadPointCloudFiles(filePaths);
}

void MainWindow::addPointCloudFiles()
{
    hideBackstageView();
    const QStringList filePaths = showStyledOpenFileNamesDialog(
        this,
        tr("Add LAS Point Clouds"),
        QString(),
        tr("LAS Files (*.las *.laz);;All Files (*.*)"));

    if (filePaths.isEmpty()) {
        showUserMessage(LogLevel::Info, tr("Add datasets cancelled."), 2000);
        return;
    }

    appendPointCloudFiles(filePaths);
}

bool MainWindow::loadPointCloudFile(const QString& filePath)
{
    return loadPointCloudFiles(QStringList { filePath });
}

bool MainWindow::loadPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }

    QString errorMessage;
    if (viewer_->loadPointCloudFiles(filePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        linkedTowerFilePath_.clear();
        linkedRouteFilePath_.clear();
        setTowerEditingEnabled(false);
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        const QString successMessage = filePaths.size() == 1
            ? tr("Loaded %1. %2").arg(QFileInfo(filePaths.constFirst()).fileName(), errorMessage)
            : tr("Loaded %1 datasets. %2")
                  .arg(QLocale().toString(filePaths.size()))
                  .arg(errorMessage);
        showUserMessage(LogLevel::Info, successMessage, 4500);
        return true;
    }

    syncUiFromViewer();
    showUserMessage(
        LogLevel::Error,
        errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
        6000);
    return false;
}

bool MainWindow::appendPointCloudFiles(const QStringList& filePaths)
{
    if (viewer_ == nullptr) {
        return false;
    }
    QString errorMessage;
    if (!viewer_->appendPointCloudFiles(filePaths, &errorMessage)) {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
        return false;
    }

    currentProjectFilePath_.clear();
    linkedTowerFilePath_.clear();
    linkedRouteFilePath_.clear();
    setTowerEditingEnabled(false);
    vegetationRiskResults_.clear();
    selectedVegetationRiskIndex_ = -1;
    currentPowerlineRoute_ = PowerlineRouteDocument();
    selectedRouteWaypointIndex_ = -1;
    syncUiFromViewer();
    showUserMessage(
        LogLevel::Info,
        errorMessage.isEmpty() ? tr("Datasets added.") : errorMessage,
        4500);
    return true;
}

void MainWindow::clearPointCloud()
{
    currentProjectFilePath_.clear();
    linkedTowerFilePath_.clear();
    linkedRouteFilePath_.clear();
    classificationEditsDirty_ = false;
    setTowerEditingEnabled(false);
    vegetationRiskResults_.clear();
    selectedVegetationRiskIndex_ = -1;
    currentPowerlineRoute_ = PowerlineRouteDocument();
    selectedRouteWaypointIndex_ = -1;
    viewer_->clearPointCloud();
}

void MainWindow::removeSelectedDataset()
{
    if (viewer_ == nullptr || projectTreeWidget_ == nullptr) {
        return;
    }

    const QString datasetPath = selectedDatasetPath();
    if (datasetPath.isEmpty()) {
        showUserMessage(LogLevel::Warning, tr("Select a dataset in the project tree before removing it."), 3000);
        return;
    }
    QStringList remainingFilePaths = viewer_->currentFilePaths();
    remainingFilePaths.removeAll(datasetPath);

    if (remainingFilePaths.isEmpty()) {
        clearPointCloud();
        showUserMessage(LogLevel::Info, tr("Dataset removed. The project is now empty."), 3000);
        return;
    }

    const QList<TowerMarker> towerMarkers = viewer_->towerMarkers();
    const QList<InspectionIssue> inspectionIssues = viewer_->inspectionIssues();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    QHash<QString, bool> datasetVisibility;
    for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
        datasetVisibility.insert(datasetInfo.filePath.toLower(), datasetInfo.visible);
    }
    QString errorMessage;
    if (viewer_->loadPointCloudFiles(remainingFilePaths, &errorMessage)) {
        currentProjectFilePath_.clear();
        linkedRouteFilePath_.clear();
        setTowerEditingEnabled(false);
        for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
            const auto visibilityIt = datasetVisibility.constFind(datasetInfo.filePath.toLower());
            if (visibilityIt != datasetVisibility.constEnd() && !visibilityIt.value()) {
                viewer_->setPointCloudDatasetVisible(datasetInfo.filePath, false);
            }
        }
        viewer_->setTowerMarkers(towerMarkers);
        viewer_->setInspectionIssues(inspectionIssues);
        viewer_->setSelectedTowerIndex(selectedTowerIndex);
        viewer_->setSelectedIssueIndex(selectedIssueIndex);
        vegetationRiskResults_.clear();
        selectedVegetationRiskIndex_ = -1;
        currentPowerlineRoute_ = PowerlineRouteDocument();
        selectedRouteWaypointIndex_ = -1;
        viewer_->clearInspectionRouteWaypoints();
        syncUiFromViewer();
        showUserMessage(LogLevel::Info, tr("Dataset removed from the project."), 3000);
    } else {
        syncUiFromViewer();
        showUserMessage(
            LogLevel::Error,
            errorMessage.isEmpty() ? tr("Failed to load point cloud.") : errorMessage,
            6000);
    }
}

void MainWindow::choosePointColor()
{
    const QColor initialColor = viewer_->visualizationOptions().singleColor;
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Single Point Color"));
    if (chosenColor.isValid()) {
        viewer_->setSingleColor(chosenColor);
        if (viewer_->visualizationOptions().colorMode != PointCloudColorMode::SingleColor) {
            viewer_->setColorMode(PointCloudColorMode::SingleColor);
        }
    }
}

void MainWindow::chooseBackgroundColor()
{
    const QColor initialColor = viewer_->visualizationOptions().backgroundColor;
    const QColor chosenColor = showStyledColorDialog(this, initialColor, tr("Choose Background Color"));
    if (chosenColor.isValid()) {
        viewer_->setBackgroundColor(chosenColor);
    }
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

void MainWindow::applyOfficeTheme(Qtitan::RibbonStyle::Theme theme)
{
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        ribbonStyle->setTheme(theme);
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

    QPalette toolTipPalette = QToolTip::palette();
    toolTipPalette.setColor(QPalette::ToolTipBase, QColor(248, 250, 252));
    toolTipPalette.setColor(QPalette::ToolTipText, QColor(15, 23, 42));
    QToolTip::setPalette(toolTipPalette);

    updateWindowControlAppearance(theme);
    updateWindowControlButtons();
    update();
}

void MainWindow::updateWindowControlButtons()
{
    if (minimizeButton_ == nullptr || maximizeButton_ == nullptr || closeButton_ == nullptr) {
        return;
    }

    Qtitan::RibbonStyle::Theme theme = Qtitan::RibbonStyle::Office2016White;
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        theme = ribbonStyle->getTheme();
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QColor iconColor = useDarkChrome ? QColor(241, 245, 249) : QColor(31, 41, 55);

    minimizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Minimize, iconColor));
    closeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Close, iconColor));

    if (isMaximized()) {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Restore, iconColor));
        maximizeButton_->setToolTip(tr("Restore Down"));
    } else {
        maximizeButton_->setIcon(createWindowControlIcon(WindowControlGlyph::Maximize, iconColor));
        maximizeButton_->setToolTip(tr("Maximize"));
    }

    minimizeButton_->setToolTip(tr("Minimize"));
    closeButton_->setToolTip(tr("Close"));
}

void MainWindow::updateWindowControlAppearance(Qtitan::RibbonStyle::Theme theme)
{
    if (windowControlsWidget_ == nullptr) {
        return;
    }

    const bool useDarkChrome = theme == Qtitan::RibbonStyle::Office2016DarkGray;
    const QString hoverColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.12)") : QStringLiteral("rgba(15, 23, 42, 0.08)");
    const QString pressedColor = useDarkChrome ? QStringLiteral("rgba(255, 255, 255, 0.18)") : QStringLiteral("rgba(15, 23, 42, 0.14)");
    windowControlsWidget_->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "border: none;"
        "background: transparent;"
        "padding: 0;"
        "}"
        "QToolButton:hover {"
        "background: %1;"
        "}"
        "QToolButton:pressed {"
        "background: %2;"
        "}"
        "QToolButton#windowCloseButton:hover {"
        "background: #e11d48;"
        "}"
        "QToolButton#windowCloseButton:pressed {"
        "background: #be123c;"
        "}").arg(hoverColor, pressedColor));
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

void MainWindow::updateDatasetPanel()
{
    const PointCloudData* pointCloudData = viewer_->pointCloudData();
    if (pointCloudData == nullptr && viewer_->currentFilePaths().isEmpty()) {
        datasetNameValueLabel_->setText(tr("No dataset loaded"));
        datasetPathValueLabel_->setText(tr("Open, add, or drag LAS/LAZ files into the window."));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    if (pointCloudData == nullptr) {
        datasetNameValueLabel_->setText(tr("All datasets hidden"));
        datasetPathValueLabel_->setText(datasetPathSummary(viewer_->currentFilePaths()));
        datasetPointsValueLabel_->setText(QStringLiteral("0"));
        datasetBoundsValueLabel_->setText(tr("N/A"));
        datasetExtentValueLabel_->setText(tr("N/A"));
        datasetColorValueLabel_->setText(colorModeName(viewer_->visualizationOptions().colorMode));
        return;
    }

    const QStringList filePaths = viewer_->currentFilePaths();
    const PointRecord& minBounds = pointCloudData->minBounds();
    const PointRecord& maxBounds = pointCloudData->maxBounds();

    datasetNameValueLabel_->setText(
        filePaths.size() == 1
            ? QFileInfo(filePaths.constFirst()).fileName()
            : tr("%1 datasets").arg(QLocale().toString(filePaths.size())));
    datasetPathValueLabel_->setText(datasetPathSummary(filePaths));
    datasetPointsValueLabel_->setText(QLocale().toString(static_cast<qlonglong>(pointCloudData->size())));
    datasetBoundsValueLabel_->setText(
        tr("Min (%1)\nMax (%2)")
            .arg(formatTriplet(minBounds.x, minBounds.y, minBounds.z))
            .arg(formatTriplet(maxBounds.x, maxBounds.y, maxBounds.z)));
    datasetExtentValueLabel_->setText(formatTriplet(
        maxBounds.x - minBounds.x,
        maxBounds.y - minBounds.y,
        maxBounds.z - minBounds.z));
    datasetColorValueLabel_->setText(
        tr("%1 | Native RGB: %2")
            .arg(colorModeName(viewer_->visualizationOptions().colorMode))
            .arg(pointCloudData->hasColor() ? tr("yes") : tr("no")));
}

void MainWindow::ensureRouteRoamFloatingDialog()
{
    if (routeRoamFloatingDialog_ != nullptr) {
        return;
    }

    routeRoamFloatingDialog_ = new QDialog(this, Qt::Tool);
    routeRoamFloatingDialog_->setModal(false);
    routeRoamFloatingDialog_->setAttribute(Qt::WA_DeleteOnClose, false);
    routeRoamFloatingDialog_->setWindowTitle(tr("Route Roam Controls"));
    routeRoamFloatingDialog_->setMinimumWidth(320);
    routeRoamFloatingDialog_->setStyleSheet(QStringLiteral(
        "QDialog {"
        "background-color: #f8fafc;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 10px;"
        "}"
        "QLabel {"
        "color: #334155;"
        "}"
        "QDoubleSpinBox, QComboBox {"
        "background-color: #ffffff;"
        "color: #0f172a;"
        "border: 1px solid #cbd5e1;"
        "border-radius: 6px;"
        "padding: 4px 8px;"
        "}"));

    auto* layout = new QFormLayout(routeRoamFloatingDialog_);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    routeRoamFloatingSpeedSpinBox_ = new QDoubleSpinBox(routeRoamFloatingDialog_);
    routeRoamFloatingSpeedSpinBox_->setRange(0.1, 80.0);
    routeRoamFloatingSpeedSpinBox_->setDecimals(1);
    routeRoamFloatingSpeedSpinBox_->setSingleStep(0.5);

    routeRoamFloatingViewModeComboBox_ = new QComboBox(routeRoamFloatingDialog_);
    routeRoamFloatingViewModeComboBox_->addItem(tr("Third Person"), static_cast<int>(RouteRoamViewMode::ThirdPerson));
    routeRoamFloatingViewModeComboBox_->addItem(tr("First Person"), static_cast<int>(RouteRoamViewMode::FirstPerson));

    routeRoamFloatingCaptureLabel_ = new QLabel(routeRoamFloatingDialog_);
    routeRoamFloatingCaptureLabel_->setWordWrap(true);
    routeRoamFloatingCaptureLabel_->setStyleSheet(QStringLiteral("color: #166534; font-weight: 600;"));
    routeRoamFloatingCaptureLabel_->setText(tr("Awaiting photo capture."));

    layout->addRow(tr("Roam Speed"), routeRoamFloatingSpeedSpinBox_);
    layout->addRow(tr("Roam View Mode"), routeRoamFloatingViewModeComboBox_);
    layout->addRow(tr("Capture"), routeRoamFloatingCaptureLabel_);

    connect(routeRoamFloatingSpeedSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double speed) {
        if (viewer_ != nullptr) {
            viewer_->setInspectionRouteRoamSpeedMetersPerSecond(speed);
            persistWindowSettings();
        }
    });
    connect(routeRoamFloatingViewModeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (viewer_ == nullptr || routeRoamFloatingViewModeComboBox_ == nullptr) {
            return;
        }
        viewer_->setInspectionRouteRoamViewMode(static_cast<RouteRoamViewMode>(routeRoamFloatingViewModeComboBox_->currentData().toInt()));
        persistWindowSettings();
    });
}

void MainWindow::syncRouteRoamFloatingDialog()
{
    if (viewer_ == nullptr) {
        if (routeRoamFloatingDialog_ != nullptr) {
            routeRoamFloatingDialog_->hide();
        }
        return;
    }

    const bool roamActive = viewer_->inspectionRouteRoamActive();
    if (!roamActive) {
        if (routeRoamFloatingDialog_ != nullptr) {
            routeRoamFloatingDialog_->hide();
        }
        return;
    }

    ensureRouteRoamFloatingDialog();
    if (routeRoamFloatingDialog_ == nullptr) {
        return;
    }

    if (routeRoamFloatingSpeedSpinBox_ != nullptr) {
        const QSignalBlocker blocker(routeRoamFloatingSpeedSpinBox_);
        routeRoamFloatingSpeedSpinBox_->setValue(viewer_->inspectionRouteRoamSpeedMetersPerSecond());
    }
    if (routeRoamFloatingViewModeComboBox_ != nullptr) {
        const QSignalBlocker blocker(routeRoamFloatingViewModeComboBox_);
        const int modeIndex = routeRoamFloatingViewModeComboBox_->findData(static_cast<int>(viewer_->inspectionRouteRoamViewMode()));
        routeRoamFloatingViewModeComboBox_->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
    }
    if (routeRoamFloatingCaptureLabel_ != nullptr) {
        routeRoamFloatingCaptureLabel_->setText(
            routeRoamLastCaptureSummary_.trimmed().isEmpty()
                ? tr("Awaiting photo capture.")
                : routeRoamLastCaptureSummary_);
    }

    if (!routeRoamFloatingDialog_->isVisible()) {
        QPoint anchor = mapToGlobal(QPoint(width() - routeRoamFloatingDialog_->width() - 24, 120));
        if (viewer_ != nullptr) {
            const QPoint viewerGlobalTopLeft = viewer_->mapToGlobal(QPoint(0, 0));
            const QRect viewerGlobalRect(viewerGlobalTopLeft, viewer_->size());
            const int anchorX = viewerGlobalRect.right() - routeRoamFloatingDialog_->width() - 16;
            const int anchorY = viewerGlobalRect.top()
                + std::max(
                    16,
                    (viewerGlobalRect.height() - routeRoamFloatingDialog_->height()) / 2);
            anchor = QPoint(anchorX, anchorY);
        }
        routeRoamFloatingDialog_->move(anchor);
    }
    routeRoamFloatingDialog_->show();
    routeRoamFloatingDialog_->raise();
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

void MainWindow::updateActionState()
{
    const bool hasPointCloud = viewer_->hasPointCloud();
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
    clearAction_->setEnabled(viewer_ != nullptr && !viewer_->currentFilePaths().isEmpty());
    fitSceneAction_->setEnabled(hasPointCloud);
    topViewAction_->setEnabled(hasPointCloud);
    frontViewAction_->setEnabled(hasPointCloud);
    rightViewAction_->setEnabled(hasPointCloud);
    measureAction_->setEnabled(hasPointCloud);
    profileClassificationAction_->setEnabled(profileClassificationReady);
    showProfileClassificationDockAction_->setEnabled(true);
    showProfileClassificationDockAction_->setChecked(profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible());
    saveProfileClassificationEditsAction_->setEnabled(hasPointCloud && viewer_->classificationEditedPointCount() > 0);
    clearMeasurementAction_->setEnabled(hasPointCloud && viewer_->measurementResult().hasStartPoint);
    analyzeVegetationRisksAction_->setEnabled(hasPointCloud && hasMeasuredCorridor);
    exportClearanceCsvAction_->setEnabled(hasPointCloud && viewer_->measurementResult().isComplete());
    showProfileDockAction_->setEnabled(true);
    showProfileDockAction_->setChecked(profileDock_ != nullptr && profileDock_->isVisible());
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

void MainWindow::loadInteractionSettings()
{
    QSettings settings;
    InteractionOptions options;
    options.invertOrbitDrag = settings.value(settingskeys::kInteractionInvertOrbitDrag, false).toBool();
    options.invertPanDrag = settings.value(settingskeys::kInteractionInvertPanDrag, false).toBool();
    options.invertWheelZoom = settings.value(settingskeys::kInteractionInvertWheelZoom, false).toBool();
    options.wheelZoomSensitivityPercent =
        settings.value(settingskeys::kInteractionWheelZoomSensitivityPercent, 100).toInt();
    viewer_->setInteractionOptions(options);
}

void MainWindow::persistInteractionSettings() const
{
    QSettings settings;
    const InteractionOptions& options = viewer_->interactionOptions();
    settings.setValue(settingskeys::kInteractionInvertOrbitDrag, options.invertOrbitDrag);
    settings.setValue(settingskeys::kInteractionInvertPanDrag, options.invertPanDrag);
    settings.setValue(settingskeys::kInteractionInvertWheelZoom, options.invertWheelZoom);
    settings.setValue(settingskeys::kInteractionWheelZoomSensitivityPercent, options.wheelZoomSensitivityPercent);
}

void MainWindow::loadMeasurementSettings()
{
    QSettings settings;
    clearanceWarningThresholdMeters_ = settings.value(
        settingskeys::kMeasurementClearanceThresholdMeters,
        clearanceWarningThresholdMeters_).toDouble();
    clearanceRulePreset_ = static_cast<ClearanceRulePreset>(settings.value(
        settingskeys::kMeasurementClearanceRulePreset,
        static_cast<int>(clearanceRulePreset_)).toInt());
    vegetationSearchRadiusMeters_ = settings.value(
        settingskeys::kMeasurementVegetationSearchRadiusMeters,
        vegetationSearchRadiusMeters_).toDouble();
    vegetationClusterGapMeters_ = settings.value(
        settingskeys::kMeasurementVegetationClusterGapMeters,
        vegetationClusterGapMeters_).toDouble();
    vegetationClusterPointCount_ = settings.value(
        settingskeys::kMeasurementVegetationClusterPointCount,
        vegetationClusterPointCount_).toInt();
    preferVegetationClassification_ = settings.value(
        settingskeys::kMeasurementPreferVegetationClassification,
        preferVegetationClassification_).toBool();
    const ProfileClassificationSelectionMode profileClassificationSelectionMode =
        static_cast<ProfileClassificationSelectionMode>(settings.value(
            settingskeys::kMeasurementProfileClassificationSelectionMode,
            static_cast<int>(ProfileClassificationSelectionMode::Rectangle)).toInt());

    if (clearanceThresholdSpinBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceThresholdSpinBox_);
        clearanceThresholdSpinBox_->setValue(clearanceWarningThresholdMeters_);
    }
    if (clearanceRulePresetComboBox_ != nullptr) {
        const QSignalBlocker blocker(clearanceRulePresetComboBox_);
        const int presetIndex = clearanceRulePresetComboBox_->findData(static_cast<int>(clearanceRulePreset_));
        clearanceRulePresetComboBox_->setCurrentIndex(presetIndex >= 0 ? presetIndex : 0);
    }
    if (vegetationSearchRadiusSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationSearchRadiusSpinBox_);
        vegetationSearchRadiusSpinBox_->setValue(vegetationSearchRadiusMeters_);
    }
    if (vegetationClusterGapSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterGapSpinBox_);
        vegetationClusterGapSpinBox_->setValue(vegetationClusterGapMeters_);
    }
    if (vegetationClusterPointCountSpinBox_ != nullptr) {
        const QSignalBlocker blocker(vegetationClusterPointCountSpinBox_);
        vegetationClusterPointCountSpinBox_->setValue(vegetationClusterPointCount_);
    }
    if (preferVegetationClassificationCheckBox_ != nullptr) {
        const QSignalBlocker blocker(preferVegetationClassificationCheckBox_);
        preferVegetationClassificationCheckBox_->setChecked(preferVegetationClassification_);
    }
    if (viewer_ != nullptr) {
        viewer_->setProfileClassificationSelectionMode(profileClassificationSelectionMode);
    }
    updateProfileClassificationPanel();
}

void MainWindow::persistMeasurementSettings() const
{
    QSettings settings;
    settings.setValue(settingskeys::kMeasurementClearanceThresholdMeters, clearanceWarningThresholdMeters_);
    settings.setValue(settingskeys::kMeasurementClearanceRulePreset, static_cast<int>(clearanceRulePreset_));
    settings.setValue(settingskeys::kMeasurementVegetationSearchRadiusMeters, vegetationSearchRadiusMeters_);
    settings.setValue(settingskeys::kMeasurementVegetationClusterGapMeters, vegetationClusterGapMeters_);
    settings.setValue(settingskeys::kMeasurementVegetationClusterPointCount, vegetationClusterPointCount_);
    settings.setValue(settingskeys::kMeasurementPreferVegetationClassification, preferVegetationClassification_);
    settings.setValue(
        settingskeys::kMeasurementProfileClassificationSelectionMode,
        viewer_ != nullptr
            ? static_cast<int>(viewer_->profileClassificationSelectionMode())
            : static_cast<int>(ProfileClassificationSelectionMode::Rectangle));
}

void MainWindow::loadVisualizationSettings()
{
    if (viewer_ == nullptr) {
        return;
    }

    QSettings settings;
    const PointCloudVisualizationOptions defaults = viewer_->visualizationOptions();
    viewer_->setPointSize(settings.value(settingskeys::kVisualizationPointSize, defaults.pointSize).toInt());
    viewer_->setPointOpacity(settings.value(settingskeys::kVisualizationPointOpacity, defaults.pointOpacity * 100.0f).toInt());
    viewer_->setDepthCueStrength(settings.value(settingskeys::kVisualizationDepthCueStrength, defaults.depthCueStrength * 100.0f).toInt());
    viewer_->setEdlStrength(settings.value(settingskeys::kVisualizationEdlStrength, defaults.edlStrength * 100.0f).toInt());
    viewer_->setColorMode(settings.value(settingskeys::kVisualizationColorMode, static_cast<int>(defaults.colorMode)).toInt());
    viewer_->setSingleColor(settings.value(settingskeys::kVisualizationSingleColor, defaults.singleColor).value<QColor>());
    const QJsonDocument classificationColorDocument = QJsonDocument::fromJson(
        settings.value(settingskeys::kVisualizationClassificationColorsJson).toByteArray());
    if (classificationColorDocument.isObject()) {
        viewer_->setClassificationColorMap(
            classificationColorMapFromJson(classificationColorDocument.object(), defaults.classificationColors));
    }
    const QJsonDocument classificationVisibilityDocument = QJsonDocument::fromJson(
        settings.value(settingskeys::kVisualizationClassificationVisibilityJson).toByteArray());
    if (classificationVisibilityDocument.isObject()) {
        viewer_->setClassificationVisibilityMap(
            classificationVisibilityMapFromJson(classificationVisibilityDocument.object(), defaults.classificationVisibility));
    }
    const QJsonDocument classificationNameDocument = QJsonDocument::fromJson(
        settings.value(settingskeys::kVisualizationClassificationNameOverridesJson).toByteArray());
    classificationNameOverrides_ = classificationNameDocument.isObject()
        ? classificationNameMapFromJson(classificationNameDocument.object())
        : QMap<int, QString>();
    viewer_->setClassificationFallbackColor(
        settings.value(settingskeys::kVisualizationClassificationFallbackColor, defaults.classificationFallbackColor).value<QColor>());
    viewer_->setBackgroundColor(settings.value(settingskeys::kVisualizationBackgroundColor, defaults.backgroundColor).value<QColor>());
    viewer_->setInspectionRouteWaypointColor(
        settings.value(settingskeys::kVisualizationRouteWaypointColor, viewer_->inspectionRouteWaypointColor()).value<QColor>());
    viewer_->setInspectionRoutePartPointColor(
        settings.value(settingskeys::kVisualizationRoutePartPointColor, viewer_->inspectionRoutePartPointColor()).value<QColor>());
    viewer_->setInspectionRouteTrajectoryColor(
        settings.value(settingskeys::kVisualizationRouteTrajectoryColor, viewer_->inspectionRouteTrajectoryColor()).value<QColor>());
    viewer_->setUseRoundSplats(settings.value(settingskeys::kVisualizationUseRoundSplats, defaults.useRoundSplats).toBool());
    viewer_->setShowAxes(settings.value(settingskeys::kVisualizationShowAxes, defaults.showAxes).toBool());
    viewer_->setShowBoundingBox(settings.value(settingskeys::kVisualizationShowBoundingBox, defaults.showBoundingBox).toBool());
}

void MainWindow::persistVisualizationSettings() const
{
    if (viewer_ == nullptr) {
        return;
    }

    QSettings settings;
    const PointCloudVisualizationOptions& options = viewer_->visualizationOptions();
    settings.setValue(settingskeys::kVisualizationPointSize, options.pointSize);
    settings.setValue(settingskeys::kVisualizationPointOpacity, options.pointOpacity * 100.0f);
    settings.setValue(settingskeys::kVisualizationDepthCueStrength, options.depthCueStrength * 100.0f);
    settings.setValue(settingskeys::kVisualizationEdlStrength, options.edlStrength * 100.0f);
    settings.setValue(settingskeys::kVisualizationColorMode, static_cast<int>(options.colorMode));
    settings.setValue(settingskeys::kVisualizationSingleColor, options.singleColor);
    settings.setValue(
        settingskeys::kVisualizationClassificationColorsJson,
        QJsonDocument(classificationColorMapToJson(options.classificationColors)).toJson(QJsonDocument::Compact));
    settings.setValue(
        settingskeys::kVisualizationClassificationVisibilityJson,
        QJsonDocument(classificationVisibilityMapToJson(options.classificationVisibility)).toJson(QJsonDocument::Compact));
    settings.setValue(
        settingskeys::kVisualizationClassificationNameOverridesJson,
        QJsonDocument(classificationNameMapToJson(classificationNameOverrides_)).toJson(QJsonDocument::Compact));
    settings.setValue(settingskeys::kVisualizationClassificationFallbackColor, options.classificationFallbackColor);
    settings.setValue(settingskeys::kVisualizationBackgroundColor, options.backgroundColor);
    settings.setValue(settingskeys::kVisualizationRouteWaypointColor, viewer_->inspectionRouteWaypointColor());
    settings.setValue(settingskeys::kVisualizationRoutePartPointColor, viewer_->inspectionRoutePartPointColor());
    settings.setValue(settingskeys::kVisualizationRouteTrajectoryColor, viewer_->inspectionRouteTrajectoryColor());
    settings.setValue(settingskeys::kVisualizationUseRoundSplats, options.useRoundSplats);
    settings.setValue(settingskeys::kVisualizationShowAxes, options.showAxes);
    settings.setValue(settingskeys::kVisualizationShowBoundingBox, options.showBoundingBox);
}

void MainWindow::loadLanguageSettings()
{
    QSettings settings;
    const QString storedLanguage = settings.value(settingskeys::kUiLanguage).toString();
    if (storedLanguage == QStringLiteral("zh_CN")) {
        currentLanguage_ = UiLanguage::Chinese;
    } else if (storedLanguage == QStringLiteral("en")) {
        currentLanguage_ = UiLanguage::English;
    } else {
        currentLanguage_ = defaultLanguageFromLocale();
    }
}

void MainWindow::persistLanguageSettings() const
{
    QSettings settings;
    settings.setValue(settingskeys::kUiLanguage, languageCodeFor(currentLanguage_));
}

void MainWindow::loadWindowSettings()
{
    loadingWindowSettings_ = true;
    QSettings settings;
    const auto& uiHistoryStore = lasviewer::gui::UiHistoryStore::instance();
    const QByteArray geometry = settings.value(settingskeys::kWindowGeometry).toByteArray();
    const QByteArray state = settings.value(settingskeys::kWindowState).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const bool restoredState = !state.isEmpty() && restoreState(state, kMainWindowStateVersion);
    if (settings.value(settingskeys::kWindowMaximized, false).toBool()) {
        showMaximized();
    }

    if (inspectorTabWidget_ != nullptr) {
        inspectorTabWidget_->setCurrentIndex(settings.value(settingskeys::kWindowInspectorTab, 0).toInt());
    }
    if (routeDetailsTabWidget_ != nullptr) {
        routeDetailsTabWidget_->setCurrentIndex(settings.value(settingskeys::kWindowRouteDetailsTab, 0).toInt());
    }

    const QList<DjiAircraftProfile> supportedProfiles = supportedDjiAircraftProfiles();
    const int aircraftProfileDefault = settings.value(
        settingskeys::kRoutePlanningAircraftProfile,
        static_cast<int>(routePlanningOptions_.aircraftProfile)).toInt();
    DjiAircraftProfile savedAircraftProfile = static_cast<DjiAircraftProfile>(uiHistoryStore.loadInt(
        QString::fromLatin1(kRouteHistoryIdPlanningAircraftProfile),
        aircraftProfileDefault));
    if (!supportedProfiles.contains(savedAircraftProfile)) {
        savedAircraftProfile = routePlanningOptions_.aircraftProfile;
    }
    routePlanningOptions_.aircraftProfile = savedAircraftProfile;
    routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(uiHistoryStore.loadDouble(
        QString::fromLatin1(kRouteHistoryIdPlanningSafetyHeightMeters),
        settings.value(
            settingskeys::kRoutePlanningSafetyHeightMeters,
            routePlanningOptions_.safety.safetyHeightMeters).toDouble()));
    routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(uiHistoryStore.loadDouble(
        QString::fromLatin1(kRouteHistoryIdPlanningWaypointSpeedMps),
        settings.value(
            settingskeys::kRoutePlanningWaypointSpeedMps,
            routePlanningOptions_.safety.defaultWaypointSpeedMps).toDouble()));
    routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(uiHistoryStore.loadDouble(
        QString::fromLatin1(kRouteHistoryIdPlanningWaypointSpacingMeters),
        settings.value(
            settingskeys::kRoutePlanningWaypointSpacingMeters,
            routePlanningOptions_.generation.waypointSpacingMeters).toDouble()));
    routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(uiHistoryStore.loadDouble(
        QString::fromLatin1(kRouteHistoryIdPlanningSmoothingStrengthPercent),
        settings.value(
            settingskeys::kRoutePlanningSmoothingStrengthPercent,
            routePlanningOptions_.generation.smoothingStrengthPercent).toDouble()));
    routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(uiHistoryStore.loadDouble(
        QString::fromLatin1(kRouteHistoryIdPlanningHeightOffsetMeters),
        settings.value(
            settingskeys::kRoutePlanningHeightOffsetMeters,
            routePlanningOptions_.safety.heightOffsetMeters).toDouble()));
    if (aircraftProfileComboBox_ != nullptr) {
        const QSignalBlocker blocker(aircraftProfileComboBox_);
        const int profileIndex = aircraftProfileComboBox_->findData(static_cast<int>(savedAircraftProfile));
        aircraftProfileComboBox_->setCurrentIndex(profileIndex >= 0 ? profileIndex : 0);
    }
    if (routeSafetyHeightSpinBox_ != nullptr) {
        const QSignalBlocker blocker(routeSafetyHeightSpinBox_);
        routeSafetyHeightSpinBox_->setValue(routePlanningOptions_.safety.safetyHeightMeters);
        routePlanningOptions_.safety.safetyHeightMeters = static_cast<float>(routeSafetyHeightSpinBox_->value());
    }
    if (routeWaypointSpeedSpinBox_ != nullptr) {
        const QSignalBlocker blocker(routeWaypointSpeedSpinBox_);
        routeWaypointSpeedSpinBox_->setValue(routePlanningOptions_.safety.defaultWaypointSpeedMps);
        routePlanningOptions_.safety.defaultWaypointSpeedMps = static_cast<float>(routeWaypointSpeedSpinBox_->value());
    }
    if (routeWaypointSpacingSpinBox_ != nullptr) {
        const QSignalBlocker blocker(routeWaypointSpacingSpinBox_);
        routeWaypointSpacingSpinBox_->setValue(routePlanningOptions_.generation.waypointSpacingMeters);
        routePlanningOptions_.generation.waypointSpacingMeters = static_cast<float>(routeWaypointSpacingSpinBox_->value());
    }
    if (routeSmoothingStrengthSpinBox_ != nullptr) {
        const QSignalBlocker blocker(routeSmoothingStrengthSpinBox_);
        routeSmoothingStrengthSpinBox_->setValue(routePlanningOptions_.generation.smoothingStrengthPercent);
        routePlanningOptions_.generation.smoothingStrengthPercent = static_cast<float>(routeSmoothingStrengthSpinBox_->value());
    }
    if (routeHeightOffsetSpinBox_ != nullptr) {
        const QSignalBlocker blocker(routeHeightOffsetSpinBox_);
        routeHeightOffsetSpinBox_->setValue(routePlanningOptions_.safety.heightOffsetMeters);
        routePlanningOptions_.safety.heightOffsetMeters = static_cast<float>(routeHeightOffsetSpinBox_->value());
    }

    if (viewer_ != nullptr) {
        const double roamSpeed = uiHistoryStore.loadDouble(
            QString::fromLatin1(kRouteHistoryIdRoamSpeedMps),
            settings.value(
                settingskeys::kRouteRoamSpeedMps,
                viewer_->inspectionRouteRoamSpeedMetersPerSecond()).toDouble());
        const int savedRoamViewModeValue = uiHistoryStore.loadInt(
            QString::fromLatin1(kRouteHistoryIdRoamViewMode),
            settings.value(
                settingskeys::kRouteRoamViewMode,
                static_cast<int>(viewer_->inspectionRouteRoamViewMode())).toInt());
        const RouteRoamViewMode savedRoamViewMode =
            savedRoamViewModeValue == static_cast<int>(RouteRoamViewMode::FirstPerson)
                ? RouteRoamViewMode::FirstPerson
                : RouteRoamViewMode::ThirdPerson;
        viewer_->setInspectionRouteRoamSpeedMetersPerSecond(roamSpeed);
        viewer_->setInspectionRouteRoamViewMode(savedRoamViewMode);
        if (routeRoamSpeedSpinBox_ != nullptr) {
            const QSignalBlocker blocker(routeRoamSpeedSpinBox_);
            routeRoamSpeedSpinBox_->setValue(viewer_->inspectionRouteRoamSpeedMetersPerSecond());
        }
        if (routeRoamViewModeComboBox_ != nullptr) {
            const QSignalBlocker blocker(routeRoamViewModeComboBox_);
            const int roamModeIndex = routeRoamViewModeComboBox_->findData(static_cast<int>(savedRoamViewMode));
            routeRoamViewModeComboBox_->setCurrentIndex(roamModeIndex >= 0 ? roamModeIndex : 0);
        }
    }
    setRouteEditingEnabled(
        uiHistoryStore.loadBool(
            QString::fromLatin1(kRouteHistoryIdEditingEnabled),
            settings.value(settingskeys::kRouteEditingEnabled, routeEditingEnabled_).toBool()),
        false);

    if (routeWaypointLabelModeComboBox_ != nullptr) {
        const int savedWaypointLabelMode = uiHistoryStore.loadInt(
            QString::fromLatin1(kRouteHistoryIdDisplayWaypointLabelMode),
            settings.value(
                settingskeys::kRouteDisplayWaypointLabelMode,
                settings.value(
                    settingskeys::kWindowRouteWaypointLabelMode,
                    static_cast<int>(RouteLabelDisplayMode::Name))).toInt());
        const int waypointLabelModeIndex = routeWaypointLabelModeComboBox_->findData(savedWaypointLabelMode);
        const QSignalBlocker blocker(routeWaypointLabelModeComboBox_);
        routeWaypointLabelModeComboBox_->setCurrentIndex(waypointLabelModeIndex >= 0 ? waypointLabelModeIndex : 0);
    }
    if (routePartLabelModeComboBox_ != nullptr) {
        const int savedPartLabelMode = uiHistoryStore.loadInt(
            QString::fromLatin1(kRouteHistoryIdDisplayPartLabelMode),
            settings.value(
                settingskeys::kRouteDisplayPartLabelMode,
                settings.value(
                    settingskeys::kWindowRoutePartLabelMode,
                    static_cast<int>(RouteLabelDisplayMode::Name))).toInt());
        const int partLabelModeIndex = routePartLabelModeComboBox_->findData(savedPartLabelMode);
        const QSignalBlocker blocker(routePartLabelModeComboBox_);
        routePartLabelModeComboBox_->setCurrentIndex(partLabelModeIndex >= 0 ? partLabelModeIndex : 0);
    }
    if (routeWaypointShowCoordinatesCheckBox_ != nullptr) {
        const QSignalBlocker blocker(routeWaypointShowCoordinatesCheckBox_);
        routeWaypointShowCoordinatesCheckBox_->setChecked(
            uiHistoryStore.loadBool(
                QString::fromLatin1(kRouteHistoryIdDisplayWaypointShowCoordinates),
                settings.value(
                    settingskeys::kRouteDisplayWaypointShowCoordinates,
                    settings.value(settingskeys::kWindowRouteWaypointShowCoordinates, true)).toBool()));
    }
    if (routeWaypointShowCaptureAnglesCheckBox_ != nullptr) {
        const QSignalBlocker blocker(routeWaypointShowCaptureAnglesCheckBox_);
        routeWaypointShowCaptureAnglesCheckBox_->setChecked(
            uiHistoryStore.loadBool(
                QString::fromLatin1(kRouteHistoryIdDisplayWaypointShowCaptureAngles),
                settings.value(
                    settingskeys::kRouteDisplayWaypointShowCaptureAngles,
                    settings.value(settingskeys::kWindowRouteWaypointShowCaptureAngles, true)).toBool()));
    }
    if (routePartShowCoordinatesCheckBox_ != nullptr) {
        const QSignalBlocker blocker(routePartShowCoordinatesCheckBox_);
        routePartShowCoordinatesCheckBox_->setChecked(
            uiHistoryStore.loadBool(
                QString::fromLatin1(kRouteHistoryIdDisplayPartShowCoordinates),
                settings.value(
                    settingskeys::kRouteDisplayPartShowCoordinates,
                    settings.value(settingskeys::kWindowRoutePartShowCoordinates, true)).toBool()));
    }
    if (routePartShowCaptureAnglesCheckBox_ != nullptr) {
        const QSignalBlocker blocker(routePartShowCaptureAnglesCheckBox_);
        routePartShowCaptureAnglesCheckBox_->setChecked(
            uiHistoryStore.loadBool(
                QString::fromLatin1(kRouteHistoryIdDisplayPartShowCaptureAngles),
                settings.value(
                    settingskeys::kRouteDisplayPartShowCaptureAngles,
                    settings.value(settingskeys::kWindowRoutePartShowCaptureAngles, true)).toBool()));
    }
    if (viewer_ != nullptr) {
        if (routeWaypointLabelModeComboBox_ != nullptr) {
            viewer_->setInspectionRouteWaypointLabelDisplayMode(static_cast<RouteLabelDisplayMode>(
                routeWaypointLabelModeComboBox_->currentData().toInt()));
        }
        if (routePartLabelModeComboBox_ != nullptr) {
            viewer_->setInspectionRoutePartLabelDisplayMode(static_cast<RouteLabelDisplayMode>(
                routePartLabelModeComboBox_->currentData().toInt()));
        }
    }
    applyRouteWaypointTableColumnVisibility();
    applyRoutePartTableColumnVisibility();

    if (logDock_ != nullptr) {
        const int savedFilterLevel = settings.value(settingskeys::kWindowLogFilterLevel, -1).toInt();
        logDock_->setSelectedFilterLevel(savedFilterLevel);
        logDock_->setSearchKeyword(settings.value(settingskeys::kWindowLogSearchKeyword).toString());
        logDock_->setAutoScrollEnabled(settings.value(settingskeys::kWindowLogAutoScroll, true).toBool());
    }

    if (!restoredState) {
        const bool showLog = settings.value(settingskeys::kWindowShowLog, false).toBool();
        const bool showProfileClassification = settings.value(settingskeys::kWindowShowProfileClassification, false).toBool();
        const bool showRouteDetails = settings.value(settingskeys::kWindowShowRouteDetails, false).toBool();
        if (logDock_ != nullptr) {
            logDock_->setVisible(showLog);
        }
        if (profileClassificationDock_ != nullptr) {
            profileClassificationDock_->setVisible(showProfileClassification);
        }
        if (routeDetailsDock_ != nullptr) {
            routeDetailsDock_->setVisible(showRouteDetails);
        }
    }

    syncProfileDockForMeasurementMode(viewer_ != nullptr && viewer_->measurementEnabled());

    if (showLogAction_ != nullptr) {
        const QSignalBlocker blocker(showLogAction_);
        showLogAction_->setChecked(logDock_ != nullptr && logDock_->isVisible());
    }
    if (showProfileDockAction_ != nullptr) {
        const QSignalBlocker blocker(showProfileDockAction_);
        showProfileDockAction_->setChecked(profileDock_ != nullptr && profileDock_->isVisible());
    }
    if (showProfileClassificationDockAction_ != nullptr) {
        const QSignalBlocker blocker(showProfileClassificationDockAction_);
        showProfileClassificationDockAction_->setChecked(profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible());
    }

    refreshLogPanel();

    if (projectDock_ != nullptr) {
        projectDock_->show();
        projectDock_->raise();
    }
    loadingWindowSettings_ = false;
}

void MainWindow::persistWindowSettings() const
{
    if (loadingWindowSettings_) {
        return;
    }

    QSettings settings;
    const auto& uiHistoryStore = lasviewer::gui::UiHistoryStore::instance();
    settings.setValue(settingskeys::kWindowGeometry, saveGeometry());
    settings.setValue(settingskeys::kWindowState, saveState(kMainWindowStateVersion));
    settings.setValue(settingskeys::kWindowMaximized, isMaximized());
    settings.setValue(settingskeys::kWindowShowLog, logDock_ != nullptr && logDock_->isVisible());
    settings.setValue(settingskeys::kWindowShowProfile, profileDock_ != nullptr && profileDock_->isVisible());
    settings.setValue(
        settingskeys::kWindowShowProfileClassification,
        profileClassificationDock_ != nullptr && profileClassificationDock_->isVisible());
    settings.setValue(
        settingskeys::kWindowShowRouteDetails,
        routeDetailsDock_ != nullptr && routeDetailsDock_->isVisible());
    settings.setValue(
        settingskeys::kWindowInspectorTab,
        inspectorTabWidget_ != nullptr ? inspectorTabWidget_->currentIndex() : 0);
    settings.setValue(
        settingskeys::kWindowRouteDetailsTab,
        routeDetailsTabWidget_ != nullptr ? routeDetailsTabWidget_->currentIndex() : 0);
    settings.setValue(
        settingskeys::kWindowRouteWaypointLabelMode,
        routeWaypointLabelModeComboBox_ != nullptr
            ? routeWaypointLabelModeComboBox_->currentData().toInt()
            : static_cast<int>(RouteLabelDisplayMode::Name));
    settings.setValue(
        settingskeys::kWindowRoutePartLabelMode,
        routePartLabelModeComboBox_ != nullptr
            ? routePartLabelModeComboBox_->currentData().toInt()
            : static_cast<int>(RouteLabelDisplayMode::Name));
    settings.setValue(
        settingskeys::kWindowRouteWaypointShowCoordinates,
        routeWaypointShowCoordinatesCheckBox_ == nullptr || routeWaypointShowCoordinatesCheckBox_->isChecked());
    settings.setValue(
        settingskeys::kWindowRouteWaypointShowCaptureAngles,
        routeWaypointShowCaptureAnglesCheckBox_ == nullptr || routeWaypointShowCaptureAnglesCheckBox_->isChecked());
    settings.setValue(
        settingskeys::kWindowRoutePartShowCoordinates,
        routePartShowCoordinatesCheckBox_ == nullptr || routePartShowCoordinatesCheckBox_->isChecked());
    settings.setValue(
        settingskeys::kWindowRoutePartShowCaptureAngles,
        routePartShowCaptureAnglesCheckBox_ == nullptr || routePartShowCaptureAnglesCheckBox_->isChecked());
    const int waypointLabelMode = routeWaypointLabelModeComboBox_ != nullptr
        ? routeWaypointLabelModeComboBox_->currentData().toInt()
        : static_cast<int>(RouteLabelDisplayMode::Name);
    const int partLabelMode = routePartLabelModeComboBox_ != nullptr
        ? routePartLabelModeComboBox_->currentData().toInt()
        : static_cast<int>(RouteLabelDisplayMode::Name);
    const bool waypointShowCoordinates =
        routeWaypointShowCoordinatesCheckBox_ == nullptr || routeWaypointShowCoordinatesCheckBox_->isChecked();
    const bool waypointShowCaptureAngles =
        routeWaypointShowCaptureAnglesCheckBox_ == nullptr || routeWaypointShowCaptureAnglesCheckBox_->isChecked();
    const bool partShowCoordinates =
        routePartShowCoordinatesCheckBox_ == nullptr || routePartShowCoordinatesCheckBox_->isChecked();
    const bool partShowCaptureAngles =
        routePartShowCaptureAnglesCheckBox_ == nullptr || routePartShowCaptureAnglesCheckBox_->isChecked();
    const double roamSpeed = viewer_ != nullptr
        ? viewer_->inspectionRouteRoamSpeedMetersPerSecond()
        : (routeRoamSpeedSpinBox_ != nullptr ? routeRoamSpeedSpinBox_->value() : 2.0);
    const int roamViewMode = viewer_ != nullptr
        ? static_cast<int>(viewer_->inspectionRouteRoamViewMode())
        : (routeRoamViewModeComboBox_ != nullptr
            ? routeRoamViewModeComboBox_->currentData().toInt()
            : static_cast<int>(RouteRoamViewMode::ThirdPerson));

    settings.setValue(settingskeys::kRouteDisplayWaypointLabelMode, waypointLabelMode);
    settings.setValue(settingskeys::kRouteDisplayPartLabelMode, partLabelMode);
    settings.setValue(settingskeys::kRouteDisplayWaypointShowCoordinates, waypointShowCoordinates);
    settings.setValue(settingskeys::kRouteDisplayWaypointShowCaptureAngles, waypointShowCaptureAngles);
    settings.setValue(settingskeys::kRouteDisplayPartShowCoordinates, partShowCoordinates);
    settings.setValue(settingskeys::kRouteDisplayPartShowCaptureAngles, partShowCaptureAngles);
    settings.setValue(settingskeys::kRouteEditingEnabled, routeEditingEnabled_);
    settings.setValue(settingskeys::kRoutePlanningAircraftProfile, static_cast<int>(routePlanningOptions_.aircraftProfile));
    settings.setValue(settingskeys::kRoutePlanningSafetyHeightMeters, routePlanningOptions_.safety.safetyHeightMeters);
    settings.setValue(settingskeys::kRoutePlanningWaypointSpeedMps, routePlanningOptions_.safety.defaultWaypointSpeedMps);
    settings.setValue(settingskeys::kRoutePlanningWaypointSpacingMeters, routePlanningOptions_.generation.waypointSpacingMeters);
    settings.setValue(settingskeys::kRoutePlanningSmoothingStrengthPercent, routePlanningOptions_.generation.smoothingStrengthPercent);
    settings.setValue(settingskeys::kRoutePlanningHeightOffsetMeters, routePlanningOptions_.safety.heightOffsetMeters);
    settings.setValue(settingskeys::kRouteRoamSpeedMps, roamSpeed);
    settings.setValue(settingskeys::kRouteRoamViewMode, roamViewMode);

    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdDisplayWaypointLabelMode), waypointLabelMode);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdDisplayPartLabelMode), partLabelMode);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdDisplayWaypointShowCoordinates), waypointShowCoordinates);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdDisplayWaypointShowCaptureAngles), waypointShowCaptureAngles);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdDisplayPartShowCoordinates), partShowCoordinates);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdDisplayPartShowCaptureAngles), partShowCaptureAngles);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdEditingEnabled), routeEditingEnabled_);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdPlanningAircraftProfile), static_cast<int>(routePlanningOptions_.aircraftProfile));
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdPlanningSafetyHeightMeters), routePlanningOptions_.safety.safetyHeightMeters);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdPlanningWaypointSpeedMps), routePlanningOptions_.safety.defaultWaypointSpeedMps);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdPlanningWaypointSpacingMeters), routePlanningOptions_.generation.waypointSpacingMeters);
    uiHistoryStore.save(
        QString::fromLatin1(kRouteHistoryIdPlanningSmoothingStrengthPercent),
        routePlanningOptions_.generation.smoothingStrengthPercent);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdPlanningHeightOffsetMeters), routePlanningOptions_.safety.heightOffsetMeters);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdRoamSpeedMps), roamSpeed);
    uiHistoryStore.save(QString::fromLatin1(kRouteHistoryIdRoamViewMode), roamViewMode);
    settings.setValue(
        settingskeys::kWindowLogFilterLevel,
        logDock_ != nullptr ? logDock_->selectedFilterLevel() : -1);
    settings.setValue(
        settingskeys::kWindowLogSearchKeyword,
        logDock_ != nullptr ? logDock_->searchKeyword() : QString());
    settings.setValue(
        settingskeys::kWindowLogAutoScroll,
        logDock_ == nullptr || logDock_->autoScrollEnabled());
}

void MainWindow::loadThemeSettings()
{
    const int storedTheme = QSettings().value(
        settingskeys::kUiTheme,
        static_cast<int>(Qtitan::RibbonStyle::Office2016Colorful)).toInt();

    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        const auto theme = static_cast<Qtitan::RibbonStyle::Theme>(storedTheme);
        ribbonStyle->setTheme(theme);
        updateWindowChromePalette(theme);
    }
}

void MainWindow::persistThemeSettings() const
{
    if (auto* ribbonStyle = qobject_cast<Qtitan::RibbonStyle*>(qApp->style())) {
        QSettings().setValue(settingskeys::kUiTheme, static_cast<int>(ribbonStyle->getTheme()));
    }
}

void MainWindow::syncProfileDockForMeasurementMode(bool measurementEnabled)
{
    if (profileDock_ == nullptr) {
        return;
    }

    const bool targetVisible = measurementEnabled;
    if (profileDock_->isVisible() != targetVisible) {
        profileDock_->setVisible(targetVisible);
    }

    if (showProfileDockAction_ != nullptr && showProfileDockAction_->isChecked() != targetVisible) {
        const QSignalBlocker blocker(showProfileDockAction_);
        showProfileDockAction_->setChecked(targetVisible);
    }
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

void MainWindow::updateMeasurementPanel()
{
    if (viewer_ == nullptr || measurementToggleButton_ == nullptr || measurementClearButton_ == nullptr) {
        return;
    }

    const MeasurementResult& measurementResult = viewer_->measurementResult();
    const ClearanceAnalysisResult clearanceAnalysis = analyzeClearancePath(
        measurementResult.points,
        static_cast<float>(clearanceWarningThresholdMeters_));
    const ClearanceRuleEvaluationResult ruleEvaluation = evaluateClearanceRules(
        clearanceAnalysis,
        { clearanceRulePreset_, static_cast<float>(clearanceWarningThresholdMeters_) });
    measurementToggleButton_->setText(
        viewer_->measurementEnabled() ? tr("Stop Measurement") : tr("Start Measurement"));
    measurementClearButton_->setText(tr("Clear Measurement"));

    measurementStartValueLabel_->setText(measurementPointText(measurementResult, true));
    measurementEndValueLabel_->setText(measurementPointText(measurementResult, false));
    measurementDistanceValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.distance3d) : tr("N/A"));
    measurementHorizontalDistanceValueLabel_->setText(
        clearanceAnalysis.isValid() ? formatCoordinate(clearanceAnalysis.totalHorizontalDistance) : tr("N/A"));
    measurementDeltaZValueLabel_->setText(
        measurementResult.isComplete() ? formatCoordinate(measurementResult.deltaZ) : tr("N/A"));
    measurementSegmentsValueLabel_->setText(
        measurementResult.isComplete()
            ? QLocale().toString(measurementResult.pointCount() - 1)
            : QStringLiteral("0"));

    QString clearanceStatusText = tr("Add at least two measured points to analyze corridor clearance.");
    QString clearanceStatusStyle = QStringLiteral("color: #475569;");
    if (clearanceRuleBandsValueLabel_ != nullptr) {
        clearanceRuleBandsValueLabel_->setText(
            ruleEvaluation.enabled()
                ? tr("Advisory %1 m | Warning %2 m | Critical %3 m")
                    .arg(formatCoordinate(ruleEvaluation.advisoryThreshold))
                    .arg(formatCoordinate(ruleEvaluation.warningThreshold))
                    .arg(formatCoordinate(ruleEvaluation.criticalThreshold))
                : tr("Disabled"));
    }
    if (!clearanceAnalysis.isValid()) {
        clearanceShortestValueLabel_->setText(tr("N/A"));
        clearanceWarningCountValueLabel_->setText(tr("0 / 0 / 0"));
    } else {
        clearanceShortestValueLabel_->setText(formatCoordinate(clearanceAnalysis.minimumSegmentDistance));
        clearanceWarningCountValueLabel_->setText(
            tr("%1 / %2 / %3")
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.criticalCount)));

        if (!ruleEvaluation.enabled()) {
            clearanceStatusText = tr("Clearance threshold is disabled. Set a value above 0 m to enable risk bands.");
            clearanceStatusStyle = QStringLiteral("color: #475569;");
        } else if (ruleEvaluation.criticalCount > 0) {
            clearanceStatusText = tr("%1 critical segment(s), %2 warning segment(s), and %3 advisory segment(s) were detected under %4.")
                .arg(QLocale().toString(ruleEvaluation.criticalCount))
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(ruleEvaluation.presetLabel);
            clearanceStatusStyle = QStringLiteral("color: #b91c1c; font-weight: 600;");
        } else if (ruleEvaluation.warningCount > 0 || ruleEvaluation.advisoryCount > 0) {
            clearanceStatusText = tr("%1 warning segment(s) and %2 advisory segment(s) were detected under %3.")
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(ruleEvaluation.presetLabel);
            clearanceStatusStyle = QStringLiteral("color: #b45309; font-weight: 600;");
        } else {
            clearanceStatusText = tr("All measured segments stay outside the active %1 risk bands.")
                .arg(ruleEvaluation.presetLabel);
            clearanceStatusStyle = QStringLiteral("color: #15803d; font-weight: 600;");
        }
    }

    clearanceStatusValueLabel_->setText(clearanceStatusText);
    clearanceStatusValueLabel_->setStyleSheet(clearanceStatusStyle);
    updateClearanceSegmentsTable(clearanceAnalysis);
    if (profilePlotWidget_ != nullptr) {
        profilePlotWidget_->setAnalysisResult(clearanceAnalysis);
        profilePlotWidget_->setRuleEvaluation(ruleEvaluation);
        profilePlotWidget_->setProfileMarkers(projectProfileMarkers(
            clearanceAnalysis,
            viewer_->towerMarkers(),
            viewer_->selectedTowerIndex(),
            viewer_->inspectionIssues(),
            viewer_->selectedIssueIndex()));
        profilePlotWidget_->setSelectedSegmentIndex(
            clearanceSegmentsTableWidget_ != nullptr ? clearanceSegmentsTableWidget_->currentRow() : -1);
    }
}

void MainWindow::updateClearanceSegmentsTable(const ClearanceAnalysisResult& clearanceAnalysis)
{
    if (clearanceSegmentsSummaryLabel_ == nullptr || clearanceSegmentsTableWidget_ == nullptr) {
        return;
    }

    const ClearanceRuleEvaluationResult ruleEvaluation = evaluateClearanceRules(
        clearanceAnalysis,
        { clearanceRulePreset_, static_cast<float>(clearanceWarningThresholdMeters_) });

    const int previousRow = clearanceSegmentsTableWidget_->currentRow();
    const QSignalBlocker blocker(clearanceSegmentsTableWidget_);
    clearanceSegmentsTableWidget_->setRowCount(0);

    if (!clearanceAnalysis.isValid()) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("Add at least two measured points to list corridor segments and export clearance details."));
        clearanceSegmentsTableWidget_->clearSelection();
        return;
    }

    if (!ruleEvaluation.enabled()) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("Listed %1 path segment(s). Set a threshold above 0 m to enable electric-scene risk bands.")
                .arg(QLocale().toString(clearanceAnalysis.segments.size())));
    } else if (ruleEvaluation.criticalCount > 0 || ruleEvaluation.warningCount > 0 || ruleEvaluation.advisoryCount > 0) {
        clearanceSegmentsSummaryLabel_->setText(
            tr("%1 critical, %2 warning, %3 advisory segment(s) under %4. Select a row to highlight it in the profile or export the full list.")
                .arg(QLocale().toString(ruleEvaluation.criticalCount))
                .arg(QLocale().toString(ruleEvaluation.warningCount))
                .arg(QLocale().toString(ruleEvaluation.advisoryCount))
                .arg(ruleEvaluation.presetLabel));
    } else {
        clearanceSegmentsSummaryLabel_->setText(
            tr("All %1 segment(s) stay outside the current %2 risk bands.")
                .arg(QLocale().toString(clearanceAnalysis.segments.size()))
                .arg(ruleEvaluation.presetLabel));
    }

    int preferredRow = previousRow;
    if (preferredRow < 0 || preferredRow >= clearanceAnalysis.segments.size()) {
        preferredRow = 0;
        for (int segmentIndex = 0; segmentIndex < clearanceAnalysis.segments.size(); ++segmentIndex) {
            if (segmentIndex < ruleEvaluation.segmentEvaluations.size()
                && ruleEvaluation.segmentEvaluations.at(segmentIndex).severity != AnalysisSeverity::None) {
                preferredRow = segmentIndex;
                break;
            }
        }
    }

    for (int segmentIndex = 0; segmentIndex < clearanceAnalysis.segments.size(); ++segmentIndex) {
        const ClearanceSegment& segment = clearanceAnalysis.segments.at(segmentIndex);
        const ClearanceSegmentEvaluation evaluation = segmentIndex < ruleEvaluation.segmentEvaluations.size()
            ? ruleEvaluation.segmentEvaluations.at(segmentIndex)
            : ClearanceSegmentEvaluation();
        clearanceSegmentsTableWidget_->insertRow(segmentIndex);

        auto createReadOnlyItem = [](const QString& text, const QColor& color = QColor(), Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            item->setTextAlignment(alignment);
            if (color.isValid()) {
                item->setForeground(color);
            }
            return item;
        };

        const QColor statusColor = analysisSeverityColor(evaluation.severity);
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            0,
            createReadOnlyItem(QLocale().toString(segmentIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            1,
            createReadOnlyItem(QLocale().toString(segment.startPointIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            2,
            createReadOnlyItem(QLocale().toString(segment.endPointIndex + 1), QColor(), Qt::AlignCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            3,
            createReadOnlyItem(
                tr("%1 - %2 m")
                    .arg(formatCoordinate(segment.chainageStart))
                    .arg(formatCoordinate(segment.chainageEnd))));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            4,
            createReadOnlyItem(formatCoordinate(segment.horizontalDistance), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            5,
            createReadOnlyItem(formatCoordinate(segment.distance3d), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            6,
            createReadOnlyItem(formatCoordinate(segment.deltaZ), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        clearanceSegmentsTableWidget_->setItem(
            segmentIndex,
            7,
            createReadOnlyItem(evaluation.severityLabel, statusColor, Qt::AlignCenter));
    }

    if (clearanceSegmentsTableWidget_->rowCount() > 0) {
        const int normalizedRow = std::max(0, std::min(preferredRow, clearanceSegmentsTableWidget_->rowCount() - 1));
        clearanceSegmentsTableWidget_->setCurrentCell(normalizedRow, 0);
    } else {
        clearanceSegmentsTableWidget_->clearSelection();
    }
}

void MainWindow::updateVegetationRiskPanel()
{
    if (vegetationRiskCountValueLabel_ == nullptr || vegetationRiskStatusValueLabel_ == nullptr || vegetationRisksTableWidget_ == nullptr) {
        return;
    }

    const ClearanceAnalysisResult pathAnalysis = (viewer_ != nullptr)
        ? analyzeClearancePath(viewer_->measurementResult().points, static_cast<float>(clearanceWarningThresholdMeters_))
        : ClearanceAnalysisResult();
    const bool pathReady = pathAnalysis.isValid();

    vegetationRiskCountValueLabel_->setText(
        vegetationRiskResults_.isEmpty()
            ? tr("No vegetation risk clusters available.")
            : tr("%1 vegetation risk cluster(s)").arg(QLocale().toString(vegetationRiskResults_.size())));
    vegetationRiskStatusValueLabel_->setText(
        !pathReady
            ? tr("Measure a corridor path first, then run the analysis.")
            : vegetationRiskResults_.isEmpty()
                ? tr("Run analysis to scan points near the measured corridor and propose vegetation issues.")
                : tr("Select a cluster to focus it in the scene or convert it into inspection issues."));
    vegetationRiskSummaryLabel_->setText(
        tr("Search radius %1 m | Cluster gap %2 m | Min cluster points %3 | Classification preference %4")
            .arg(formatCoordinate(static_cast<float>(vegetationSearchRadiusMeters_)))
            .arg(formatCoordinate(static_cast<float>(vegetationClusterGapMeters_)))
            .arg(QLocale().toString(vegetationClusterPointCount_))
            .arg(preferVegetationClassification_ ? tr("on") : tr("off")));

    const QSignalBlocker blocker(vegetationRisksTableWidget_);
    vegetationRisksTableWidget_->setRowCount(0);
    for (int riskIndex = 0; riskIndex < vegetationRiskResults_.size(); ++riskIndex) {
        const VegetationRiskRecord& risk = vegetationRiskResults_.at(riskIndex);
        vegetationRisksTableWidget_->insertRow(riskIndex);

        auto createReadOnlyItem = [](const QString& text, const QColor& color = QColor(), Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* item = new QTableWidgetItem(text);
            item->setFlags((item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            item->setTextAlignment(alignment);
            if (color.isValid()) {
                item->setForeground(color);
            }
            return item;
        };

        const QColor severityColor = analysisSeverityColor(risk.severity);
        vegetationRisksTableWidget_->setItem(riskIndex, 0, createReadOnlyItem(QLocale().toString(riskIndex + 1), QColor(), Qt::AlignCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 1, createReadOnlyItem(risk.title));
        vegetationRisksTableWidget_->setItem(riskIndex, 2, createReadOnlyItem(analysisSeverityDisplayName(risk.severity), severityColor, Qt::AlignCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 3, createReadOnlyItem(formatCoordinate(risk.minimumDistance), QColor(), Qt::AlignRight | Qt::AlignVCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 4, createReadOnlyItem(
            tr("%1 - %2 m").arg(formatCoordinate(risk.chainageStart)).arg(formatCoordinate(risk.chainageEnd)),
            QColor(),
            Qt::AlignRight | Qt::AlignVCenter));
        vegetationRisksTableWidget_->setItem(riskIndex, 5, createReadOnlyItem(risk.nearestTowerName.isEmpty() ? tr("N/A") : risk.nearestTowerName));
        vegetationRisksTableWidget_->setItem(riskIndex, 6, createReadOnlyItem(QLocale().toString(risk.supportPointCount), QColor(), Qt::AlignCenter));
    }

    if (selectedVegetationRiskIndex_ >= 0 && selectedVegetationRiskIndex_ < vegetationRisksTableWidget_->rowCount()) {
        vegetationRisksTableWidget_->setCurrentCell(selectedVegetationRiskIndex_, 1);
    } else {
        vegetationRisksTableWidget_->clearSelection();
    }
}

void MainWindow::updateRoutePlanningPanel()
{
    if (routeStatusValueLabel_ == nullptr
        || routeSummaryValueLabel_ == nullptr
        || routeQaSummaryValueLabel_ == nullptr
        || routePartPointsTableWidget_ == nullptr
        || routeWaypointsTableWidget_ == nullptr
        || routeWaypointTargetsTableWidget_ == nullptr
        || routeQaIssuesTableWidget_ == nullptr) {
        return;
    }

    if (currentPowerlineRoute_.waypoints.isEmpty()) {
        routeQaReport_ = RouteQaReport();
    } else {
        routeQaReport_ = evaluatePowerlineRouteQa(currentPowerlineRoute_, routePlanningOptions_.aircraftProfile);
    }

    const QString routeQaStateText = currentPowerlineRoute_.waypoints.isEmpty()
        ? tr("waiting for route data")
        : routeQaSummaryText(routeQaReport_);

    const QString routeEditStateText = routeEditingEnabled_ ? tr("Enabled") : tr("Locked");
    const bool routeRoamActive = viewer_ != nullptr && viewer_->inspectionRouteRoamActive();
    const bool routeRoamPaused = viewer_ != nullptr && viewer_->inspectionRouteRoamPaused();
    const QString routeRoamStateText = !routeRoamActive
        ? tr("Stopped")
        : (routeRoamPaused ? tr("Paused") : tr("Playing"));
    const QString routeRoamModeText =
        viewer_ != nullptr && viewer_->inspectionRouteRoamViewMode() == RouteRoamViewMode::FirstPerson
        ? tr("First Person")
        : tr("Third Person");
    const QString routeRoamStatusText = tr("Roam: %1 (%2, %3 m/s)")
        .arg(
            routeRoamStateText,
            routeRoamModeText,
            formatCoordinate(static_cast<float>(viewer_ != nullptr ? viewer_->inspectionRouteRoamSpeedMetersPerSecond() : 0.0)));
    const QString routeRoamVisibilityHint =
        viewer_ != nullptr && !viewer_->inspectionRouteVisible()
        ? QStringLiteral("\n") + tr("Route visibility is off. Enable route display before starting roam.")
        : QString();
    routeStatusValueLabel_->setText(
        currentPowerlineRoute_.waypoints.isEmpty()
            ? tr("No route generated. Analyze vegetation risks first, then generate inspection route.")
                + QStringLiteral("\n")
                + tr("Edit Route: %1").arg(routeEditStateText)
                + QStringLiteral("\n")
                + tr("Route QA: %1").arg(routeQaStateText)
                + QStringLiteral("\n")
                + routeRoamStatusText
                + routeRoamVisibilityHint
            : tr("%1 waypoint(s), %2 part point(s) ready for scene review and KML/KMZ interoperability.")
                .arg(QLocale().toString(currentPowerlineRoute_.waypoints.size()))
                .arg(QLocale().toString(currentPowerlineRoute_.partPoints.size()))
                + QStringLiteral("\n")
                + tr("Edit Route: %1").arg(routeEditStateText)
                + QStringLiteral("\n")
                + tr("Route QA: %1").arg(routeQaStateText)
                + QStringLiteral("\n")
                + routeRoamStatusText
                + routeRoamVisibilityHint);
    routeSummaryValueLabel_->setText(
        tr("%1 -> %2 | DJI profile: %3 | Safety %4 m | Speed %5 m/s | Spacing %6 m | Smoothing %7%% | Height offset %8 m")
            .arg(formatCoordinateSystemCode(projectCoordinateSystems_.pointCloudCrs, tr("Unset")))
            .arg(formatCoordinateSystemCode(projectCoordinateSystems_.geographicCrs, QStringLiteral("EPSG:4326")))
            .arg(djiAircraftProfileDisplayName(routePlanningOptions_.aircraftProfile))
            .arg(formatCoordinate(routePlanningOptions_.safety.safetyHeightMeters))
            .arg(formatCoordinate(routePlanningOptions_.safety.defaultWaypointSpeedMps))
            .arg(formatCoordinate(routePlanningOptions_.generation.waypointSpacingMeters))
            .arg(formatCoordinate(routePlanningOptions_.generation.smoothingStrengthPercent))
            .arg(formatCoordinate(routePlanningOptions_.safety.heightOffsetMeters)));

    const QColor routeQaSummaryColor = routeQaReport_.hasBlockingIssues()
        ? routeQaSeverityColor(RouteQaSeverity::Blocking)
        : (routeQaReport_.hasWarnings()
            ? routeQaSeverityColor(RouteQaSeverity::Warning)
            : QColor(22, 101, 52));
    routeQaSummaryValueLabel_->setText(
        currentPowerlineRoute_.waypoints.isEmpty()
            ? tr("Route QA is waiting for route data.")
            : tr("Route QA summary: %1").arg(routeQaSummaryText(routeQaReport_)));
    routeQaSummaryValueLabel_->setStyleSheet(
        QStringLiteral("color: %1; font-weight: 600;")
            .arg(routeQaSummaryColor.name()));

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
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            0,
            createReadOnlyItem(QLocale().toString(waypointIndex + 1), Qt::AlignCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnPart,
            createReadOnlyItem(routeWaypointPartSummary(waypoint, partPointByIndex)));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnX,
            createReadOnlyItem(formatCoordinate(waypoint.localPoint.x), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnY,
            createReadOnlyItem(formatCoordinate(waypoint.localPoint.y), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnZ,
            createReadOnlyItem(formatCoordinate(waypoint.localPoint.z), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnAircraftYaw,
            createReadOnlyItem(formatCoordinate(waypoint.aircraftYawDeg), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnGimbalPitch,
            createReadOnlyItem(formatCoordinate(waypoint.gimbalPitchDeg), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnCameraYaw,
            createReadOnlyItem(formatCoordinate(firstTarget.cameraYawDeg), Qt::AlignRight | Qt::AlignVCenter));
        routeWaypointsTableWidget_->setItem(
            waypointIndex,
            kRouteWaypointColumnCameraPitch,
            createReadOnlyItem(formatCoordinate(firstTarget.cameraPitchDeg), Qt::AlignRight | Qt::AlignVCenter));
    }

    const int linkedPartIndex =
        selectedRouteWaypointIndex_ >= 0 && selectedRouteWaypointIndex_ < currentPowerlineRoute_.waypoints.size()
        ? routeWaypointRepresentativePartIndex(currentPowerlineRoute_.waypoints.at(selectedRouteWaypointIndex_))
        : -1;
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
        selectedRouteWaypointTargetIndex_ = selectedWaypoint.captureTargets.isEmpty()
            ? -1
            : std::clamp(selectedRouteWaypointTargetIndex_, 0, selectedWaypoint.captureTargets.size() - 1);

        for (int targetIndex = 0; targetIndex < selectedWaypoint.captureTargets.size(); ++targetIndex) {
            const RouteCaptureTarget& captureTarget = selectedWaypoint.captureTargets.at(targetIndex);
            routeWaypointTargetsTableWidget_->insertRow(targetIndex);

            PointRecord resolvedTargetPoint = captureTarget.targetLocalPoint;
            if (captureTarget.partIndex > 0 && partPointByIndex.contains(captureTarget.partIndex)) {
                resolvedTargetPoint = partPointByIndex.value(captureTarget.partIndex).localPoint;
            }

            const QString targetPointText = tr("%1 / %2 / %3")
                .arg(formatCoordinate(resolvedTargetPoint.x))
                .arg(formatCoordinate(resolvedTargetPoint.y))
                .arg(formatCoordinate(resolvedTargetPoint.z));
            const QString partDisplayName = routeCaptureTargetDisplayName(captureTarget, partPointByIndex, targetIndex + 1);

            QTableWidgetItem* indexItem = createReadOnlyItem(QLocale().toString(targetIndex + 1), Qt::AlignCenter);
            indexItem->setData(Qt::UserRole, targetIndex);
            routeWaypointTargetsTableWidget_->setItem(targetIndex, 0, indexItem);
            routeWaypointTargetsTableWidget_->setItem(
                targetIndex,
                kRouteWaypointTargetColumnPart,
                createReadOnlyItem(partDisplayName));
            routeWaypointTargetsTableWidget_->setItem(
                targetIndex,
                kRouteWaypointTargetColumnFocalRatio,
                createReadOnlyItem(formatCoordinate(captureTarget.focalLengthRatio), Qt::AlignRight | Qt::AlignVCenter));
            routeWaypointTargetsTableWidget_->setItem(
                targetIndex,
                kRouteWaypointTargetColumnCameraYaw,
                createReadOnlyItem(formatCoordinate(captureTarget.cameraYawDeg), Qt::AlignRight | Qt::AlignVCenter));
            routeWaypointTargetsTableWidget_->setItem(
                targetIndex,
                kRouteWaypointTargetColumnCameraPitch,
                createReadOnlyItem(formatCoordinate(captureTarget.cameraPitchDeg), Qt::AlignRight | Qt::AlignVCenter));
            routeWaypointTargetsTableWidget_->setItem(
                targetIndex,
                kRouteWaypointTargetColumnTargetPoint,
                createReadOnlyItem(targetPointText, Qt::AlignRight | Qt::AlignVCenter));
        }

        if (selectedRouteWaypointTargetIndex_ >= 0
            && selectedRouteWaypointTargetIndex_ < routeWaypointTargetsTableWidget_->rowCount()) {
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

        const QString partNameText = qaIssue.partIndex > 0
            ? (partPointByIndex.contains(qaIssue.partIndex)
                ? routePartDisplayName(partPointByIndex.value(qaIssue.partIndex))
                : tr("Part %1").arg(QLocale().toString(qaIssue.partIndex)))
            : tr("-");
        QString descriptionText = qaIssue.message;
        if (!qaIssue.detail.trimmed().isEmpty()) {
            descriptionText = tr("%1 (%2)").arg(qaIssue.message, qaIssue.detail);
        }

        QTableWidgetItem* severityItem = createReadOnlyItem(routeQaSeverityDisplayName(qaIssue.severity), Qt::AlignCenter);
        severityItem->setForeground(routeQaSeverityColor(qaIssue.severity));
        routeQaIssuesTableWidget_->setItem(qaIssueIndex, kRouteQaColumnSeverity, severityItem);
        routeQaIssuesTableWidget_->setItem(
            qaIssueIndex,
            kRouteQaColumnType,
            createReadOnlyItem(routeQaIssueTypeDisplayName(qaIssue.type), Qt::AlignCenter));
        routeQaIssuesTableWidget_->setItem(
            qaIssueIndex,
            kRouteQaColumnLocation,
            createReadOnlyItem(routeQaIssueLocationText(qaIssue), Qt::AlignCenter));
        routeQaIssuesTableWidget_->setItem(
            qaIssueIndex,
            kRouteQaColumnPart,
            createReadOnlyItem(partNameText));
        routeQaIssuesTableWidget_->setItem(
            qaIssueIndex,
            kRouteQaColumnDescription,
            createReadOnlyItem(descriptionText));
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

    const bool showCoordinates =
        routeWaypointShowCoordinatesCheckBox_ == nullptr
        || routeWaypointShowCoordinatesCheckBox_->isChecked();
    const bool showCaptureAngles =
        routeWaypointShowCaptureAnglesCheckBox_ == nullptr
        || routeWaypointShowCaptureAnglesCheckBox_->isChecked();

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

    const bool showCoordinates =
        routePartShowCoordinatesCheckBox_ == nullptr
        || routePartShowCoordinatesCheckBox_->isChecked();
    const bool showCaptureAngles =
        routePartShowCaptureAnglesCheckBox_ == nullptr
        || routePartShowCaptureAnglesCheckBox_->isChecked();

    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnCameraAngle, !showCaptureAngles);
    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnX, !showCoordinates);
    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnY, !showCoordinates);
    routePartPointsTableWidget_->setColumnHidden(kRoutePartColumnZ, !showCoordinates);
}

void MainWindow::rebuildProjectTree()
{
    if (projectTreeWidget_ == nullptr || viewer_ == nullptr) {
        return;
    }

    syncDataManagerTrajectory();
    const DataManager& dataManager = DataManager::instance();

    const QTreeWidgetItem* previousItem = projectTreeWidget_->currentItem();
    const QString previousItemType = projectTreeItemType(previousItem);
    const QString previousFilePath = projectTreeItemFilePath(previousItem);
    const int previousIssueIndex = previousItem != nullptr ? previousItem->data(0, kProjectTreeIssueIndexRole).toInt() : -1;
    const QSignalBlocker blocker(projectTreeWidget_);
    updatingProjectTree_ = true;
    projectTreeWidget_->clear();

    const QString projectName = currentProjectFilePath_.trimmed().isEmpty()
        ? tr("Unsaved Project")
        : QFileInfo(currentProjectFilePath_).completeBaseName();
    auto* coordinateSystemsItem = new QTreeWidgetItem(projectTreeWidget_, QStringList { tr("Project Properties") });
    coordinateSystemsItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("coordinateSystemsItem"));
    coordinateSystemsItem->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogContentsView));
    coordinateSystemsItem->setToolTip(
        0,
        tr("Project: %1\nCurrent project CRS: %2")
            .arg(projectName, formatProjectCoordinateSystemsSummary(projectCoordinateSystems_)));

    auto* pointCloudGroup = new QTreeWidgetItem(projectTreeWidget_, QStringList {
        tr("Point Clouds (%1)").arg(QLocale().toString(dataManager.pointCloudDatasets().size()))
    });
    pointCloudGroup->setData(0, kProjectTreeItemTypeRole, QStringLiteral("pointCloudGroup"));
    pointCloudGroup->setIcon(0, style()->standardIcon(QStyle::SP_DriveHDIcon));

    auto* imageGroup = new QTreeWidgetItem(projectTreeWidget_, QStringList {
        tr("Images (%1)").arg(QLocale().toString(dataManager.imageItems().size()))
    });
    imageGroup->setData(0, kProjectTreeItemTypeRole, QStringLiteral("imageGroup"));
    imageGroup->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));

    auto* trajectoryGroup = new QTreeWidgetItem(projectTreeWidget_, QStringList {
        tr("Trajectories (%1)").arg(QLocale().toString(dataManager.hasTrajectory() ? 1 : 0))
    });
    trajectoryGroup->setData(0, kProjectTreeItemTypeRole, QStringLiteral("trajectoryGroup"));
    trajectoryGroup->setIcon(0, style()->standardIcon(QStyle::SP_ArrowRight));

    QTreeWidgetItem* selectedItem = nullptr;
    if (previousItemType == QStringLiteral("coordinateSystemsItem")) {
        selectedItem = coordinateSystemsItem;
    } else if (previousItemType == QStringLiteral("pointCloudGroup")) {
        selectedItem = pointCloudGroup;
    } else if (previousItemType == QStringLiteral("imageGroup")) {
        selectedItem = imageGroup;
    } else if (previousItemType == QStringLiteral("trajectoryGroup")) {
        selectedItem = trajectoryGroup;
    }

    for (const PointCloudDatasetInfo& datasetInfo : dataManager.pointCloudDatasets()) {
        const QFileInfo fileInfo(datasetInfo.filePath);
        const QString absoluteFilePath = fileInfo.absoluteFilePath();
        auto* datasetItem = new QTreeWidgetItem(pointCloudGroup, QStringList {
            fileInfo.fileName().isEmpty() ? absoluteFilePath : fileInfo.fileName()
        });
        datasetItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("pointCloudItem"));
        datasetItem->setData(0, kProjectTreeFilePathRole, absoluteFilePath);
        datasetItem->setToolTip(0, absoluteFilePath);
        datasetItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        datasetItem->setFlags(datasetItem->flags() | Qt::ItemIsUserCheckable);
        datasetItem->setCheckState(0, datasetInfo.visible ? Qt::Checked : Qt::Unchecked);

        if (previousItemType == QStringLiteral("pointCloudItem")
            && previousFilePath.compare(absoluteFilePath, Qt::CaseInsensitive) == 0) {
            selectedItem = datasetItem;
        }
    }

    for (const DataImageItem& imageInfoRecord : dataManager.imageItems()) {
        const QFileInfo imageInfo(imageInfoRecord.filePath);
        const QString absoluteImagePath = imageInfo.absoluteFilePath();
        QString itemText = imageInfo.fileName();
        if (itemText.isEmpty()) {
            itemText = imageInfoRecord.title.trimmed().isEmpty()
                ? tr("Image %1").arg(QLocale().toString(imageInfoRecord.issueIndex + 1))
                : imageInfoRecord.title;
        }

        auto* imageItem = new QTreeWidgetItem(imageGroup, QStringList { itemText });
        imageItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("imageItem"));
        imageItem->setData(0, kProjectTreeFilePathRole, absoluteImagePath);
        imageItem->setData(0, kProjectTreeIssueIndexRole, imageInfoRecord.issueIndex);
        imageItem->setToolTip(0, tr("%1\n%2").arg(imageInfoRecord.title, absoluteImagePath));
        imageItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        imageItem->setFlags(imageItem->flags() | Qt::ItemIsUserCheckable);
        imageItem->setCheckState(0, imageInfoRecord.visible ? Qt::Checked : Qt::Unchecked);

        if (previousItemType == QStringLiteral("imageItem")
            && previousIssueIndex == imageInfoRecord.issueIndex
            && previousFilePath.compare(absoluteImagePath, Qt::CaseInsensitive) == 0) {
            selectedItem = imageItem;
        }
    }

    if (dataManager.hasTrajectory()) {
        const DataTrajectoryItem& trajectoryItem = dataManager.trajectoryItem();
        const QString routeName = trajectoryItem.name.trimmed().isEmpty()
            ? tr("Inspection Route")
            : trajectoryItem.name.trimmed();
        auto* routeItem = new QTreeWidgetItem(trajectoryGroup, QStringList {
            tr("%1 (%2 WP)").arg(routeName, QLocale().toString(trajectoryItem.points.size()))
        });
        routeItem->setData(0, kProjectTreeItemTypeRole, QStringLiteral("trajectoryItem"));
        routeItem->setToolTip(0, tr("%1 waypoint(s)").arg(QLocale().toString(trajectoryItem.points.size())));
        routeItem->setIcon(0, style()->standardIcon(QStyle::SP_ArrowForward));
        routeItem->setFlags(routeItem->flags() | Qt::ItemIsUserCheckable);
        routeItem->setCheckState(0, trajectoryItem.visible ? Qt::Checked : Qt::Unchecked);

        if (previousItemType == QStringLiteral("trajectoryItem")) {
            selectedItem = routeItem;
        }
    }

    pointCloudGroup->setExpanded(true);
    imageGroup->setExpanded(true);
    trajectoryGroup->setExpanded(true);

    if (selectedItem == nullptr) {
        QTreeWidgetItemIterator it(projectTreeWidget_);
        while (*it != nullptr) {
            const QString itemType = (*it)->data(0, kProjectTreeItemTypeRole).toString();
            if (itemType == QStringLiteral("coordinateSystemsItem")
                || itemType == QStringLiteral("pointCloudItem")
                || itemType == QStringLiteral("imageItem")
                || itemType == QStringLiteral("trajectoryItem")) {
                selectedItem = *it;
                break;
            }
            ++it;
        }
    }

    projectTreeWidget_->setCurrentItem(selectedItem != nullptr ? selectedItem : coordinateSystemsItem);
    updatingProjectTree_ = false;
    refreshProjectTreeFilter();
}

void MainWindow::refreshProjectTreeFilter()
{
    if (projectExplorerController_ != nullptr) {
        projectExplorerController_->refreshFilter();
    }
    updateActionState();
}

QString MainWindow::selectedDatasetPath() const
{
    if (projectTreeWidget_ == nullptr || projectTreeWidget_->currentItem() == nullptr) {
        return QString();
    }

    const QTreeWidgetItem* currentItem = projectTreeWidget_->currentItem();
    if (currentItem->data(0, kProjectTreeItemTypeRole).toString() != QStringLiteral("pointCloudItem")) {
        return QString();
    }

    return currentItem->data(0, kProjectTreeFilePathRole).toString();
}

void MainWindow::focusProjectTreeItem(QTreeWidgetItem* item)
{
    if (viewer_ == nullptr || item == nullptr) {
        return;
    }

    const QString itemType = projectTreeItemType(item);
    if (itemType == QStringLiteral("coordinateSystemsItem")) {
        openProjectCoordinateSystems();
        return;
    }

    if (itemType == QStringLiteral("pointCloudItem")) {
        const QString filePath = projectTreeItemFilePath(item);
        for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
            if (datasetInfo.filePath.compare(filePath, Qt::CaseInsensitive) == 0) {
                viewer_->focusOnBounds(datasetInfo.minBounds, datasetInfo.maxBounds, 0.9);
                return;
            }
        }
        return;
    }

    if (itemType == QStringLiteral("imageItem")) {
        const int issueIndex = item->data(0, kProjectTreeIssueIndexRole).toInt();
        for (const DataImageItem& imageItem : DataManager::instance().imageItems()) {
            if (imageItem.issueIndex != issueIndex) {
                continue;
            }
            viewer_->setSelectedIssueIndex(issueIndex);
            viewer_->focusOnPoint(imageItem.point, 0.2);
            updateIssuePanel();
            break;
        }
        return;
    }

    if (itemType == QStringLiteral("trajectoryItem")) {
        PointRecord minBounds;
        PointRecord maxBounds;
        const QList<PointRecord> routePoints = toRouteDisplayPoints(currentPowerlineRoute_);
        if (boundsFromPoints(routePoints, &minBounds, &maxBounds)) {
            viewer_->focusOnBounds(minBounds, maxBounds, 1.15);
        }
    }
}

void MainWindow::applyProjectTreeItemCheckState(QTreeWidgetItem* item)
{
    if (updatingProjectTree_ || viewer_ == nullptr || item == nullptr) {
        return;
    }

    const QString itemType = projectTreeItemType(item);
    const bool visible = item->checkState(0) != Qt::Unchecked;
    if (itemType == QStringLiteral("pointCloudItem")) {
        viewer_->setPointCloudDatasetVisible(projectTreeItemFilePath(item), visible);
        updateDatasetPanel();
        updateActionState();
        return;
    }

    if (itemType == QStringLiteral("imageItem")) {
        viewer_->setInspectionIssueVisible(item->data(0, kProjectTreeIssueIndexRole).toInt(), visible);
        updateIssuePanel();
        updateActionState();
        return;
    }

    if (itemType == QStringLiteral("trajectoryItem")) {
        viewer_->setInspectionRouteVisible(visible);
        updateRoutePlanningPanel();
        updateActionState();
    }
}

void MainWindow::setProjectTreeGroupVisibility(const QString& groupType, bool visible)
{
    if (viewer_ == nullptr) {
        return;
    }

    if (groupType == QStringLiteral("pointCloudGroup")) {
        for (const PointCloudDatasetInfo& datasetInfo : DataManager::instance().pointCloudDatasets()) {
            viewer_->setPointCloudDatasetVisible(datasetInfo.filePath, visible);
        }
        updateDatasetPanel();
    } else if (groupType == QStringLiteral("imageGroup")) {
        for (const DataImageItem& imageItem : DataManager::instance().imageItems()) {
            viewer_->setInspectionIssueVisible(imageItem.issueIndex, visible);
        }
        updateIssuePanel();
    } else if (groupType == QStringLiteral("trajectoryGroup")) {
        viewer_->setInspectionRouteVisible(visible);
        updateRoutePlanningPanel();
    }

    rebuildProjectTree();
    updateActionState();
}

void MainWindow::clearAllProjectImages()
{
    if (viewer_ == nullptr) {
        return;
    }

    QList<InspectionIssue> issues = viewer_->inspectionIssues();
    if (issues.isEmpty()) {
        return;
    }

    bool changed = false;
    for (InspectionIssue& issue : issues) {
        if (issue.imagePath.trimmed().isEmpty()) {
            continue;
        }

        issue.imagePath.clear();
        changed = true;
    }

    if (!changed) {
        return;
    }

    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    viewer_->setInspectionIssues(issues);
    viewer_->setSelectedIssueIndex(selectedIssueIndex);
    updateIssuePanel();
    rebuildProjectTree();
    updateActionState();
    showUserMessage(LogLevel::Info, tr("All image attachments were removed from the project."), 3000);
}

bool MainWindow::attachImageToIssue(int issueIndex)
{
    if (viewer_ == nullptr) {
        return false;
    }

    const QList<InspectionIssue> issues = viewer_->inspectionIssues();
    if (issueIndex < 0 || issueIndex >= issues.size()) {
        showUserMessage(LogLevel::Warning, tr("Select an inspection issue before attaching an image."), 3000);
        return false;
    }

    const InspectionIssue& selectedIssue = issues.at(issueIndex);
    const QString initialPath = selectedIssue.imagePath.trimmed();
    const QString filePath = showStyledOpenFileNameDialog(
        this,
        tr("Attach Image"),
        initialPath.isEmpty() ? QDir::homePath() : QFileInfo(initialPath).absolutePath(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All Files (*.*)"));
    if (filePath.isEmpty()) {
        return false;
    }

    InspectionIssue updatedIssue = selectedIssue;
    updatedIssue.imagePath = QFileInfo(filePath).absoluteFilePath();
    if (!viewer_->updateInspectionIssue(issueIndex, updatedIssue)) {
        showUserMessage(LogLevel::Warning, tr("Unable to attach the selected image."), 3000);
        return false;
    }

    viewer_->setSelectedIssueIndex(issueIndex);
    updateIssuePanel();
    rebuildProjectTree();
    updateActionState();
    showUserMessage(LogLevel::Info, tr("Image attached to the selected inspection issue."), 3000);
    return true;
}

void MainWindow::showProjectTreeContextMenu(const QPoint& pos)
{
    if (projectTreeWidget_ == nullptr) {
        return;
    }

    if (QTreeWidgetItem* item = projectTreeWidget_->itemAt(pos)) {
        projectTreeWidget_->setCurrentItem(item);
    }
    updateActionState();

    QTreeWidgetItem* currentItem = projectTreeWidget_->currentItem();
    const QString itemType = projectTreeItemType(currentItem);
    QMenu menu(this);
    menu.setAttribute(Qt::WA_TranslucentBackground, false);
    menu.setWindowOpacity(1.0);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "background-color: #f8fbff;"
        "border: 1px solid #cfd9e6;"
        "border-radius: 10px;"
        "padding: 6px;"
        "color: #0f172a;"
        "}"
        "QMenu::item {"
        "padding: 8px 16px 8px 12px;"
        "border-radius: 7px;"
        "background-color: transparent;"
        "}"
        "QMenu::item:selected {"
        "background-color: #dbeafe;"
        "color: #0f172a;"
        "}"
        "QMenu::separator {"
        "height: 1px;"
        "margin: 6px 8px;"
        "background: #d7e1ee;"
        "}"));
    const QPoint globalPos = projectTreeWidget_->viewport()->mapToGlobal(pos);

    if (itemType == QStringLiteral("pointCloudItem")) {
        QAction* detailsAction = menu.addAction(tr("Point Cloud Details"));
        QAction* focusAction = menu.addAction(tr("Focus in View"));
        menu.addSeparator();
        QAction* openFolderAction = menu.addAction(tr("Open Folder"));
        QAction* copyPathAction = menu.addAction(tr("Copy Path"));
        menu.addSeparator();
        QAction* removeAction = menu.addAction(tr("Remove Selected Dataset"));

        QAction* chosenAction = menu.exec(globalPos);
        const QString filePath = projectTreeItemFilePath(currentItem);
        if (chosenAction == focusAction) {
            focusProjectTreeItem(currentItem);
        } else if (chosenAction == detailsAction) {
            showPointCloudDatasetDetails(filePath);
        } else if (chosenAction == openFolderAction) {
            const QString folderPath = QFileInfo(filePath).absolutePath();
            if (!folderPath.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the selected file folder."), 3000);
            }
        } else if (chosenAction == copyPathAction) {
            if (!filePath.isEmpty() && QGuiApplication::clipboard() != nullptr) {
                QGuiApplication::clipboard()->setText(filePath);
                showUserMessage(LogLevel::Info, tr("Selected path copied."), 2000);
            }
        } else if (chosenAction == removeAction) {
            removeSelectedDataset();
        }
        return;
    }

    if (itemType == QStringLiteral("imageItem")) {
        QAction* focusAction = menu.addAction(tr("Focus in View"));
        QAction* openImageAction = menu.addAction(tr("Open Image"));
        QAction* attachImageAction = menu.addAction(tr("Replace Image"));
        menu.addSeparator();
        QAction* openFolderAction = menu.addAction(tr("Open Folder"));
        QAction* copyPathAction = menu.addAction(tr("Copy Path"));
        QAction* removeImageAction = menu.addAction(tr("Remove Image"));

        QAction* chosenAction = menu.exec(globalPos);
        const QString imagePath = projectTreeItemFilePath(currentItem);
        const int issueIndex = currentItem != nullptr ? currentItem->data(0, kProjectTreeIssueIndexRole).toInt() : -1;
        if (chosenAction == focusAction) {
            focusProjectTreeItem(currentItem);
        } else if (chosenAction == openImageAction) {
            if (!imagePath.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(imagePath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the image file."), 3000);
            }
        } else if (chosenAction == attachImageAction) {
            attachImageToIssue(issueIndex);
        } else if (chosenAction == openFolderAction) {
            const QString folderPath = QFileInfo(imagePath).absolutePath();
            if (!folderPath.isEmpty() && !QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath))) {
                showUserMessage(LogLevel::Warning, tr("Unable to open the selected file folder."), 3000);
            }
        } else if (chosenAction == copyPathAction) {
            if (!imagePath.isEmpty() && QGuiApplication::clipboard() != nullptr) {
                QGuiApplication::clipboard()->setText(imagePath);
                showUserMessage(LogLevel::Info, tr("Selected path copied."), 2000);
            }
        } else if (chosenAction == removeImageAction) {
            QList<InspectionIssue> issues = viewer_->inspectionIssues();
            if (issueIndex >= 0 && issueIndex < issues.size()) {
                issues[issueIndex].imagePath.clear();
                viewer_->setInspectionIssues(issues);
                viewer_->setSelectedIssueIndex(issueIndex);
                updateIssuePanel();
                rebuildProjectTree();
                updateActionState();
                showUserMessage(LogLevel::Info, tr("Image attachment removed."), 2500);
            }
        }
        return;
    }

    if (itemType == QStringLiteral("trajectoryItem")) {
        QAction* focusAction = menu.addAction(tr("Focus in View"));
        QAction* detailsAction = menu.addAction(tr("Trajectory Details"));
        menu.addSeparator();
        QAction* hideAction = menu.addAction(tr("Hide"));
        QAction* clearAction = menu.addAction(tr("Remove Trajectory"));

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == focusAction) {
            focusProjectTreeItem(currentItem);
        } else if (chosenAction == detailsAction) {
            showInspectionRouteDetails();
        } else if (chosenAction == hideAction) {
            viewer_->setInspectionRouteVisible(false);
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        } else if (chosenAction == clearAction) {
            currentPowerlineRoute_ = PowerlineRouteDocument();
            linkedRouteFilePath_.clear();
            selectedRouteWaypointIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        }
        return;
    }

    if (itemType == QStringLiteral("pointCloudGroup")) {
        QAction* addAction = menu.addAction(tr("Add LAS/LAZ Files"));
        menu.addSeparator();
        QAction* showAllAction = menu.addAction(tr("Show All"));
        QAction* hideAllAction = menu.addAction(tr("Hide All"));
        menu.addSeparator();
        QAction* removeAllAction = menu.addAction(tr("Remove All Point Clouds"));
        removeAllAction->setEnabled(viewer_ != nullptr && viewer_->hasLoadedPointClouds());

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == addAction) {
            addPointCloudFiles();
        } else if (chosenAction == showAllAction) {
            setProjectTreeGroupVisibility(itemType, true);
        } else if (chosenAction == hideAllAction) {
            setProjectTreeGroupVisibility(itemType, false);
        } else if (chosenAction == removeAllAction) {
            clearPointCloud();
        }
        return;
    }

    if (itemType == QStringLiteral("imageGroup")) {
        QAction* markIssueAction = menu.addAction(tr("Mark Issue"));
        QAction* attachImageAction = menu.addAction(tr("Attach Image To Selected Issue"));
        attachImageAction->setEnabled(viewer_ != nullptr && viewer_->selectedIssueIndex() >= 0);
        menu.addSeparator();
        QAction* showAllAction = menu.addAction(tr("Show All"));
        QAction* hideAllAction = menu.addAction(tr("Hide All"));
        menu.addSeparator();
        QAction* removeAllAction = menu.addAction(tr("Remove All Images"));
        removeAllAction->setEnabled(!DataManager::instance().imageItems().isEmpty());

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == markIssueAction) {
            startIssueMarkAction_->trigger();
        } else if (chosenAction == attachImageAction) {
            attachImageToIssue(viewer_ != nullptr ? viewer_->selectedIssueIndex() : -1);
        } else if (chosenAction == showAllAction) {
            setProjectTreeGroupVisibility(itemType, true);
        } else if (chosenAction == hideAllAction) {
            setProjectTreeGroupVisibility(itemType, false);
        } else if (chosenAction == removeAllAction) {
            clearAllProjectImages();
        }
        return;
    }

    if (itemType == QStringLiteral("trajectoryGroup")) {
        QAction* importJsonAction = menu.addAction(tr("Import Route File"));
        QAction* importAction = menu.addAction(tr("Import Route KML"));
        menu.addSeparator();
        QAction* showAllAction = menu.addAction(tr("Show All"));
        QAction* hideAllAction = menu.addAction(tr("Hide All"));
        showAllAction->setEnabled(DataManager::instance().hasTrajectory());
        hideAllAction->setEnabled(DataManager::instance().hasTrajectory());
        menu.addSeparator();
        QAction* removeAction = menu.addAction(tr("Remove Trajectory"));
        removeAction->setEnabled(DataManager::instance().hasTrajectory());

        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == importJsonAction) {
            importRouteFileAction_->trigger();
        } else if (chosenAction == importAction) {
            importRouteKmlAction_->trigger();
        } else if (chosenAction == showAllAction) {
            setProjectTreeGroupVisibility(itemType, true);
        } else if (chosenAction == hideAllAction) {
            setProjectTreeGroupVisibility(itemType, false);
        } else if (chosenAction == removeAction) {
            currentPowerlineRoute_ = PowerlineRouteDocument();
            linkedRouteFilePath_.clear();
            selectedRouteWaypointIndex_ = -1;
            viewer_->clearInspectionRouteWaypoints();
            updateRoutePlanningPanel();
            rebuildProjectTree();
            updateActionState();
        }
        return;
    }

    if (itemType == QStringLiteral("projectGroup") || itemType == QStringLiteral("coordinateSystemsItem")) {
        QAction* propertiesAction = menu.addAction(tr("Project Properties"));
        QAction* chosenAction = menu.exec(globalPos);
        if (chosenAction == propertiesAction) {
            openProjectCoordinateSystems();
        }
        return;
    }

    menu.addAction(openAction_);
    menu.addAction(addPointCloudAction_);
    menu.addSeparator();
    menu.addAction(expandProjectTreeAction_);
    menu.addAction(collapseProjectTreeAction_);
    menu.exec(globalPos);
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

void MainWindow::syncDataManagerTrajectory() const
{
    const QList<PointRecord> routePoints = toRouteDisplayPoints(currentPowerlineRoute_);

    DataManager::instance().setTrajectory(
        currentPowerlineRoute_.taskName.trimmed().isEmpty() ? tr("Inspection Route") : currentPowerlineRoute_.taskName.trimmed(),
        routePoints,
        viewer_ != nullptr ? viewer_->inspectionRouteVisible() : true);
}

void MainWindow::updateTowerPanel()
{
    if (viewer_ == nullptr || towerTableWidget_ == nullptr || towerCountValueLabel_ == nullptr || towerToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<TowerMarker>& towerMarkers = viewer_->towerMarkers();
    towerCountValueLabel_->setText(
        towerMarkers.isEmpty()
            ? tr("No tower markers yet. Use the icon tools above to add one from the point cloud.")
            : tr("%1 tower marker(s)").arg(QLocale().toString(towerMarkers.size())));

    switch (viewer_->towerEditMode()) {
    case TowerEditMode::AddAfterLast:
        towerToolStatusLabel_->setText(tr("Tower tool: click points in the view continuously to add tower markers. Cancel the tool when finished."));
        break;
    case TowerEditMode::InsertBeforeSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to insert a tower marker before current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::MoveSelected:
        towerToolStatusLabel_->setText(
            tr("Tower tool: click a point in the view to move current tower #%1.")
                .arg(QLocale().toString(viewer_->towerEditTargetIndex() + 1)));
        break;
    case TowerEditMode::None:
    default:
        towerToolStatusLabel_->setText(
            towerEditingEnabled_
                ? tr("Tower editing active. Select the current tower, then use the toolbar above to insert, move, rename, focus, or remove it.")
                : tr("Tower editing is off. Use the Ribbon to start editing before changing tower markers."));
        break;
    }

    const int selectedRow = viewer_->selectedTowerIndex();
    const QSignalBlocker blocker(towerTableWidget_);
    towerTableWidget_->setEditTriggers(static_cast<QAbstractItemView::EditTriggers>(
        towerEditingEnabled_
            ? (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed)
            : QAbstractItemView::NoEditTriggers));
    towerTableWidget_->setRowCount(0);
    for (int towerIndex = 0; towerIndex < towerMarkers.size(); ++towerIndex) {
        const TowerMarker& towerMarker = towerMarkers.at(towerIndex);
        towerTableWidget_->insertRow(towerIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(towerMarker.index));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        towerTableWidget_->setItem(towerIndex, 0, indexItem);

        auto* nameItem = new QTableWidgetItem(towerMarker.name);
        if (towerEditingEnabled_) {
            nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
        } else {
            nameItem->setFlags((nameItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        }
        towerTableWidget_->setItem(towerIndex, 1, nameItem);

        const QStringList coordinateTexts = {
            formatCoordinate(towerMarker.point.x),
            formatCoordinate(towerMarker.point.y),
            formatCoordinate(towerMarker.point.z)
        };
        for (int coordinateColumn = 0; coordinateColumn < coordinateTexts.size(); ++coordinateColumn) {
            auto* coordinateItem = new QTableWidgetItem(coordinateTexts.at(coordinateColumn));
            coordinateItem->setFlags((coordinateItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            coordinateItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            towerTableWidget_->setItem(towerIndex, coordinateColumn + 2, coordinateItem);
        }
    }

    if (towerTableWidget_->rowCount() > 0) {
        towerTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, towerTableWidget_->rowCount() - 1), 1);
    } else {
        towerTableWidget_->clearSelection();
    }

    updateTowerDetailEditor();
}

QString MainWindow::nextDefaultTowerName() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingNames;
    for (const TowerMarker& towerMarker : viewer_->towerMarkers()) {
        existingNames.insert(towerMarker.name.trimmed());
    }

    int towerIndex = 1;
    while (true) {
        const QString candidate = tr("Tower %1").arg(QLocale().toString(towerIndex));
        if (existingNames.find(candidate) == existingNames.end()) {
            return candidate;
        }
        ++towerIndex;
    }
}

QString MainWindow::nextDefaultIssueTitle() const
{
    if (viewer_ == nullptr) {
        return QString();
    }

    std::set<QString> existingTitles;
    for (const InspectionIssue& issue : viewer_->inspectionIssues()) {
        existingTitles.insert(issue.title.trimmed());
    }

    int issueIndex = 1;
    while (true) {
        const QString candidate = tr("Issue %1").arg(QLocale().toString(issueIndex));
        if (existingTitles.find(candidate) == existingTitles.end()) {
            return candidate;
        }
        ++issueIndex;
    }
}

void MainWindow::updateTowerDetailEditor()
{
    if (viewer_ == nullptr || towerCodeEdit_ == nullptr || towerTypeComboBox_ == nullptr) {
        return;
    }

    updatingTowerDetails_ = true;
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedTowerIndex = viewer_->selectedTowerIndex();
    const bool hasSelection = selectedTowerIndex >= 0 && selectedTowerIndex < towers.size();

    const TowerRecord towerRecord = hasSelection ? towers.at(selectedTowerIndex) : TowerRecord();
    towerCodeEdit_->setText(towerRecord.code);
    towerLineNameEdit_->setText(towerRecord.lineName);
    towerVoltageLevelEdit_->setText(towerRecord.voltageLevel);
    const int towerTypeOption = towerTypeComboBox_->findData(static_cast<int>(towerRecord.towerType));
    towerTypeComboBox_->setCurrentIndex(towerTypeOption >= 0 ? towerTypeOption : 0);
    towerStructureTypeEdit_->setText(towerRecord.structureType);
    towerInspectionDateEdit_->setText(towerRecord.inspectionDate);
    towerStatusEdit_->setText(towerRecord.status);
    towerNotesEdit_->setPlainText(towerRecord.notes);

    const QList<QWidget*> editors = {
        towerCodeEdit_,
        towerLineNameEdit_,
        towerVoltageLevelEdit_,
        towerTypeComboBox_,
        towerStructureTypeEdit_,
        towerInspectionDateEdit_,
        towerStatusEdit_,
        towerNotesEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection && towerEditingEnabled_);
        }
    }
    updatingTowerDetails_ = false;
}

void MainWindow::updateIssuePanel()
{
    if (viewer_ == nullptr || issueTableWidget_ == nullptr || issueCountValueLabel_ == nullptr || issueToolStatusLabel_ == nullptr) {
        return;
    }

    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    issueCountValueLabel_->setText(
        issues.isEmpty()
            ? tr("No inspection issues yet. Use the toolbar above to mark one from the point cloud.")
            : tr("%1 inspection issue(s)").arg(QLocale().toString(issues.size())));
    issueToolStatusLabel_->setText(
        viewer_->issueEditMode() == IssueEditMode::Add
            ? tr("Issue tool: click points in the view continuously to add inspection issues. Right-click to cancel the tool.")
            : tr("Select an issue to edit its business details, focus the scene, or export reports."));

    const int selectedRow = viewer_->selectedIssueIndex();
    const QSignalBlocker blocker(issueTableWidget_);
    issueTableWidget_->setRowCount(0);
    for (int issueIndex = 0; issueIndex < issues.size(); ++issueIndex) {
        const InspectionIssue& issue = issues.at(issueIndex);
        issueTableWidget_->insertRow(issueIndex);

        auto* indexItem = new QTableWidgetItem(QLocale().toString(issueIndex + 1));
        indexItem->setFlags((indexItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        issueTableWidget_->setItem(issueIndex, 0, indexItem);

        auto* titleItem = new QTableWidgetItem(issue.title);
        titleItem->setFlags((titleItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
        issueTableWidget_->setItem(issueIndex, 1, titleItem);

        const QStringList rowValues = {
            issueSeverityDisplayName(issue.severity),
            issueStatusDisplayName(issue.status),
            issue.relatedTowerName,
            issue.category
        };
        for (int valueIndex = 0; valueIndex < rowValues.size(); ++valueIndex) {
            auto* valueItem = new QTableWidgetItem(rowValues.at(valueIndex));
            valueItem->setFlags((valueItem->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled) & ~Qt::ItemIsEditable);
            issueTableWidget_->setItem(issueIndex, valueIndex + 2, valueItem);
        }
    }

    if (issueTableWidget_->rowCount() > 0 && selectedRow >= 0) {
        issueTableWidget_->setCurrentCell(std::clamp(selectedRow, 0, issueTableWidget_->rowCount() - 1), 1);
    } else {
        issueTableWidget_->clearSelection();
    }

    updateIssueDetailEditor();
}

void MainWindow::updateIssueDetailEditor()
{
    if (viewer_ == nullptr || issueTitleEdit_ == nullptr) {
        return;
    }

    updatingIssueDetails_ = true;
    const QList<InspectionIssue>& issues = viewer_->inspectionIssues();
    const QList<TowerRecord>& towers = viewer_->towerMarkers();
    const int selectedIssueIndex = viewer_->selectedIssueIndex();
    const bool hasSelection = selectedIssueIndex >= 0 && selectedIssueIndex < issues.size();
    const InspectionIssue issue = hasSelection ? issues.at(selectedIssueIndex) : InspectionIssue();

    issueTitleEdit_->setText(issue.title);
    issueCategoryComboBox_->setEditText(issue.category);
    issueSeverityComboBox_->setCurrentIndex(static_cast<int>(issue.severity));
    issueStatusComboBox_->setCurrentIndex(static_cast<int>(issue.status));
    issueImagePathEdit_->setText(issue.imagePath);
    issueDescriptionEdit_->setPlainText(issue.description);
    issueLocationValueLabel_->setText(
        hasSelection ? formatTriplet(issue.point.x, issue.point.y, issue.point.z) : tr("N/A"));
    issueCreatedAtValueLabel_->setText(hasSelection ? issue.createdAt : tr("N/A"));

    issueRelatedTowerComboBox_->clear();
    issueRelatedTowerComboBox_->addItem(tr("None"), -1);
    for (int towerIndex = 0; towerIndex < towers.size(); ++towerIndex) {
        issueRelatedTowerComboBox_->addItem(towers.at(towerIndex).name, towerIndex);
    }
    const int relatedTowerOption = issueRelatedTowerComboBox_->findData(issue.relatedTowerIndex);
    issueRelatedTowerComboBox_->setCurrentIndex(relatedTowerOption >= 0 ? relatedTowerOption : 0);

    const QList<QWidget*> editors = {
        issueTitleEdit_,
        issueCategoryComboBox_,
        issueSeverityComboBox_,
        issueStatusComboBox_,
        issueRelatedTowerComboBox_,
        issueImagePathEdit_,
        issueDescriptionEdit_
    };
    for (QWidget* editor : editors) {
        if (editor != nullptr) {
            editor->setEnabled(hasSelection);
        }
    }
    updatingIssueDetails_ = false;
}

void MainWindow::setTowerEditingEnabled(bool enabled)
{
    if (towerEditingEnabled_ == enabled) {
        return;
    }

    towerEditingEnabled_ = enabled;
    if (!towerEditingEnabled_ && viewer_ != nullptr) {
        viewer_->cancelTowerEditMode();
    }

    updateActionState();
    updateTowerPanel();
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



