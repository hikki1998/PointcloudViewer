#include "gui/MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>

#include "gui/ApplicationLogDock.h"
#include "gui/MainWindowInternal.h"
#include "gui/PointCloudViewer.h"
#include "gui/ProfileClassificationDock.h"
#include "gui/ProjectExplorerDock.h"
#include "gui/RouteDetailsDock.h"
#include "gui/SpanProfileDock.h"
#include "gui/UiHistoryStore.h"
#include "gui/support/SettingsKeys.h"

using namespace mainwindow_internal;
namespace settingskeys = lasviewer::gui::settingskeys;

namespace
{
constexpr int kMainWindowStateVersion = 1;
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