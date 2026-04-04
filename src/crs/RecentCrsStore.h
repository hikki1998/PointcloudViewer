#pragma once

#include <QList>

#include "crs/CrsTypes.h"
#include "crs/LASViewerCrsExport.h"

namespace lasviewer::crs
{
class LASVIEWERCRS_EXPORT RecentCrsStore
{
public:
    QList<CoordinateSystemRef> load() const;
    void save(const QList<CoordinateSystemRef>& entries) const;
    void add(const CoordinateSystemRef& crs) const;

private:
    static constexpr int kMaxRecentEntries = 8;
};
}
