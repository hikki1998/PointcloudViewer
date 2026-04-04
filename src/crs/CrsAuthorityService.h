#pragma once

#include <QList>
#include <QString>

#include "crs/CrsTypes.h"
#include "crs/LASViewerCrsExport.h"

namespace lasviewer::crs
{
class LASVIEWERCRS_EXPORT CrsAuthorityService
{
public:
    static bool isAvailable();

    static CrsResolveResult resolveFromAuthority(const QString& authName, int code);
    static CrsResolveResult resolveFromAuthorityText(const QString& authorityText);
    static CrsResolveResult resolveFromWkt(const QString& wkt);
    static QList<CoordinateSystemDefinition> findByName(
        const QString& name,
        CoordinateSystemKindFilter kindFilter = CoordinateSystemKindFilter::Any,
        int limit = 20);

    static bool normalizeCoordinateSystem(
        const CoordinateSystemRef& input,
        CoordinateSystemRef* output,
        QString* errorMessage = nullptr);

    static QString authorityText(const CoordinateSystemRef& crs);
    static QString displayName(const CoordinateSystemRef& crs);
    static QString exportWkt(const CoordinateSystemRef& crs);
    static QString exportProjString(const CoordinateSystemRef& crs);
    static QString exportProjJson(const CoordinateSystemRef& crs);
};
}
