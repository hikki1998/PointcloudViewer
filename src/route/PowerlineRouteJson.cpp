#include "route/PowerlineRouteJson.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <algorithm>

namespace
{
const QString kTaskNameKey = QStringLiteral("taskname");
const QString kCreatedAtKey = QStringLiteral("date");
const QString kUpdatedAtKey = QStringLiteral("updateTime");
const QString kPointsKey = QStringLiteral("points");
const QString kPowerlineKey = QStringLiteral("powerline");
const QString kPowerlineBoundaryKey = QStringLiteral("boundaryPts");
const QString kPowerlineSafeBoxKey = QStringLiteral("safeBox");
const QString kPowerlineOriginSafeDistanceKey = QStringLiteral("originSafeDis");
const QString kPowerlineTowerTypeKey = QStringLiteral("towerType");
const QString kPowerlineKeyPointKey = QStringLiteral("keyPoint");
const QString kPowerlineWaypointKey = QStringLiteral("waypoint");

const QSet<QString> kKnownMissionKeys = {
    QStringLiteral("areaSize"),
    QStringLiteral("autoflightLength"),
    QStringLiteral("autoflightPointsCount"),
    QStringLiteral("cameraDisplayName"),
    QStringLiteral("cameraHeading"),
    QStringLiteral("cameraMode"),
    QStringLiteral("cornerRadiusInMeters"),
    QStringLiteral("elecFenceBox"),
    QStringLiteral("finishedAction"),
    QStringLiteral("flightPathMode"),
    QStringLiteral("gimbalMode"),
    QStringLiteral("gimbalPitchAngle"),
    QStringLiteral("height"),
    QStringLiteral("i_sideCamOverlap"),
    QStringLiteral("i_sideTaskOverlap"),
    QStringLiteral("imgFlightFov"),
    QStringLiteral("imgSideFov"),
    QStringLiteral("isSubstationTrajectory"),
    QStringLiteral("lasorFov"),
    QStringLiteral("mainFlightAngle"),
    QStringLiteral("mainLineCount"),
    QStringLiteral("makeWayType"),
    QStringLiteral("origin"),
    QStringLiteral("overlapRatio"),
    QStringLiteral("sideOverlapRation"),
    QStringLiteral("speed"),
    QStringLiteral("startHeight"),
    QStringLiteral("startPosx"),
    QStringLiteral("startPosy"),
    QStringLiteral("substationVirtualTowerPosX"),
    QStringLiteral("substationVirtualTowerPosY"),
    QStringLiteral("substationVirtualTowerPosZ"),
    QStringLiteral("type"),
    QStringLiteral("uploaded"),
    QStringLiteral("webplannedtimeinterval"),
    QStringLiteral("yawMode")
};

const QSet<QString> kKnownTopPointKeys = {
    QStringLiteral("SIMainWayPointType"),
    QStringLiteral("aircraftYaw"),
    QStringLiteral("cornerRadiusInMeters"),
    QStringLiteral("dh"),
    QStringLiteral("gimbalPitch"),
    QStringLiteral("height"),
    QStringLiteral("isStart"),
    QStringLiteral("keyID"),
    QStringLiteral("lat"),
    QStringLiteral("lng"),
    QStringLiteral("phaseSequence"),
    QStringLiteral("towerName"),
    QStringLiteral("turnMode"),
    QStringLiteral("waypointSpeed"),
    QStringLiteral("yawPitchArray")
};

const QSet<QString> kKnownTopShotKeys = {
    QStringLiteral("FocalLengthRatio"),
    QStringLiteral("aircraftYaw"),
    QStringLiteral("cameraType"),
    QStringLiteral("captureCount"),
    QStringLiteral("gimbalPitch"),
    QStringLiteral("keyID"),
    QStringLiteral("keyName")
};

const QSet<QString> kKnownPartPointKeys = {
    QStringLiteral("ID"),
    QStringLiteral("additionalPartInfo"),
    QStringLiteral("cameraAngle"),
    QStringLiteral("cameraDistance"),
    QStringLiteral("cameraOrder"),
    QStringLiteral("captureCount"),
    QStringLiteral("circuitType"),
    QStringLiteral("curAccountID"),
    QStringLiteral("dh"),
    QStringLiteral("hardwareType"),
    QStringLiteral("index"),
    QStringLiteral("isUsed"),
    QStringLiteral("lat"),
    QStringLiteral("lng"),
    QStringLiteral("pX"),
    QStringLiteral("pY"),
    QStringLiteral("pZ"),
    QStringLiteral("partName"),
    QStringLiteral("partitionName"),
    QStringLiteral("phaseSequence"),
    QStringLiteral("sceneDirX"),
    QStringLiteral("sceneDirY"),
    QStringLiteral("sceneDirZ"),
    QStringLiteral("towerIndex"),
    QStringLiteral("towerSide")
};

const QSet<QString> kKnownLocalWaypointKeys = {
    QStringLiteral("IDFromMainWayPoint"),
    QStringLiteral("SIMainWayPointType"),
    QStringLiteral("isEmergencyReturnPoint"),
    QStringLiteral("keyID"),
    QStringLiteral("order"),
    QStringLiteral("pX"),
    QStringLiteral("pY"),
    QStringLiteral("pZ"),
    QStringLiteral("rotationCenterX"),
    QStringLiteral("rotationCenterY"),
    QStringLiteral("rotationCenterZ"),
    QStringLiteral("towerIndex"),
    QStringLiteral("waypointSpeed"),
    QStringLiteral("yawPitchArray")
};

const QSet<QString> kKnownLocalShotKeys = {
    QStringLiteral("FocalLengthRatio"),
    QStringLiteral("cameraPitch"),
    QStringLiteral("cameraType"),
    QStringLiteral("cameraYaw"),
    QStringLiteral("keyPosX"),
    QStringLiteral("keyPosY"),
    QStringLiteral("keyPosZ"),
    QStringLiteral("photoKeyID"),
    QStringLiteral("photoKeyName")
};

QJsonObject extractExtraFields(const QJsonObject& object, const QSet<QString>& knownKeys)
{
    QJsonObject extraFields = object;
    for (const QString& key : knownKeys) {
        extraFields.remove(key);
    }
    return extraFields;
}

double objectDouble(const QJsonObject& object, const QString& key, double defaultValue = 0.0)
{
    return object.value(key).toDouble(defaultValue);
}

int objectInt(const QJsonObject& object, const QString& key, int defaultValue = 0)
{
    return object.value(key).toInt(defaultValue);
}

bool hasAllRotationFields(const QJsonObject& object)
{
    return object.contains(QStringLiteral("rotationCenterX"))
        && object.contains(QStringLiteral("rotationCenterY"))
        && object.contains(QStringLiteral("rotationCenterZ"));
}

PointRecord pointRecordFromCoordinates(double x, double y, double z)
{
    PointRecord point;
    point.x = x;
    point.y = y;
    point.z = z;
    return point;
}

QString partNameForIndex(const QHash<int, RoutePartPoint>& partPointByIndex, int partIndex)
{
    return partPointByIndex.contains(partIndex)
        ? partPointByIndex.value(partIndex).partName
        : QString();
}

int fileIdForPartIndex(const QHash<int, RoutePartPoint>& partPointByIndex, int partIndex)
{
    return partPointByIndex.contains(partIndex)
        ? partPointByIndex.value(partIndex).fileId
        : -1;
}

bool resolvePartIndexFromFileId(
    int fileId,
    const QHash<int, int>& partIndexByFileId,
    int* resolvedPartIndex,
    QString* errorMessage,
    const QString& contextLabel)
{
    if (resolvedPartIndex == nullptr) {
        return false;
    }

    if (fileId <= 0) {
        *resolvedPartIndex = fileId;
        return true;
    }

    if (!partIndexByFileId.contains(fileId)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "PowerlineRouteJson",
                "%1 references unknown part ID %2.")
                .arg(contextLabel, QString::number(fileId));
        }
        return false;
    }

    *resolvedPartIndex = partIndexByFileId.value(fileId);
    return true;
}

RouteCaptureTarget buildFallbackCaptureTarget(
    const RouteWaypoint& waypoint,
    const QHash<int, RoutePartPoint>& partPointByIndex)
{
    RouteCaptureTarget target;
    target.partIndex = waypoint.primaryPartIndex;
    target.partFileId = waypoint.rawKeyId;
    target.partName = partNameForIndex(partPointByIndex, waypoint.primaryPartIndex);
    target.aircraftYawDeg = waypoint.aircraftYawDeg;
    target.gimbalPitchDeg = waypoint.gimbalPitchDeg;
    if (partPointByIndex.contains(waypoint.primaryPartIndex)) {
        target.targetLocalPoint = partPointByIndex.value(waypoint.primaryPartIndex).localPoint;
    }
    return target;
}

QJsonArray buildTopYawPitchArray(
    const RouteWaypoint& waypoint,
    const QHash<int, RoutePartPoint>& partPointByIndex)
{
    QJsonArray shotArray;
    for (const RouteCaptureTarget& target : waypoint.captureTargets) {
        QJsonObject shotObject = target.extraTopShotFields;
        const int partFileId = target.partFileId > 0
            ? target.partFileId
            : fileIdForPartIndex(partPointByIndex, target.partIndex);
        shotObject.insert(QStringLiteral("FocalLengthRatio"), target.focalLengthRatio);
        shotObject.insert(QStringLiteral("aircraftYaw"), target.aircraftYawDeg);
        shotObject.insert(QStringLiteral("cameraType"), target.cameraType);
        shotObject.insert(QStringLiteral("captureCount"), target.captureCount);
        shotObject.insert(QStringLiteral("gimbalPitch"), target.gimbalPitchDeg);
        shotObject.insert(QStringLiteral("keyID"), partFileId > 0 ? partFileId : waypoint.rawKeyId);
        shotObject.insert(
            QStringLiteral("keyName"),
            target.partName.trimmed().isEmpty()
                ? partNameForIndex(partPointByIndex, target.partIndex)
                : target.partName);
        shotArray.append(shotObject);
    }
    return shotArray;
}

QJsonArray buildLocalYawPitchArray(
    const RouteWaypoint& waypoint,
    const QHash<int, RoutePartPoint>& partPointByIndex)
{
    QJsonArray shotArray;
    for (const RouteCaptureTarget& target : waypoint.captureTargets) {
        QJsonObject shotObject = target.extraLocalShotFields;
        const int partFileId = target.partFileId > 0
            ? target.partFileId
            : fileIdForPartIndex(partPointByIndex, target.partIndex);
        PointRecord targetPoint = target.targetLocalPoint;
        if (targetPoint.x == 0.0f && targetPoint.y == 0.0f && targetPoint.z == 0.0f && partPointByIndex.contains(target.partIndex)) {
            targetPoint = partPointByIndex.value(target.partIndex).localPoint;
        }

        shotObject.insert(QStringLiteral("FocalLengthRatio"), target.focalLengthRatio);
        shotObject.insert(QStringLiteral("cameraPitch"), target.cameraPitchDeg);
        shotObject.insert(QStringLiteral("cameraType"), target.cameraType);
        shotObject.insert(QStringLiteral("cameraYaw"), target.cameraYawDeg);
        shotObject.insert(QStringLiteral("keyPosX"), targetPoint.x);
        shotObject.insert(QStringLiteral("keyPosY"), targetPoint.y);
        shotObject.insert(QStringLiteral("keyPosZ"), targetPoint.z);
        shotObject.insert(QStringLiteral("photoKeyID"), partFileId > 0 ? partFileId : waypoint.rawKeyId);
        shotObject.insert(
            QStringLiteral("photoKeyName"),
            target.partName.trimmed().isEmpty()
                ? partNameForIndex(partPointByIndex, target.partIndex)
                : target.partName);
        shotArray.append(shotObject);
    }
    return shotArray;
}

int exportKeyIdForWaypoint(
    const RouteWaypoint& waypoint,
    const QHash<int, RoutePartPoint>& partPointByIndex)
{
    if (waypoint.primaryPartIndex > 0) {
        const int fileId = fileIdForPartIndex(partPointByIndex, waypoint.primaryPartIndex);
        return fileId > 0 ? fileId : waypoint.rawKeyId;
    }
    if (waypoint.rawKeyId < 0) {
        return waypoint.rawKeyId;
    }
    return -1;
}

QString isoDateOrNow(const QDateTime& value)
{
    return (value.isValid() ? value : QDateTime::currentDateTimeUtc()).toString(Qt::ISODate);
}
}

bool validatePowerlineRoute(
    const PowerlineRouteDocument& route,
    QStringList* errors,
    QStringList* warnings)
{
    if (errors != nullptr) {
        errors->clear();
    }
    if (warnings != nullptr) {
        warnings->clear();
    }

    QSet<int> partIndices;
    QHash<int, RoutePartPoint> partPointByIndex;
    for (const RoutePartPoint& partPoint : route.partPoints) {
        if (partPoint.partIndex <= 0) {
            if (errors != nullptr) {
                errors->append(QCoreApplication::translate(
                    "PowerlineRouteJson",
                    "Part point index must be > 0."));
            }
            continue;
        }
        if (partIndices.contains(partPoint.partIndex)) {
            if (errors != nullptr) {
                errors->append(QCoreApplication::translate(
                    "PowerlineRouteJson",
                    "Duplicate part point index %1.")
                    .arg(QString::number(partPoint.partIndex)));
            }
            continue;
        }
        partIndices.insert(partPoint.partIndex);
        partPointByIndex.insert(partPoint.partIndex, partPoint);
    }

    for (int waypointIndex = 0; waypointIndex < route.waypoints.size(); ++waypointIndex) {
        const RouteWaypoint& waypoint = route.waypoints.at(waypointIndex);
        if (waypoint.primaryPartIndex > 0 && !partPointByIndex.contains(waypoint.primaryPartIndex)) {
            if (errors != nullptr) {
                errors->append(QCoreApplication::translate(
                    "PowerlineRouteJson",
                    "Waypoint %1 references missing primary part index %2.")
                    .arg(QString::number(waypointIndex + 1), QString::number(waypoint.primaryPartIndex)));
            }
        }

        for (const RouteCaptureTarget& target : waypoint.captureTargets) {
            if (target.partIndex > 0 && !partPointByIndex.contains(target.partIndex)) {
                if (errors != nullptr) {
                    errors->append(QCoreApplication::translate(
                        "PowerlineRouteJson",
                        "Waypoint %1 capture target references missing part index %2.")
                        .arg(QString::number(waypointIndex + 1), QString::number(target.partIndex)));
                }
            }
        }

        if (waypoint.isHelperWaypoint && !waypoint.rotationCenter.has_value() && warnings != nullptr) {
            warnings->append(QCoreApplication::translate(
                "PowerlineRouteJson",
                "Helper waypoint %1 has no rotation center.")
                .arg(QString::number(waypointIndex + 1)));
        }
    }

    return errors == nullptr || errors->isEmpty();
}

bool importPowerlineRouteJson(
    const QString& filePath,
    PowerlineRouteDocument* route,
    QString* errorMessage)
{
    if (route == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("PowerlineRouteJson", "Route output pointer is null.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("PowerlineRouteJson", "Failed to open route JSON file.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "PowerlineRouteJson",
                "Route JSON is invalid: %1.")
                .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject rootObject = document.object();
    const QJsonObject powerlineObject = rootObject.value(kPowerlineKey).toObject();
    const QJsonArray topPointArray = rootObject.value(kPointsKey).toArray();
    const QJsonArray localWaypointArray = powerlineObject.value(kPowerlineWaypointKey).toArray();
    if (topPointArray.size() != localWaypointArray.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate(
                "PowerlineRouteJson",
                "Root points count does not match powerline.waypoint count.");
        }
        return false;
    }

    PowerlineRouteDocument importedRoute;
    importedRoute.taskName = rootObject.value(kTaskNameKey).toString().trimmed();
    importedRoute.createdAt = QDateTime::fromString(rootObject.value(kCreatedAtKey).toString().trimmed(), Qt::ISODate);
    importedRoute.updatedAt = QDateTime::fromString(rootObject.value(kUpdatedAtKey).toString().trimmed(), Qt::ISODate);
    importedRoute.towerType = powerlineObject.value(kPowerlineTowerTypeKey).toString().trimmed();
    importedRoute.boundaryPointsRaw = powerlineObject.value(kPowerlineBoundaryKey).toArray();
    importedRoute.safeBoxRaw = powerlineObject.value(kPowerlineSafeBoxKey);
    importedRoute.originSafeDistanceRaw = powerlineObject.value(kPowerlineOriginSafeDistanceKey);
    importedRoute.extraRootFields = extractExtraFields(
        rootObject,
        kKnownMissionKeys + QSet<QString> { kTaskNameKey, kCreatedAtKey, kUpdatedAtKey, kPointsKey, kPowerlineKey });
    importedRoute.extraPowerlineFields = extractExtraFields(
        powerlineObject,
        QSet<QString> {
            kPowerlineBoundaryKey,
            kPowerlineSafeBoxKey,
            kPowerlineOriginSafeDistanceKey,
            kPowerlineTowerTypeKey,
            kPowerlineKeyPointKey,
            kPowerlineWaypointKey
        });

    for (const QString& missionKey : kKnownMissionKeys) {
        if (rootObject.contains(missionKey)) {
            importedRoute.missionSettings.insert(missionKey, rootObject.value(missionKey));
        }
    }

    QHash<int, int> partIndexByFileId;
    QHash<int, RoutePartPoint> partPointByIndex;
    const QJsonArray keyPointArray = powerlineObject.value(kPowerlineKeyPointKey).toArray();
    importedRoute.partPoints.reserve(keyPointArray.size());
    for (const QJsonValue& keyPointValue : keyPointArray) {
        const QJsonObject keyPointObject = keyPointValue.toObject();

        RoutePartPoint partPoint;
        partPoint.fileId = objectInt(keyPointObject, QStringLiteral("ID"), -1);
        partPoint.partIndex = objectInt(keyPointObject, QStringLiteral("index"), -1);
        if (partPoint.partIndex <= 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate(
                    "PowerlineRouteJson",
                    "Part point index must be > 0.");
            }
            return false;
        }
        if (partPointByIndex.contains(partPoint.partIndex)) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate(
                    "PowerlineRouteJson",
                    "Duplicate part point index %1.")
                    .arg(QString::number(partPoint.partIndex));
            }
            return false;
        }
        if (partPoint.fileId > 0 && partIndexByFileId.contains(partPoint.fileId)) {
            if (errorMessage != nullptr) {
                *errorMessage = QCoreApplication::translate(
                    "PowerlineRouteJson",
                    "Duplicate part point file ID %1.")
                    .arg(QString::number(partPoint.fileId));
            }
            return false;
        }

        partPoint.partName = keyPointObject.value(QStringLiteral("partName")).toString().trimmed();
        partPoint.hardwareType = keyPointObject.value(QStringLiteral("hardwareType")).toString().trimmed();
        partPoint.circuitType = keyPointObject.value(QStringLiteral("circuitType")).toString().trimmed();
        partPoint.phaseSequence = keyPointObject.value(QStringLiteral("phaseSequence")).toString().trimmed();
        partPoint.partitionName = keyPointObject.value(QStringLiteral("partitionName")).toString().trimmed();
        partPoint.towerSide = keyPointObject.value(QStringLiteral("towerSide")).toString().trimmed();
        partPoint.isUsed = keyPointObject.value(QStringLiteral("isUsed")).toBool(true);
        partPoint.towerIndex = objectInt(keyPointObject, QStringLiteral("towerIndex"), -1);
        partPoint.longitude = objectDouble(keyPointObject, QStringLiteral("lng"));
        partPoint.latitude = objectDouble(keyPointObject, QStringLiteral("lat"));
        partPoint.dh = objectDouble(keyPointObject, QStringLiteral("dh"));
        partPoint.localPoint = pointRecordFromCoordinates(
            objectDouble(keyPointObject, QStringLiteral("pX")),
            objectDouble(keyPointObject, QStringLiteral("pY")),
            objectDouble(keyPointObject, QStringLiteral("pZ")));
        partPoint.cameraAngle = objectInt(keyPointObject, QStringLiteral("cameraAngle"));
        partPoint.cameraDistance = objectInt(keyPointObject, QStringLiteral("cameraDistance"));
        partPoint.cameraOrder = objectInt(keyPointObject, QStringLiteral("cameraOrder"));
        partPoint.captureCount = objectInt(keyPointObject, QStringLiteral("captureCount"));
        partPoint.sceneDirX = objectDouble(keyPointObject, QStringLiteral("sceneDirX"));
        partPoint.sceneDirY = objectDouble(keyPointObject, QStringLiteral("sceneDirY"));
        partPoint.sceneDirZ = objectDouble(keyPointObject, QStringLiteral("sceneDirZ"));
        partPoint.extraFields = extractExtraFields(keyPointObject, kKnownPartPointKeys);

        importedRoute.partPoints.append(partPoint);
        partPointByIndex.insert(partPoint.partIndex, partPoint);
        if (partPoint.fileId > 0) {
            partIndexByFileId.insert(partPoint.fileId, partPoint.partIndex);
        }
    }

    importedRoute.waypoints.reserve(topPointArray.size());
    for (int waypointIndex = 0; waypointIndex < topPointArray.size(); ++waypointIndex) {
        const QJsonObject topPointObject = topPointArray.at(waypointIndex).toObject();
        const QJsonObject localWaypointObject = localWaypointArray.at(waypointIndex).toObject();

        RouteWaypoint waypoint;
        waypoint.sequenceIndex = waypointIndex;
        waypoint.rawKeyId = objectInt(localWaypointObject, QStringLiteral("keyID"), objectInt(topPointObject, QStringLiteral("keyID"), -1));
        waypoint.towerName = topPointObject.value(QStringLiteral("towerName")).toString().trimmed();
        waypoint.towerIndex = objectInt(localWaypointObject, QStringLiteral("towerIndex"), -1);
        waypoint.phaseSequence = topPointObject.value(QStringLiteral("phaseSequence")).toString().trimmed();
        waypoint.mainWaypointType = localWaypointObject.value(QStringLiteral("SIMainWayPointType")).toString().trimmed();
        if (waypoint.mainWaypointType.isEmpty()) {
            waypoint.mainWaypointType = topPointObject.value(QStringLiteral("SIMainWayPointType")).toString().trimmed();
        }
        waypoint.isStart = topPointObject.value(QStringLiteral("isStart")).toBool(false);
        waypoint.isEmergencyReturnPoint = localWaypointObject.value(QStringLiteral("isEmergencyReturnPoint")).toBool(false);
        waypoint.turnMode = objectInt(topPointObject, QStringLiteral("turnMode"));
        waypoint.waypointSpeed = objectDouble(
            localWaypointObject,
            QStringLiteral("waypointSpeed"),
            objectDouble(topPointObject, QStringLiteral("waypointSpeed")));
        waypoint.cornerRadiusMeters = objectDouble(topPointObject, QStringLiteral("cornerRadiusInMeters"));
        waypoint.longitude = objectDouble(topPointObject, QStringLiteral("lng"));
        waypoint.latitude = objectDouble(topPointObject, QStringLiteral("lat"));
        waypoint.dh = objectDouble(topPointObject, QStringLiteral("dh"), objectDouble(localWaypointObject, QStringLiteral("pZ")));
        waypoint.height = objectDouble(topPointObject, QStringLiteral("height"), waypoint.dh);
        waypoint.aircraftYawDeg = objectDouble(topPointObject, QStringLiteral("aircraftYaw"));
        waypoint.gimbalPitchDeg = objectDouble(topPointObject, QStringLiteral("gimbalPitch"));
        waypoint.localPoint = pointRecordFromCoordinates(
            objectDouble(localWaypointObject, QStringLiteral("pX")),
            objectDouble(localWaypointObject, QStringLiteral("pY")),
            objectDouble(localWaypointObject, QStringLiteral("pZ")));

        if (hasAllRotationFields(localWaypointObject)) {
            waypoint.rotationCenter = pointRecordFromCoordinates(
                objectDouble(localWaypointObject, QStringLiteral("rotationCenterX")),
                objectDouble(localWaypointObject, QStringLiteral("rotationCenterY")),
                objectDouble(localWaypointObject, QStringLiteral("rotationCenterZ")));
        }

        if (!resolvePartIndexFromFileId(
                waypoint.rawKeyId,
                partIndexByFileId,
                &waypoint.primaryPartIndex,
                errorMessage,
                QCoreApplication::translate("PowerlineRouteJson", "Waypoint keyID"))) {
            return false;
        }

        const QJsonArray topShotArray = topPointObject.value(QStringLiteral("yawPitchArray")).toArray();
        const QJsonArray localShotArray = localWaypointObject.value(QStringLiteral("yawPitchArray")).toArray();
        const int captureCount = std::max(topShotArray.size(), localShotArray.size());
        for (int captureIndex = 0; captureIndex < captureCount; ++captureIndex) {
            const QJsonObject topShotObject = captureIndex < topShotArray.size()
                ? topShotArray.at(captureIndex).toObject()
                : QJsonObject();
            const QJsonObject localShotObject = captureIndex < localShotArray.size()
                ? localShotArray.at(captureIndex).toObject()
                : QJsonObject();

            RouteCaptureTarget captureTarget;
            const int topFileId = objectInt(topShotObject, QStringLiteral("keyID"), -1);
            const int localFileId = objectInt(localShotObject, QStringLiteral("photoKeyID"), -1);
            const int captureFileId = localFileId > 0
                ? localFileId
                : (topFileId > 0 ? topFileId : waypoint.rawKeyId);
            if (!resolvePartIndexFromFileId(
                    captureFileId,
                    partIndexByFileId,
                    &captureTarget.partIndex,
                    errorMessage,
                    QCoreApplication::translate("PowerlineRouteJson", "Capture target"))) {
                return false;
            }

            captureTarget.partFileId = captureFileId > 0
                ? captureFileId
                : (captureTarget.partIndex > 0 ? fileIdForPartIndex(partPointByIndex, captureTarget.partIndex) : captureFileId);
            captureTarget.partName = localShotObject.value(QStringLiteral("photoKeyName")).toString().trimmed();
            if (captureTarget.partName.isEmpty()) {
                captureTarget.partName = topShotObject.value(QStringLiteral("keyName")).toString().trimmed();
            }
            if (captureTarget.partName.isEmpty()) {
                captureTarget.partName = partNameForIndex(partPointByIndex, captureTarget.partIndex);
            }
            captureTarget.cameraType = objectInt(localShotObject, QStringLiteral("cameraType"), objectInt(topShotObject, QStringLiteral("cameraType")));
            captureTarget.focalLengthRatio = objectDouble(
                localShotObject,
                QStringLiteral("FocalLengthRatio"),
                objectDouble(topShotObject, QStringLiteral("FocalLengthRatio"), 1.0));
            captureTarget.captureCount = objectInt(
                topShotObject,
                QStringLiteral("captureCount"),
                objectInt(localShotObject, QStringLiteral("captureCount")));
            captureTarget.aircraftYawDeg = objectDouble(topShotObject, QStringLiteral("aircraftYaw"), waypoint.aircraftYawDeg);
            captureTarget.gimbalPitchDeg = objectDouble(topShotObject, QStringLiteral("gimbalPitch"), waypoint.gimbalPitchDeg);
            captureTarget.cameraYawDeg = objectDouble(localShotObject, QStringLiteral("cameraYaw"), captureTarget.aircraftYawDeg);
            captureTarget.cameraPitchDeg = objectDouble(localShotObject, QStringLiteral("cameraPitch"), captureTarget.gimbalPitchDeg);
            captureTarget.targetLocalPoint = pointRecordFromCoordinates(
                objectDouble(localShotObject, QStringLiteral("keyPosX")),
                objectDouble(localShotObject, QStringLiteral("keyPosY")),
                objectDouble(localShotObject, QStringLiteral("keyPosZ")));
            if (captureTarget.targetLocalPoint.x == 0.0f
                && captureTarget.targetLocalPoint.y == 0.0f
                && captureTarget.targetLocalPoint.z == 0.0f
                && partPointByIndex.contains(captureTarget.partIndex)) {
                captureTarget.targetLocalPoint = partPointByIndex.value(captureTarget.partIndex).localPoint;
            }
            captureTarget.extraTopShotFields = extractExtraFields(topShotObject, kKnownTopShotKeys);
            captureTarget.extraLocalShotFields = extractExtraFields(localShotObject, kKnownLocalShotKeys);

            if (captureTarget.partIndex > 0 || !captureTarget.partName.trimmed().isEmpty() || !topShotObject.isEmpty() || !localShotObject.isEmpty()) {
                waypoint.captureTargets.append(captureTarget);
            }
        }

        if (waypoint.captureTargets.isEmpty() && waypoint.rawKeyId > 0) {
            waypoint.captureTargets.append(buildFallbackCaptureTarget(waypoint, partPointByIndex));
        }

        waypoint.isHelperWaypoint = waypoint.rawKeyId < 0 || waypoint.captureTargets.isEmpty();
        waypoint.extraTopPointFields = extractExtraFields(topPointObject, kKnownTopPointKeys);
        waypoint.extraLocalWaypointFields = extractExtraFields(localWaypointObject, kKnownLocalWaypointKeys);
        importedRoute.waypoints.append(waypoint);
    }

    QStringList validationErrors;
    if (!validatePowerlineRoute(importedRoute, &validationErrors, nullptr)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationErrors.join(QLatin1Char('\n'));
        }
        return false;
    }

    *route = importedRoute;
    return true;
}

bool exportPowerlineRouteJson(
    const QString& filePath,
    const PowerlineRouteDocument& route,
    QString* errorMessage)
{
    QStringList validationErrors;
    if (!validatePowerlineRoute(route, &validationErrors, nullptr)) {
        if (errorMessage != nullptr) {
            *errorMessage = validationErrors.join(QLatin1Char('\n'));
        }
        return false;
    }

    QHash<int, RoutePartPoint> partPointByIndex;
    QList<RoutePartPoint> sortedPartPoints = route.partPoints;
    std::sort(sortedPartPoints.begin(), sortedPartPoints.end(), [](const RoutePartPoint& left, const RoutePartPoint& right) {
        return left.partIndex < right.partIndex;
    });
    for (const RoutePartPoint& partPoint : sortedPartPoints) {
        partPointByIndex.insert(partPoint.partIndex, partPoint);
    }

    QJsonArray keyPointArray;
    for (const RoutePartPoint& partPoint : sortedPartPoints) {
        QJsonObject keyPointObject = partPoint.extraFields;
        keyPointObject.insert(QStringLiteral("ID"), partPoint.fileId);
        keyPointObject.insert(QStringLiteral("additionalPartInfo"), QJsonArray());
        keyPointObject.insert(QStringLiteral("cameraAngle"), partPoint.cameraAngle);
        keyPointObject.insert(QStringLiteral("cameraDistance"), partPoint.cameraDistance);
        keyPointObject.insert(QStringLiteral("cameraOrder"), partPoint.cameraOrder);
        keyPointObject.insert(QStringLiteral("captureCount"), partPoint.captureCount);
        keyPointObject.insert(QStringLiteral("circuitType"), partPoint.circuitType);
        keyPointObject.insert(QStringLiteral("curAccountID"), QString());
        keyPointObject.insert(QStringLiteral("dh"), partPoint.dh != 0.0 ? partPoint.dh : partPoint.localPoint.z);
        keyPointObject.insert(QStringLiteral("hardwareType"), partPoint.hardwareType);
        keyPointObject.insert(QStringLiteral("index"), partPoint.partIndex);
        keyPointObject.insert(QStringLiteral("isUsed"), partPoint.isUsed);
        keyPointObject.insert(QStringLiteral("lat"), partPoint.latitude);
        keyPointObject.insert(QStringLiteral("lng"), partPoint.longitude);
        keyPointObject.insert(QStringLiteral("pX"), partPoint.localPoint.x);
        keyPointObject.insert(QStringLiteral("pY"), partPoint.localPoint.y);
        keyPointObject.insert(QStringLiteral("pZ"), partPoint.localPoint.z);
        keyPointObject.insert(QStringLiteral("partName"), partPoint.partName);
        keyPointObject.insert(QStringLiteral("partitionName"), partPoint.partitionName);
        keyPointObject.insert(QStringLiteral("phaseSequence"), partPoint.phaseSequence);
        keyPointObject.insert(QStringLiteral("sceneDirX"), partPoint.sceneDirX);
        keyPointObject.insert(QStringLiteral("sceneDirY"), partPoint.sceneDirY);
        keyPointObject.insert(QStringLiteral("sceneDirZ"), partPoint.sceneDirZ);
        keyPointObject.insert(QStringLiteral("towerIndex"), partPoint.towerIndex);
        keyPointObject.insert(QStringLiteral("towerSide"), partPoint.towerSide);
        keyPointArray.append(keyPointObject);
    }

    QJsonArray topPointArray;
    QJsonArray localWaypointArray;
    for (const RouteWaypoint& waypoint : route.waypoints) {
        const int keyId = exportKeyIdForWaypoint(waypoint, partPointByIndex);

        QJsonObject topPointObject = waypoint.extraTopPointFields;
        topPointObject.insert(QStringLiteral("SIMainWayPointType"), waypoint.mainWaypointType);
        topPointObject.insert(QStringLiteral("aircraftYaw"), waypoint.aircraftYawDeg);
        topPointObject.insert(QStringLiteral("cornerRadiusInMeters"), waypoint.cornerRadiusMeters);
        topPointObject.insert(QStringLiteral("dh"), waypoint.dh != 0.0 ? waypoint.dh : waypoint.localPoint.z);
        topPointObject.insert(QStringLiteral("gimbalPitch"), waypoint.gimbalPitchDeg);
        topPointObject.insert(QStringLiteral("height"), waypoint.height != 0.0 ? waypoint.height : waypoint.localPoint.z);
        topPointObject.insert(QStringLiteral("isStart"), waypoint.isStart);
        topPointObject.insert(QStringLiteral("keyID"), keyId);
        topPointObject.insert(QStringLiteral("lat"), waypoint.latitude);
        topPointObject.insert(QStringLiteral("lng"), waypoint.longitude);
        topPointObject.insert(QStringLiteral("phaseSequence"), waypoint.phaseSequence);
        topPointObject.insert(QStringLiteral("towerName"), waypoint.towerName);
        topPointObject.insert(QStringLiteral("turnMode"), waypoint.turnMode);
        topPointObject.insert(QStringLiteral("waypointSpeed"), waypoint.waypointSpeed);
        topPointObject.insert(
            QStringLiteral("yawPitchArray"),
            waypoint.isHelperWaypoint ? QJsonArray() : buildTopYawPitchArray(waypoint, partPointByIndex));
        topPointArray.append(topPointObject);

        QJsonObject localWaypointObject = waypoint.extraLocalWaypointFields;
        localWaypointObject.insert(
            QStringLiteral("IDFromMainWayPoint"),
            objectInt(waypoint.extraLocalWaypointFields, QStringLiteral("IDFromMainWayPoint"), -1));
        localWaypointObject.insert(QStringLiteral("SIMainWayPointType"), waypoint.mainWaypointType);
        localWaypointObject.insert(QStringLiteral("isEmergencyReturnPoint"), waypoint.isEmergencyReturnPoint);
        localWaypointObject.insert(QStringLiteral("keyID"), keyId);
        localWaypointObject.insert(
            QStringLiteral("order"),
            objectInt(waypoint.extraLocalWaypointFields, QStringLiteral("order"), 3));
        localWaypointObject.insert(QStringLiteral("pX"), waypoint.localPoint.x);
        localWaypointObject.insert(QStringLiteral("pY"), waypoint.localPoint.y);
        localWaypointObject.insert(QStringLiteral("pZ"), waypoint.localPoint.z);
        localWaypointObject.remove(QStringLiteral("rotationCenterX"));
        localWaypointObject.remove(QStringLiteral("rotationCenterY"));
        localWaypointObject.remove(QStringLiteral("rotationCenterZ"));
        if (waypoint.rotationCenter.has_value()) {
            localWaypointObject.insert(QStringLiteral("rotationCenterX"), waypoint.rotationCenter->x);
            localWaypointObject.insert(QStringLiteral("rotationCenterY"), waypoint.rotationCenter->y);
            localWaypointObject.insert(QStringLiteral("rotationCenterZ"), waypoint.rotationCenter->z);
        }
        localWaypointObject.insert(QStringLiteral("towerIndex"), waypoint.towerIndex);
        localWaypointObject.insert(QStringLiteral("waypointSpeed"), waypoint.waypointSpeed);
        localWaypointObject.insert(
            QStringLiteral("yawPitchArray"),
            waypoint.isHelperWaypoint ? QJsonArray() : buildLocalYawPitchArray(waypoint, partPointByIndex));
        localWaypointArray.append(localWaypointObject);
    }

    QJsonObject powerlineObject = route.extraPowerlineFields;
    powerlineObject.insert(kPowerlineBoundaryKey, route.boundaryPointsRaw);
    if (!route.safeBoxRaw.isUndefined()) {
        powerlineObject.insert(kPowerlineSafeBoxKey, route.safeBoxRaw);
    }
    if (!route.originSafeDistanceRaw.isUndefined()) {
        powerlineObject.insert(kPowerlineOriginSafeDistanceKey, route.originSafeDistanceRaw);
    }
    powerlineObject.insert(kPowerlineTowerTypeKey, route.towerType);
    powerlineObject.insert(kPowerlineKeyPointKey, keyPointArray);
    powerlineObject.insert(kPowerlineWaypointKey, localWaypointArray);

    QJsonObject rootObject = route.extraRootFields;
    for (auto it = route.missionSettings.constBegin(); it != route.missionSettings.constEnd(); ++it) {
        rootObject.insert(it.key(), it.value());
    }
    rootObject.insert(kTaskNameKey, route.taskName);
    rootObject.insert(kCreatedAtKey, isoDateOrNow(route.createdAt));
    rootObject.insert(kUpdatedAtKey, isoDateOrNow(route.updatedAt));
    rootObject.insert(kPointsKey, topPointArray);
    rootObject.insert(kPowerlineKey, powerlineObject);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("PowerlineRouteJson", "Failed to create route JSON file.");
        }
        return false;
    }

    const QByteArray json = QJsonDocument(rootObject).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("PowerlineRouteJson", "Failed to write route JSON file.");
        }
        file.close();
        return false;
    }

    file.close();
    return true;
}
