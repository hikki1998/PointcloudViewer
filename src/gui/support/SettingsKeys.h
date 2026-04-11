#pragma once

namespace lasviewer::gui::settingskeys
{
inline constexpr char kProjectRecentProjects[] = "project/recentProjects";
inline constexpr char kProjectLastOpenedProject[] = "project/lastOpenedProject";

inline constexpr char kInteractionInvertOrbitDrag[] = "interaction/invertOrbitDrag";
inline constexpr char kInteractionInvertPanDrag[] = "interaction/invertPanDrag";
inline constexpr char kInteractionInvertWheelZoom[] = "interaction/invertWheelZoom";
inline constexpr char kInteractionWheelZoomSensitivityPercent[] = "interaction/wheelZoomSensitivityPercent";

inline constexpr char kMeasurementClearanceThresholdMeters[] = "measurement/clearanceThresholdMeters";
inline constexpr char kMeasurementClearanceRulePreset[] = "measurement/clearanceRulePreset";
inline constexpr char kMeasurementVegetationSearchRadiusMeters[] = "measurement/vegetationSearchRadiusMeters";
inline constexpr char kMeasurementVegetationClusterGapMeters[] = "measurement/vegetationClusterGapMeters";
inline constexpr char kMeasurementVegetationClusterPointCount[] = "measurement/vegetationClusterPointCount";
inline constexpr char kMeasurementPreferVegetationClassification[] = "measurement/preferVegetationClassification";
inline constexpr char kMeasurementProfileClassificationSelectionMode[] = "measurement/profileClassificationSelectionMode";

inline constexpr char kVisualizationPointSize[] = "visualization/pointSize";
inline constexpr char kVisualizationPointOpacity[] = "visualization/pointOpacity";
inline constexpr char kVisualizationDepthCueStrength[] = "visualization/depthCueStrength";
inline constexpr char kVisualizationEdlStrength[] = "visualization/edlStrength";
inline constexpr char kVisualizationColorMode[] = "visualization/colorMode";
inline constexpr char kVisualizationSingleColor[] = "visualization/singleColor";
inline constexpr char kVisualizationClassificationColorsJson[] = "visualization/classificationColorsJson";
inline constexpr char kVisualizationClassificationVisibilityJson[] = "visualization/classificationVisibilityJson";
inline constexpr char kVisualizationClassificationNameOverridesJson[] = "visualization/classificationNameOverridesJson";
inline constexpr char kVisualizationClassificationFallbackColor[] = "visualization/classificationFallbackColor";
inline constexpr char kVisualizationBackgroundColor[] = "visualization/backgroundColor";
inline constexpr char kVisualizationRouteWaypointColor[] = "visualization/routeWaypointColor";
inline constexpr char kVisualizationRoutePartPointColor[] = "visualization/routePartPointColor";
inline constexpr char kVisualizationRouteTrajectoryColor[] = "visualization/routeTrajectoryColor";
inline constexpr char kVisualizationUseRoundSplats[] = "visualization/useRoundSplats";
inline constexpr char kVisualizationShowAxes[] = "visualization/showAxes";
inline constexpr char kVisualizationShowBoundingBox[] = "visualization/showBoundingBox";

inline constexpr char kUiLanguage[] = "ui/language";
inline constexpr char kUiTheme[] = "ui/theme";

inline constexpr char kWindowGeometry[] = "window/geometry";
inline constexpr char kWindowState[] = "window/state";
inline constexpr char kWindowMaximized[] = "window/maximized";
inline constexpr char kWindowShowLog[] = "window/showLog";
inline constexpr char kWindowShowProfile[] = "window/showProfile";
inline constexpr char kWindowShowProfileClassification[] = "window/showProfileClassification";
inline constexpr char kWindowShowRouteDetails[] = "window/showRouteDetails";
inline constexpr char kWindowInspectorTab[] = "window/inspectorTab";
inline constexpr char kWindowRouteDetailsTab[] = "window/routeDetailsTab";
inline constexpr char kWindowRouteWaypointLabelMode[] = "window/routeWaypointLabelMode";
inline constexpr char kWindowRoutePartLabelMode[] = "window/routePartLabelMode";
inline constexpr char kWindowRouteWaypointShowCoordinates[] = "window/routeWaypointShowCoordinates";
inline constexpr char kWindowRouteWaypointShowCaptureAngles[] = "window/routeWaypointShowCaptureAngles";
inline constexpr char kWindowRoutePartShowCoordinates[] = "window/routePartShowCoordinates";
inline constexpr char kWindowRoutePartShowCaptureAngles[] = "window/routePartShowCaptureAngles";
inline constexpr char kWindowLogFilterLevel[] = "window/logFilterLevel";
inline constexpr char kWindowLogSearchKeyword[] = "window/logSearchKeyword";
inline constexpr char kWindowLogAutoScroll[] = "window/logAutoScroll";

inline constexpr char kRouteEditingEnabled[] = "route/editingEnabled";
inline constexpr char kRoutePlanningAircraftProfile[] = "route/planning/aircraftProfile";
inline constexpr char kRoutePlanningSafetyHeightMeters[] = "route/planning/safetyHeightMeters";
inline constexpr char kRoutePlanningWaypointSpeedMps[] = "route/planning/waypointSpeedMps";
inline constexpr char kRoutePlanningWaypointSpacingMeters[] = "route/planning/waypointSpacingMeters";
inline constexpr char kRoutePlanningSmoothingStrengthPercent[] = "route/planning/smoothingStrengthPercent";
inline constexpr char kRoutePlanningHeightOffsetMeters[] = "route/planning/heightOffsetMeters";
inline constexpr char kRouteRoamSpeedMps[] = "route/roam/speedMps";
inline constexpr char kRouteRoamViewMode[] = "route/roam/viewMode";
inline constexpr char kRouteDisplayWaypointLabelMode[] = "route/display/waypointLabelMode";
inline constexpr char kRouteDisplayPartLabelMode[] = "route/display/partLabelMode";
inline constexpr char kRouteDisplayWaypointShowCoordinates[] = "route/display/waypointShowCoordinates";
inline constexpr char kRouteDisplayWaypointShowCaptureAngles[] = "route/display/waypointShowCaptureAngles";
inline constexpr char kRouteDisplayPartShowCoordinates[] = "route/display/partShowCoordinates";
inline constexpr char kRouteDisplayPartShowCaptureAngles[] = "route/display/partShowCaptureAngles";
}
