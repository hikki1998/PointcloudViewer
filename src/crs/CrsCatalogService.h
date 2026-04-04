#pragma once

#include <QList>

#include "crs/CrsTypes.h"
#include "crs/LASViewerCrsExport.h"
#include "crs/RecentCrsStore.h"

namespace lasviewer::crs
{
class LASVIEWERCRS_EXPORT CrsCatalogService
{
public:
    CrsCatalogService();

    QList<CrsCatalogEntry> allEntries() const;
    QList<CrsCatalogEntry> filter(
        const QString& query,
        CoordinateSystemKindFilter kindFilter = CoordinateSystemKindFilter::Any,
        bool includeDeprecated = false) const;
    QList<CrsCatalogEntry> recentEntries(
        CoordinateSystemKindFilter kindFilter = CoordinateSystemKindFilter::Any) const;
    bool findByCode(const QString& authName, int code, CrsCatalogEntry* outputEntry = nullptr) const;
    void markRecentlyUsed(const CoordinateSystemRef& crs) const;

private:
    QList<CrsCatalogEntry> entries_;
    RecentCrsStore recentStore_;
};
}
