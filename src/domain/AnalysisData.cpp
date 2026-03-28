#include "domain/AnalysisData.h"

#include <QObject>

namespace
{
QJsonObject pointRecordToJson(const PointRecord& point)
{
    return QJsonObject {
        { QStringLiteral("x"), point.x },
        { QStringLiteral("y"), point.y },
        { QStringLiteral("z"), point.z },
        { QStringLiteral("r"), static_cast<int>(point.r) },
        { QStringLiteral("g"), static_cast<int>(point.g) },
        { QStringLiteral("b"), static_cast<int>(point.b) },
        { QStringLiteral("a"), static_cast<int>(point.a) },
        { QStringLiteral("intensity"), static_cast<int>(point.intensity) },
        { QStringLiteral("classification"), static_cast<int>(point.classification) },
        { QStringLiteral("returnNumber"), static_cast<int>(point.returnNumber) },
        { QStringLiteral("numberOfReturns"), static_cast<int>(point.numberOfReturns) },
        { QStringLiteral("gpsTime"), point.gpsTime },
        { QStringLiteral("hasIntensity"), point.hasIntensity },
        { QStringLiteral("hasClassification"), point.hasClassification },
        { QStringLiteral("hasReturnInfo"), point.hasReturnInfo },
        { QStringLiteral("hasGpsTime"), point.hasGpsTime }
    };
}

PointRecord pointRecordFromJson(const QJsonObject& object)
{
    PointRecord point;
    point.x = static_cast<float>(object.value(QStringLiteral("x")).toDouble());
    point.y = static_cast<float>(object.value(QStringLiteral("y")).toDouble());
    point.z = static_cast<float>(object.value(QStringLiteral("z")).toDouble());
    point.r = static_cast<std::uint8_t>(object.value(QStringLiteral("r")).toInt(255));
    point.g = static_cast<std::uint8_t>(object.value(QStringLiteral("g")).toInt(255));
    point.b = static_cast<std::uint8_t>(object.value(QStringLiteral("b")).toInt(255));
    point.a = static_cast<std::uint8_t>(object.value(QStringLiteral("a")).toInt(255));
    point.intensity = static_cast<std::uint16_t>(object.value(QStringLiteral("intensity")).toInt());
    point.classification = static_cast<std::uint8_t>(object.value(QStringLiteral("classification")).toInt());
    point.returnNumber = static_cast<std::uint8_t>(object.value(QStringLiteral("returnNumber")).toInt());
    point.numberOfReturns = static_cast<std::uint8_t>(object.value(QStringLiteral("numberOfReturns")).toInt());
    point.gpsTime = object.value(QStringLiteral("gpsTime")).toDouble();
    point.hasIntensity = object.value(QStringLiteral("hasIntensity")).toBool(false);
    point.hasClassification = object.value(QStringLiteral("hasClassification")).toBool(false);
    point.hasReturnInfo = object.value(QStringLiteral("hasReturnInfo")).toBool(false);
    point.hasGpsTime = object.value(QStringLiteral("hasGpsTime")).toBool(false);
    return point;
}
}

QString analysisSeverityDisplayName(AnalysisSeverity severity)
{
    switch (severity) {
    case AnalysisSeverity::Advisory:
        return QObject::tr("Advisory");
    case AnalysisSeverity::Warning:
        return QObject::tr("Warning");
    case AnalysisSeverity::Critical:
        return QObject::tr("Critical");
    case AnalysisSeverity::None:
    default:
        return QObject::tr("Normal");
    }
}

QJsonObject vegetationRiskRecordToJson(const VegetationRiskRecord& record)
{
    return QJsonObject {
        { QStringLiteral("id"), record.id },
        { QStringLiteral("title"), record.title },
        { QStringLiteral("severity"), static_cast<int>(record.severity) },
        { QStringLiteral("point"), pointRecordToJson(record.point) },
        { QStringLiteral("representativeChainage"), record.representativeChainage },
        { QStringLiteral("chainageStart"), record.chainageStart },
        { QStringLiteral("chainageEnd"), record.chainageEnd },
        { QStringLiteral("minimumDistance"), record.minimumDistance },
        { QStringLiteral("averageDistance"), record.averageDistance },
        { QStringLiteral("supportPointCount"), record.supportPointCount },
        { QStringLiteral("nearestSegmentIndex"), record.nearestSegmentIndex },
        { QStringLiteral("nearestTowerIndex"), record.nearestTowerIndex },
        { QStringLiteral("nearestTowerName"), record.nearestTowerName },
        { QStringLiteral("sourceRule"), record.sourceRule },
        { QStringLiteral("notes"), record.notes }
    };
}

VegetationRiskRecord vegetationRiskRecordFromJson(const QJsonObject& object)
{
    VegetationRiskRecord record;
    record.id = object.value(QStringLiteral("id")).toString();
    record.title = object.value(QStringLiteral("title")).toString();
    record.severity = static_cast<AnalysisSeverity>(object.value(QStringLiteral("severity")).toInt());
    record.point = pointRecordFromJson(object.value(QStringLiteral("point")).toObject());
    record.representativeChainage = static_cast<float>(object.value(QStringLiteral("representativeChainage")).toDouble());
    record.chainageStart = static_cast<float>(object.value(QStringLiteral("chainageStart")).toDouble());
    record.chainageEnd = static_cast<float>(object.value(QStringLiteral("chainageEnd")).toDouble());
    record.minimumDistance = static_cast<float>(object.value(QStringLiteral("minimumDistance")).toDouble());
    record.averageDistance = static_cast<float>(object.value(QStringLiteral("averageDistance")).toDouble());
    record.supportPointCount = object.value(QStringLiteral("supportPointCount")).toInt();
    record.nearestSegmentIndex = object.value(QStringLiteral("nearestSegmentIndex")).toInt(-1);
    record.nearestTowerIndex = object.value(QStringLiteral("nearestTowerIndex")).toInt(-1);
    record.nearestTowerName = object.value(QStringLiteral("nearestTowerName")).toString();
    record.sourceRule = object.value(QStringLiteral("sourceRule")).toString();
    record.notes = object.value(QStringLiteral("notes")).toString();
    return record;
}
