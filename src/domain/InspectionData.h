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

struct TowerRecord
{
    QString name;
    PointRecord point;
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

QJsonObject towerRecordToJson(const TowerRecord& towerRecord);
TowerRecord towerRecordFromJson(const QJsonObject& object);

QJsonObject inspectionIssueToJson(const InspectionIssue& issue);
InspectionIssue inspectionIssueFromJson(const QJsonObject& object);

QString issueDefaultId();
