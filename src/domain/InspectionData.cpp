#include "domain/InspectionData.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QUuid>

namespace
{
QString trimOrFallback(const QString& value, const QString& fallback)
{
    const QString trimmedValue = value.trimmed();
    return trimmedValue.isEmpty() ? fallback : trimmedValue;
}
}

QString issueSeverityDisplayName(IssueSeverity severity)
{
    switch (severity) {
    case IssueSeverity::Info:
        return QCoreApplication::translate("InspectionData", "Info");
    case IssueSeverity::Minor:
        return QCoreApplication::translate("InspectionData", "Minor");
    case IssueSeverity::Major:
        return QCoreApplication::translate("InspectionData", "Major");
    case IssueSeverity::Critical:
    default:
        return QCoreApplication::translate("InspectionData", "Critical");
    }
}

QString issueStatusDisplayName(IssueStatus status)
{
    switch (status) {
    case IssueStatus::Monitoring:
        return QCoreApplication::translate("InspectionData", "Monitoring");
    case IssueStatus::Resolved:
        return QCoreApplication::translate("InspectionData", "Resolved");
    case IssueStatus::Open:
    default:
        return QCoreApplication::translate("InspectionData", "Open");
    }
}

QJsonObject towerRecordToJson(const TowerRecord& towerRecord)
{
    return QJsonObject {
        { QStringLiteral("name"), towerRecord.name },
        { QStringLiteral("x"), towerRecord.point.x },
        { QStringLiteral("y"), towerRecord.point.y },
        { QStringLiteral("z"), towerRecord.point.z },
        { QStringLiteral("code"), towerRecord.code },
        { QStringLiteral("lineName"), towerRecord.lineName },
        { QStringLiteral("voltageLevel"), towerRecord.voltageLevel },
        { QStringLiteral("structureType"), towerRecord.structureType },
        { QStringLiteral("inspectionDate"), towerRecord.inspectionDate },
        { QStringLiteral("status"), towerRecord.status },
        { QStringLiteral("notes"), towerRecord.notes }
    };
}

TowerRecord towerRecordFromJson(const QJsonObject& object)
{
    TowerRecord towerRecord;
    towerRecord.name = object.value(QStringLiteral("name")).toString().trimmed();
    towerRecord.point.x = static_cast<float>(object.value(QStringLiteral("x")).toDouble());
    towerRecord.point.y = static_cast<float>(object.value(QStringLiteral("y")).toDouble());
    towerRecord.point.z = static_cast<float>(object.value(QStringLiteral("z")).toDouble());
    towerRecord.code = object.value(QStringLiteral("code")).toString().trimmed();
    towerRecord.lineName = object.value(QStringLiteral("lineName")).toString().trimmed();
    towerRecord.voltageLevel = object.value(QStringLiteral("voltageLevel")).toString().trimmed();
    towerRecord.structureType = object.value(QStringLiteral("structureType")).toString().trimmed();
    towerRecord.inspectionDate = object.value(QStringLiteral("inspectionDate")).toString().trimmed();
    towerRecord.status = object.value(QStringLiteral("status")).toString().trimmed();
    towerRecord.notes = object.value(QStringLiteral("notes")).toString().trimmed();
    return towerRecord;
}

QJsonObject inspectionIssueToJson(const InspectionIssue& issue)
{
    return QJsonObject {
        { QStringLiteral("id"), issue.id },
        { QStringLiteral("title"), issue.title },
        { QStringLiteral("category"), issue.category },
        { QStringLiteral("severity"), static_cast<int>(issue.severity) },
        { QStringLiteral("status"), static_cast<int>(issue.status) },
        { QStringLiteral("x"), issue.point.x },
        { QStringLiteral("y"), issue.point.y },
        { QStringLiteral("z"), issue.point.z },
        { QStringLiteral("relatedTowerIndex"), issue.relatedTowerIndex },
        { QStringLiteral("relatedTowerName"), issue.relatedTowerName },
        { QStringLiteral("imagePath"), issue.imagePath },
        { QStringLiteral("description"), issue.description },
        { QStringLiteral("createdAt"), issue.createdAt }
    };
}

InspectionIssue inspectionIssueFromJson(const QJsonObject& object)
{
    InspectionIssue issue;
    issue.id = trimOrFallback(object.value(QStringLiteral("id")).toString(), issueDefaultId());
    issue.title = object.value(QStringLiteral("title")).toString().trimmed();
    issue.category = object.value(QStringLiteral("category")).toString().trimmed();
    issue.severity = static_cast<IssueSeverity>(object.value(QStringLiteral("severity")).toInt(static_cast<int>(IssueSeverity::Major)));
    issue.status = static_cast<IssueStatus>(object.value(QStringLiteral("status")).toInt(static_cast<int>(IssueStatus::Open)));
    issue.point.x = static_cast<float>(object.value(QStringLiteral("x")).toDouble());
    issue.point.y = static_cast<float>(object.value(QStringLiteral("y")).toDouble());
    issue.point.z = static_cast<float>(object.value(QStringLiteral("z")).toDouble());
    issue.relatedTowerIndex = object.value(QStringLiteral("relatedTowerIndex")).toInt(-1);
    issue.relatedTowerName = object.value(QStringLiteral("relatedTowerName")).toString().trimmed();
    issue.imagePath = object.value(QStringLiteral("imagePath")).toString().trimmed();
    issue.description = object.value(QStringLiteral("description")).toString().trimmed();
    issue.createdAt = trimOrFallback(
        object.value(QStringLiteral("createdAt")).toString(),
        QDateTime::currentDateTime().toString(Qt::ISODate));
    return issue;
}

QString issueDefaultId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
