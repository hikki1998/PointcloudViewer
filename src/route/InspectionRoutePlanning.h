#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "domain/AnalysisData.h"
#include "domain/InspectionData.h"
#include "domain/ProjectMetadata.h"
#include "pointcloud/PointCloudData.h"
#include "route/PowerlineRouteTypes.h"

enum class DjiAircraftProfile
{
    M30Series = 0,
    M3ESeries,
    M300Series
};

struct DjiAircraftProfileMapping
{
    int droneEnumValue = 0;
    int droneSubEnumValue = 0;
    int payloadEnumValue = 0;
    int payloadPositionIndex = 0;
};

struct InspectionWaypoint
{
    QString id;
    PointRecord localPoint;
    double longitude = 0.0;
    double latitude = 0.0;
    float altitude = 0.0f;
    float speedMps = 6.0f;
    float yawDeg = 0.0f;
    float gimbalPitchDeg = -45.0f;
    int sourceRiskIndex = -1;
    float chainage = 0.0f;
};

struct InspectionRoute
{
    QString name;
    QString source;
    QList<InspectionWaypoint> waypoints;
    QDateTime generatedAtUtc;
};

struct RouteGenerationOptions
{
    float waypointSpacingMeters = 30.0f;
    float smoothingStrengthPercent = 0.0f;
};

struct RouteSafetyOptions
{
    float safetyHeightMeters = 30.0f;
    float heightOffsetMeters = 15.0f;
    float defaultWaypointSpeedMps = 6.0f;
    float globalTransitionalSpeedMps = 8.0f;
    float globalRthHeightMeters = 60.0f;
    float defaultGimbalPitchDeg = -45.0f;
};

struct CrsTransformOptions
{
    int sourceEpsg = 0;
    int targetEpsg = 4326;
};

struct RoutePlanningOptions
{
    RouteGenerationOptions generation;
    RouteSafetyOptions safety;
    CrsTransformOptions crs;
    DjiAircraftProfile aircraftProfile = DjiAircraftProfile::M30Series;
};

enum class RouteQaSeverity
{
    Info = 0,
    Warning,
    Blocking
};

enum class RouteQaIssueType
{
    WaypointCountInsufficient = 0,
    WaypointSpacingTooSmall,
    WaypointSpacingTooLarge,
    TargetDistanceTooNear,
    TargetDistanceTooFar,
    AttitudeJumpTooLarge,
    HelperWaypointMissing,
    MissingPartCoverage,
    DuplicatePartCoverage,
    UnsupportedActionCombination
};

struct RouteQaIssue
{
    RouteQaSeverity severity = RouteQaSeverity::Info;
    RouteQaIssueType type = RouteQaIssueType::WaypointSpacingTooSmall;
    int waypointIndex = -1;
    int relatedWaypointIndex = -1;
    int partIndex = -1;
    int targetIndex = -1;
    QString message;
    QString detail;
};

struct RouteQaThresholds
{
    double minWaypointSpacingMeters = 4.0;
    double maxWaypointSpacingMeters = 80.0;
    double minTargetDistanceMeters = 2.0;
    double maxTargetDistanceMeters = 120.0;
    double maxYawDeltaDeg = 85.0;
    double maxGimbalPitchDeltaDeg = 50.0;
    double maxCameraYawDeltaDeg = 85.0;
    double maxCameraPitchDeltaDeg = 50.0;
    double helperWaypointYawThresholdDeg = 45.0;
};

struct RouteQaReport
{
    QList<RouteQaIssue> issues;
    int infoIssueCount = 0;
    int warningIssueCount = 0;
    int blockingIssueCount = 0;

    bool hasBlockingIssues() const;
    bool hasWarnings() const;
};

QString djiAircraftProfileDisplayName(DjiAircraftProfile profile);
DjiAircraftProfileMapping djiAircraftProfileMapping(DjiAircraftProfile profile);
QList<DjiAircraftProfile> supportedDjiAircraftProfiles();

RouteQaThresholds defaultRouteQaThresholds();
QString routeQaSeverityDisplayName(RouteQaSeverity severity);
QString routeQaIssueTypeDisplayName(RouteQaIssueType issueType);
RouteQaReport evaluatePowerlineRouteQa(
    const PowerlineRouteDocument& route,
    DjiAircraftProfile aircraftProfile,
    const RouteQaThresholds& thresholds = defaultRouteQaThresholds());

InspectionRoute generateInspectionRouteFromRisks(
    const QList<VegetationRiskRecord>& risks,
    const QList<TowerRecord>& towers,
    const RouteGenerationOptions& generationOptions,
    const RouteSafetyOptions& safetyOptions);

bool transformRouteToWgs84(
    const InspectionRoute& localRoute,
    const ProjectCoordinateSystems& coordinateSystems,
    InspectionRoute* outputRouteWgs84,
    QString* errorMessage = nullptr);

bool transformRouteFromWgs84(
    const InspectionRoute& routeWgs84,
    const ProjectCoordinateSystems& coordinateSystems,
    InspectionRoute* outputLocalRoute,
    QString* errorMessage = nullptr);

QJsonObject inspectionWaypointToJson(const InspectionWaypoint& waypoint);
InspectionWaypoint inspectionWaypointFromJson(const QJsonObject& object);

QJsonObject inspectionRouteToJson(const InspectionRoute& route);
InspectionRoute inspectionRouteFromJson(const QJsonObject& object);

QJsonObject routePlanningOptionsToJson(const RoutePlanningOptions& options);
RoutePlanningOptions routePlanningOptionsFromJson(const QJsonObject& object);
