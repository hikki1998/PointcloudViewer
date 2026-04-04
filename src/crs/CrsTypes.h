#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "crs/LASViewerCrsExport.h"

namespace lasviewer::crs
{
enum class CoordinateSystemKind
{
    Geographic = 0,
    Projected
};

enum class CoordinateSystemKindFilter
{
    Any = 0,
    Geographic,
    Projected
};

enum class CrsMatchConfidence
{
    Unknown = 0,
    Heuristic,
    Alias,
    Equivalent,
    Exact
};

struct LASVIEWERCRS_EXPORT CoordinateSystemRef
{
    QString authName = QStringLiteral("EPSG");
    int code = 0;
    QString displayName;
    QString wkt;
    CoordinateSystemKind kind = CoordinateSystemKind::Projected;
    bool deprecated = false;
};

struct LASVIEWERCRS_EXPORT ProjectCoordinateSystems
{
    CoordinateSystemRef pointCloudCrs;
    CoordinateSystemRef geographicCrs;
};

struct LASVIEWERCRS_EXPORT CoordinateSystemDefinition
{
    CoordinateSystemRef reference;
    QString canonicalName;
    QString areaOfUse;
    QString scope;
    QString remarks;
    QString projString;
    QString projJson;
};

struct LASVIEWERCRS_EXPORT CrsCatalogEntry
{
    CoordinateSystemRef reference;
    QString groupName;
    QString summary;
    QStringList aliases;
};

struct LASVIEWERCRS_EXPORT CrsResolveResult
{
    bool ok = false;
    QString errorMessage;
    CoordinateSystemDefinition definition;
    QList<CoordinateSystemDefinition> candidates;
    CrsMatchConfidence confidence = CrsMatchConfidence::Unknown;
};

LASVIEWERCRS_EXPORT QString coordinateSystemKindDisplayName(CoordinateSystemKind kind);
LASVIEWERCRS_EXPORT QString crsMatchConfidenceDisplayName(CrsMatchConfidence confidence);
LASVIEWERCRS_EXPORT QString coordinateSystemCodeText(const CoordinateSystemRef& crs);
LASVIEWERCRS_EXPORT bool coordinateSystemRefMatches(
    const CoordinateSystemRef& left,
    const CoordinateSystemRef& right);
LASVIEWERCRS_EXPORT CoordinateSystemRef defaultGeographicCoordinateSystem();

LASVIEWERCRS_EXPORT QJsonObject coordinateSystemRefToJson(const CoordinateSystemRef& crs);
LASVIEWERCRS_EXPORT CoordinateSystemRef coordinateSystemRefFromJson(
    const QJsonObject& object,
    const CoordinateSystemRef& fallback = CoordinateSystemRef());
LASVIEWERCRS_EXPORT QJsonObject projectCoordinateSystemsToJson(const ProjectCoordinateSystems& coordinateSystems);
LASVIEWERCRS_EXPORT ProjectCoordinateSystems projectCoordinateSystemsFromJson(
    const QJsonObject& object,
    const ProjectCoordinateSystems& fallback = ProjectCoordinateSystems());
}
