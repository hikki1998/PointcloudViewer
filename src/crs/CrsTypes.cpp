#include "crs/CrsTypes.h"

#include <QCoreApplication>

namespace
{
QString kindToString(lasviewer::crs::CoordinateSystemKind kind)
{
    switch (kind) {
    case lasviewer::crs::CoordinateSystemKind::Geographic:
        return QStringLiteral("geographic");
    case lasviewer::crs::CoordinateSystemKind::Projected:
    default:
        return QStringLiteral("projected");
    }
}

lasviewer::crs::CoordinateSystemKind kindFromString(
    const QString& value,
    lasviewer::crs::CoordinateSystemKind fallback)
{
    if (value.compare(QStringLiteral("geographic"), Qt::CaseInsensitive) == 0) {
        return lasviewer::crs::CoordinateSystemKind::Geographic;
    }
    if (value.compare(QStringLiteral("projected"), Qt::CaseInsensitive) == 0) {
        return lasviewer::crs::CoordinateSystemKind::Projected;
    }
    return fallback;
}
}

namespace lasviewer::crs
{
QString coordinateSystemKindDisplayName(CoordinateSystemKind kind)
{
    switch (kind) {
    case CoordinateSystemKind::Geographic:
        return QCoreApplication::translate("CrsTypes", "Geographic CRS");
    case CoordinateSystemKind::Projected:
    default:
        return QCoreApplication::translate("CrsTypes", "Projected CRS");
    }
}

QString crsMatchConfidenceDisplayName(CrsMatchConfidence confidence)
{
    switch (confidence) {
    case CrsMatchConfidence::Exact:
        return QCoreApplication::translate("CrsTypes", "Exact");
    case CrsMatchConfidence::Equivalent:
        return QCoreApplication::translate("CrsTypes", "Equivalent");
    case CrsMatchConfidence::Alias:
        return QCoreApplication::translate("CrsTypes", "Alias");
    case CrsMatchConfidence::Heuristic:
        return QCoreApplication::translate("CrsTypes", "Heuristic");
    case CrsMatchConfidence::Unknown:
    default:
        return QCoreApplication::translate("CrsTypes", "Unknown");
    }
}

QString coordinateSystemCodeText(const CoordinateSystemRef& crs)
{
    if (crs.code <= 0) {
        return QCoreApplication::translate("CrsTypes", "Unset");
    }
    return QStringLiteral("%1:%2")
        .arg(crs.authName.trimmed().isEmpty() ? QStringLiteral("EPSG") : crs.authName.trimmed())
        .arg(crs.code);
}

bool coordinateSystemRefMatches(const CoordinateSystemRef& left, const CoordinateSystemRef& right)
{
    return left.code == right.code
        && left.kind == right.kind
        && left.authName.compare(right.authName, Qt::CaseInsensitive) == 0;
}

CoordinateSystemRef defaultGeographicCoordinateSystem()
{
    CoordinateSystemRef crs;
    crs.authName = QStringLiteral("EPSG");
    crs.code = 4326;
    crs.displayName = QStringLiteral("WGS 84");
    crs.kind = CoordinateSystemKind::Geographic;
    return crs;
}

QJsonObject coordinateSystemRefToJson(const CoordinateSystemRef& crs)
{
    return QJsonObject {
        { QStringLiteral("authName"), crs.authName },
        { QStringLiteral("code"), crs.code },
        { QStringLiteral("displayName"), crs.displayName },
        { QStringLiteral("wkt"), crs.wkt },
        { QStringLiteral("kind"), kindToString(crs.kind) },
        { QStringLiteral("deprecated"), crs.deprecated }
    };
}

CoordinateSystemRef coordinateSystemRefFromJson(const QJsonObject& object, const CoordinateSystemRef& fallback)
{
    CoordinateSystemRef crs = fallback;
    crs.authName = object.value(QStringLiteral("authName")).toString(crs.authName);
    crs.code = object.value(QStringLiteral("code")).toInt(crs.code);
    crs.displayName = object.value(QStringLiteral("displayName")).toString(crs.displayName);
    crs.wkt = object.value(QStringLiteral("wkt")).toString(crs.wkt);
    crs.kind = kindFromString(object.value(QStringLiteral("kind")).toString(), crs.kind);
    crs.deprecated = object.value(QStringLiteral("deprecated")).toBool(crs.deprecated);
    if (crs.authName.trimmed().isEmpty()) {
        crs.authName = QStringLiteral("EPSG");
    }
    return crs;
}

QJsonObject projectCoordinateSystemsToJson(const ProjectCoordinateSystems& coordinateSystems)
{
    return QJsonObject {
        { QStringLiteral("pointCloudCrs"), coordinateSystemRefToJson(coordinateSystems.pointCloudCrs) },
        { QStringLiteral("geographicCrs"), coordinateSystemRefToJson(coordinateSystems.geographicCrs) }
    };
}

ProjectCoordinateSystems projectCoordinateSystemsFromJson(
    const QJsonObject& object,
    const ProjectCoordinateSystems& fallback)
{
    ProjectCoordinateSystems coordinateSystems = fallback;
    coordinateSystems.pointCloudCrs = coordinateSystemRefFromJson(
        object.value(QStringLiteral("pointCloudCrs")).toObject(),
        coordinateSystems.pointCloudCrs);
    coordinateSystems.geographicCrs = coordinateSystemRefFromJson(
        object.value(QStringLiteral("geographicCrs")).toObject(),
        coordinateSystems.geographicCrs.code > 0
            ? coordinateSystems.geographicCrs
            : defaultGeographicCoordinateSystem());
    if (coordinateSystems.geographicCrs.code <= 0) {
        coordinateSystems.geographicCrs = defaultGeographicCoordinateSystem();
    }
    return coordinateSystems;
}
}
