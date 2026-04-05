#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>

#include <optional>

#include "pointcloud/PointCloudData.h"

struct RoutePartPoint
{
    int partIndex = -1;
    int fileId = -1;
    QString partName;
    QString hardwareType;
    QString circuitType;
    QString phaseSequence;
    QString partitionName;
    QString towerSide;
    bool isUsed = true;
    int towerIndex = -1;
    double longitude = 0.0;
    double latitude = 0.0;
    double dh = 0.0;
    PointRecord localPoint;
    int cameraAngle = 0;
    int cameraDistance = 0;
    int cameraOrder = 0;
    int captureCount = 0;
    double sceneDirX = 0.0;
    double sceneDirY = 0.0;
    double sceneDirZ = 0.0;
    QJsonObject extraFields;
};

struct RouteCaptureTarget
{
    int partIndex = -1;
    int partFileId = -1;
    QString partName;
    int cameraType = 0;
    double focalLengthRatio = 1.0;
    int captureCount = 0;
    double aircraftYawDeg = 0.0;
    double gimbalPitchDeg = 0.0;
    double cameraYawDeg = 0.0;
    double cameraPitchDeg = 0.0;
    PointRecord targetLocalPoint;
    QJsonObject extraTopShotFields;
    QJsonObject extraLocalShotFields;
};

struct RouteWaypoint
{
    int sequenceIndex = -1;
    int primaryPartIndex = -1;
    int rawKeyId = -1;
    bool isHelperWaypoint = false;
    QString towerName;
    int towerIndex = -1;
    QString phaseSequence;
    QString mainWaypointType;
    bool isStart = false;
    bool isEmergencyReturnPoint = false;
    int turnMode = 0;
    double waypointSpeed = 0.0;
    double cornerRadiusMeters = 0.0;
    double longitude = 0.0;
    double latitude = 0.0;
    double dh = 0.0;
    double height = 0.0;
    double aircraftYawDeg = 0.0;
    double gimbalPitchDeg = 0.0;
    PointRecord localPoint;
    std::optional<PointRecord> rotationCenter;
    QList<RouteCaptureTarget> captureTargets;
    QJsonObject extraTopPointFields;
    QJsonObject extraLocalWaypointFields;
};

struct PowerlineRouteDocument
{
    QString taskName;
    QDateTime createdAt;
    QDateTime updatedAt;
    QString towerType;
    QList<RoutePartPoint> partPoints;
    QList<RouteWaypoint> waypoints;
    QJsonArray boundaryPointsRaw;
    QJsonValue safeBoxRaw;
    QJsonValue originSafeDistanceRaw;
    QJsonObject missionSettings;
    QJsonObject extraRootFields;
    QJsonObject extraPowerlineFields;
};
