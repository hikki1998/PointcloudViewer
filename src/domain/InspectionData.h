#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include "pointcloud/PointCloudData.h"

enum class IssueSeverity
{
    Info = 0,
    Minor,
    Major,
    Critical
};

enum class IssueStatus
{
    Open = 0,
    Monitoring,
    Resolved
};

enum class TowerType
{
    Unknown = 0,
    Tangent,
    Strain
};

struct TowerRecord
{
    int index = 0;
    QString name;
    PointRecord point;
    TowerType towerType = TowerType::Unknown;
    QString code;
    QString lineName;
    QString voltageLevel;
    QString structureType;
    QString inspectionDate;
    QString status;
    QString notes;
};

using TowerMarker = TowerRecord;

struct InspectionIssue
{
    QString id;
    QString title;
    QString category;
    IssueSeverity severity = IssueSeverity::Major;
    IssueStatus status = IssueStatus::Open;
    PointRecord point;
    int relatedTowerIndex = -1;
    QString relatedTowerName;
    QString imagePath;
    QString description;
    QString createdAt;
};

QString issueSeverityDisplayName(IssueSeverity severity);
QString issueStatusDisplayName(IssueStatus status);
QString towerTypeDisplayName(TowerType towerType);
QString towerTypeToLiTowerString(TowerType towerType);
TowerType towerTypeFromLiTowerString(const QString& value);

QJsonObject towerRecordToJson(const TowerRecord& towerRecord);
TowerRecord towerRecordFromJson(const QJsonObject& object);

QJsonObject inspectionIssueToJson(const InspectionIssue& issue);
InspectionIssue inspectionIssueFromJson(const QJsonObject& object);

QString issueDefaultId();
